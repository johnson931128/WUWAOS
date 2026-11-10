#include "kernel/ipc/sync.h"
#include "sync_ops.h"
#include "syscall_numbers.h"
#include <limits.h>

#define SYNC_LIMIT 8

typedef enum { SYNC_FREE, SYNC_MUTEX, SYNC_SEMAPHORE } SyncKind;
typedef struct {
    SyncKind kind;
    long handle;
    long value;              /* semaphore permits */
    Thread *owner;          /* mutex owner */
    Thread *head, *tail;    /* FIFO wait queue */
    int poisoned;
} SyncObject;

static SyncObject objects[SYNC_LIMIT];
static long next_handle = 1;

static SyncObject *lookup(long handle) {
    for (int i = 0; i < SYNC_LIMIT; ++i)
        if (objects[i].kind != SYNC_FREE && objects[i].handle == handle) return &objects[i];
    return 0;
}

static long create(SyncKind kind, long initial) {
    if (initial < 0 || next_handle == LONG_MAX) return ERR_INVALID;
    for (int i = 0; i < SYNC_LIMIT; ++i) {
        if (objects[i].kind == SYNC_FREE) {
            objects[i] = (SyncObject){ .kind = kind, .handle = next_handle++, .value = initial };
            return objects[i].handle;
        }
    }
    return ERR_NO_SLOT;
}

static void enqueue(SyncObject *object, Thread *thread) {
    thread->sync_next = 0;
    thread->sync_wait_handle = object->handle;
    if (object->tail) object->tail->sync_next = thread;
    else object->head = thread;
    object->tail = thread;
}

static void remove_waiter(SyncObject *object, Thread *thread) {
    Thread *previous = 0;
    for (Thread *entry = object->head; entry; entry = entry->sync_next) {
        if (entry == thread) {
            if (previous) previous->sync_next = entry->sync_next;
            else object->head = entry->sync_next;
            if (object->tail == entry) object->tail = previous;
            entry->sync_next = 0;
            return;
        }
        previous = entry;
    }
}

static void wake_one(SyncObject *object) {
    Thread *next = object->head;
    if (!next) return;
    remove_waiter(object, next);
    thread_wake_sync(next);
}

/* Entered only in a non-preemptible, single-hart kernel. */
static long acquire(SyncObject *object) {
    Thread *current = thread_current();
    for (;;) {
        if (object->poisoned) { current->sync_wait_handle = 0; return SYNC_ERR_OWNER_DIED; }
        if (object->kind == SYNC_MUTEX) {
            if (object->owner == current) { current->sync_wait_handle = 0; return -35; }
            if (!object->owner) { object->owner = current; current->sync_wait_handle = 0; return 0; }
        } else if (object->value > 0) {
            --object->value; current->sync_wait_handle = 0; return 0;
        }
        enqueue(object, current);
        const long wait_result = thread_block_sync();
        if (wait_result < 0) {
            current->sync_wait_handle = 0;
            return wait_result;
        }
        /* Recheck after wakeup: another runnable thread may acquire first. */
    }
}

long sync_call(long operation, long argument) {
    if (operation == SYNC_MUTEX_CREATE) return create(SYNC_MUTEX, 0);
    if (operation == SYNC_SEM_CREATE) return create(SYNC_SEMAPHORE, argument);
    SyncObject *object = lookup(argument);
    if (!object) return ERR_INVALID;
    if (operation == SYNC_DESTROY) {
        /* Woken callers have left the queue but still hold a reference. */
        for (int i = 0; i < THREAD_LIMIT; ++i) {
            Thread *p = thread_slot(i);
            if (p->state != THREAD_UNUSED && p->state != THREAD_ZOMBIE && p->sync_wait_handle == argument)
                return ERR_BUSY;
        }
        if (object->owner || object->head) return ERR_BUSY;
        *object = (SyncObject){0};
        return 0;
    }
    if (operation == SYNC_MUTEX_LOCK && object->kind == SYNC_MUTEX) return acquire(object);
    if (operation == SYNC_SEM_WAIT && object->kind == SYNC_SEMAPHORE) return acquire(object);
    if (operation == SYNC_MUTEX_UNLOCK && object->kind == SYNC_MUTEX) {
        if (object->owner != thread_current()) return -1;
        object->owner = 0;
        wake_one(object);
        return 0;
    }
    if (operation == SYNC_SEM_POST && object->kind == SYNC_SEMAPHORE) {
        if (object->value == LONG_MAX) return -75;
        ++object->value;
        wake_one(object);
        return 0;
    }
    return ERR_INVALID;
}

void sync_thread_cleanup(Thread *thread) {
    SyncObject *waiting = lookup(thread->sync_wait_handle);
    if (waiting) {
        remove_waiter(waiting, thread);
        /* A woken thread can die before consuming the available resource. */
        if (waiting->poisoned || (waiting->kind == SYNC_MUTEX && !waiting->owner) ||
            (waiting->kind == SYNC_SEMAPHORE && waiting->value > 0)) wake_one(waiting);
    }
    thread->sync_wait_handle = 0;
    thread->sync_next = 0;
    for (int i = 0; i < SYNC_LIMIT; ++i) {
        SyncObject *object = &objects[i];
        if (object->kind == SYNC_MUTEX && object->owner == thread) {
            object->owner = 0;
            object->poisoned = 1;
            while (object->head) wake_one(object);
        }
    }
}

void sync_thread_cancel_wait(Thread *thread) {
    SyncObject *waiting = lookup(thread->sync_wait_handle);
    if (waiting) {
        remove_waiter(waiting, thread);
        if (waiting->poisoned || (waiting->kind == SYNC_MUTEX && !waiting->owner) ||
            (waiting->kind == SYNC_SEMAPHORE && waiting->value > 0)) wake_one(waiting);
    }
    thread->sync_wait_handle = 0;
    thread->sync_next = 0;
    thread_wake_sync(thread);
}
