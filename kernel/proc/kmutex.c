#include "kernel/proc/kmutex.h"
#include "kernel/io/console.h"
#include "arch/riscv/irq.h"

/* NULL owner means a no-thread context only when locked is set. */
static KernelMutex *no_process_held;

static _Noreturn void misuse(const KernelMutex *mutex, const char *reason) {
    console_puts("[kmutex] "); console_puts(mutex->name);
    console_puts(": "); console_puts(reason); console_putc('\n');
    kernel_idle();
}

static Thread *caller(const KernelMutex *mutex) {
    uintptr_t status;
#ifdef WUWAOS_KMUTEX_TEST
    /* Host tests replace only CPU status; queue/ownership code stays identical. */
    extern uintptr_t kmutex_test_sstatus;
    status = kmutex_test_sstatus;
#else
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
#endif
    if ((status & 2U) || irq_in_handler()) misuse(mutex, "requires SIE=0 outside IRQ");
    Thread *thread = thread_current_or_null();
    if (thread && thread->state != THREAD_RUNNING) misuse(mutex, "caller is not Running");
    return thread;
}

static KernelMutex **held_stack(Thread *thread) {
    return thread ? &thread->kernel_locks : &no_process_held;
}

static void check_order(KernelMutex *mutex, Thread *thread) {
    const KernelMutex *held = *held_stack(thread);
    if (held && mutex->rank <= held->rank) misuse(mutex, "recursive lock or rank inversion");
}

static void assign_owner(KernelMutex *mutex, Thread *thread) {
    mutex->locked = 1;
    mutex->owner = thread;
    mutex->owner_previous = *held_stack(thread);
    *held_stack(thread) = mutex;
    ++mutex->acquisitions;
}

int kmutex_trylock(KernelMutex *mutex) {
    Thread *thread = caller(mutex);
    check_order(mutex, thread);
    if (mutex->locked) return 0;
    assign_owner(mutex, thread);
    return 1;
}

void kmutex_lock(KernelMutex *mutex) {
    Thread *thread = caller(mutex);
    check_order(mutex, thread);
    if (!mutex->locked) {
        assign_owner(mutex, thread);
        return;
    }
    if (!thread) misuse(mutex, "no-thread context cannot wait");
    if (thread->kernel_waiting || thread->sync_wait_handle)
        misuse(mutex, "caller is already waiting");
    /*
     * SIE=0 serializes check -> enqueue -> THREAD_WAITING -> switch on one
     * hart. The unlocker cannot run in this interval, so wakeup cannot be lost.
     * Separate PCB links leave the user mutex/semaphore queues untouched.
     */
    thread->kernel_waiting = mutex;
    thread->kernel_wait_next = 0;
    if (mutex->tail) mutex->tail->kernel_wait_next = thread;
    else mutex->head = thread;
    mutex->tail = thread;
    ++mutex->waiters;
    ++mutex->contentions;
    thread_block_sync();
    /* Unlock transfers ownership before waking; no newcomer may steal it. */
    if (mutex->owner != thread || !mutex->locked || thread->kernel_waiting ||
        thread->kernel_locks != mutex) misuse(mutex, "wake without ownership");
}

void kmutex_unlock(KernelMutex *mutex) {
    Thread *thread = caller(mutex);
    if (!mutex->locked || mutex->owner != thread || *held_stack(thread) != mutex)
        misuse(mutex, "unlock requires owner and reverse lock order");
    *held_stack(thread) = mutex->owner_previous;
    mutex->owner_previous = 0;
    mutex->owner = 0;
    mutex->locked = 0;
    Thread *next = mutex->head;
    if (!next) return;
    mutex->head = next->kernel_wait_next;
    if (!mutex->head) mutex->tail = 0;
    --mutex->waiters;
    next->kernel_wait_next = 0;
    next->kernel_waiting = 0;
    assign_owner(mutex, next);
    thread_wake_sync(next);
}

int kmutex_thread_busy(const Thread *thread) {
    /* A handed-off owner is protected even before its kernel stack resumes. */
    return thread->kernel_locks != 0;
}

void kmutex_thread_cleanup(Thread *thread) {
    if (thread->kernel_locks) misuse(thread->kernel_locks, "terminating a kernel lock owner");
    KernelMutex *mutex = thread->kernel_waiting;
    if (!mutex) return;
    Thread *previous = 0;
    for (Thread *entry = mutex->head; entry; entry = entry->kernel_wait_next) {
        if (entry == thread) {
            if (previous) previous->kernel_wait_next = entry->kernel_wait_next;
            else mutex->head = entry->kernel_wait_next;
            if (mutex->tail == entry) mutex->tail = previous;
            --mutex->waiters;
            thread->kernel_wait_next = 0;
            thread->kernel_waiting = 0;
            return;
        }
        previous = entry;
    }
    misuse(mutex, "waiter missing from queue");
}

void kmutex_print(const KernelMutex *mutex) {
    console_puts("Kernel mutex "); console_puts(mutex->name);
    console_puts(" owner=");
    if (!mutex->locked) console_puts("none");
    else if (!mutex->owner) console_puts("no-thread");
    else console_number(mutex->owner->tid);
    console_puts(" waiters="); console_number(mutex->waiters);
    console_puts(" acquired="); console_number((long)mutex->acquisitions);
    console_puts(" contended="); console_number((long)mutex->contentions); console_putc('\n');
}
