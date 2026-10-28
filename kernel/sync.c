#include "sync.h"
#include "sync_ops.h"
#include "syscall_numbers.h"
#include <limits.h>

#define SYNC_LIMIT 8

typedef enum { SYNC_FREE, SYNC_MUTEX, SYNC_SEMAPHORE } SyncKind;
typedef struct {
    SyncKind kind;
    long handle;
    long value;              /* semaphore permits */
    Process *owner;          /* mutex owner */
    Process *head, *tail;    /* FIFO wait queue */
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

static void enqueue(SyncObject *object, Process *process) {
    process->sync_next = 0;
    process->sync_wait_handle = object->handle;
    if (object->tail) object->tail->sync_next = process;
    else object->head = process;
    object->tail = process;
}

static void remove_waiter(SyncObject *object, Process *process) {
    Process *previous = 0;
    for (Process *entry = object->head; entry; entry = entry->sync_next) {
        if (entry == process) {
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
    Process *next = object->head;
    if (!next) return;
    remove_waiter(object, next);
    process_wake_sync(next);
}

/* Entered only in a non-preemptible, single-hart kernel. */
static long acquire(SyncObject *object) {
    Process *current = process_current();
    for (;;) {
        if (object->poisoned) { current->sync_wait_handle = 0; return SYNC_ERR_OWNER_DIED; }
        if (object->kind == SYNC_MUTEX) {
            if (object->owner == current) { current->sync_wait_handle = 0; return -35; }
            if (!object->owner) { object->owner = current; current->sync_wait_handle = 0; return 0; }
        } else if (object->value > 0) {
            --object->value; current->sync_wait_handle = 0; return 0;
        }
        enqueue(object, current);
        process_block_sync();
        /* Recheck after wakeup: another runnable process may acquire first. */
    }
}

long sync_call(long operation, long argument) {
    if (operation == SYNC_MUTEX_CREATE) return create(SYNC_MUTEX, 0);
    if (operation == SYNC_SEM_CREATE) return create(SYNC_SEMAPHORE, argument);
    SyncObject *object = lookup(argument);
    if (!object) return ERR_INVALID;
    if (operation == SYNC_DESTROY) {
        /* Woken callers have left the queue but still hold a reference. */
        for (int i = 0; i < PROCESS_LIMIT; ++i) {
            Process *p = process_slot(i);
            if (p->state != PROCESS_UNUSED && p->state != PROCESS_ZOMBIE && p->sync_wait_handle == argument)
                return ERR_BUSY;
        }
        if (object->owner || object->head) return ERR_BUSY;
        *object = (SyncObject){0};
        return 0;
    }
    if (operation == SYNC_MUTEX_LOCK && object->kind == SYNC_MUTEX) return acquire(object);
    if (operation == SYNC_SEM_WAIT && object->kind == SYNC_SEMAPHORE) return acquire(object);
    if (operation == SYNC_MUTEX_UNLOCK && object->kind == SYNC_MUTEX) {
        if (object->owner != process_current()) return -1;
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

void sync_process_cleanup(Process *process) {
    SyncObject *waiting = lookup(process->sync_wait_handle);
    if (waiting) {
        remove_waiter(waiting, process);
        /* A woken process can die before consuming the available resource. */
        if (waiting->poisoned || (waiting->kind == SYNC_MUTEX && !waiting->owner) ||
            (waiting->kind == SYNC_SEMAPHORE && waiting->value > 0)) wake_one(waiting);
    }
    process->sync_wait_handle = 0;
    process->sync_next = 0;
    for (int i = 0; i < SYNC_LIMIT; ++i) {
        SyncObject *object = &objects[i];
        if (object->kind == SYNC_MUTEX && object->owner == process) {
            object->owner = 0;
            object->poisoned = 1;
            while (object->head) wake_one(object);
        }
    }
}
