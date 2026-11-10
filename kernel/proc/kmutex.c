#include "kernel/proc/kmutex.h"
#include "kernel/io/console.h"
#include "arch/riscv/irq.h"

/* NULL owner means a no-process context only when locked is set. */
static KernelMutex *no_process_held;

static _Noreturn void misuse(const KernelMutex *mutex, const char *reason) {
    console_puts("[kmutex] "); console_puts(mutex->name);
    console_puts(": "); console_puts(reason); console_putc('\n');
    kernel_idle();
}

static Process *caller(const KernelMutex *mutex) {
    uintptr_t status;
#ifdef WUWAOS_KMUTEX_TEST
    /* Host tests replace only CPU status; queue/ownership code stays identical. */
    extern uintptr_t kmutex_test_sstatus;
    status = kmutex_test_sstatus;
#else
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
#endif
    if ((status & 2U) || irq_in_handler()) misuse(mutex, "requires SIE=0 outside IRQ");
    Process *process = process_current_or_null();
    if (process && process->state != PROCESS_RUNNING) misuse(mutex, "caller is not Running");
    return process;
}

static KernelMutex **held_stack(Process *process) {
    return process ? &process->kernel_locks : &no_process_held;
}

static void check_order(KernelMutex *mutex, Process *process) {
    const KernelMutex *held = *held_stack(process);
    if (held && mutex->rank <= held->rank) misuse(mutex, "recursive lock or rank inversion");
}

static void assign_owner(KernelMutex *mutex, Process *process) {
    mutex->locked = 1;
    mutex->owner = process;
    mutex->owner_previous = *held_stack(process);
    *held_stack(process) = mutex;
    ++mutex->acquisitions;
}

int kmutex_trylock(KernelMutex *mutex) {
    Process *process = caller(mutex);
    check_order(mutex, process);
    if (mutex->locked) return 0;
    assign_owner(mutex, process);
    return 1;
}

void kmutex_lock(KernelMutex *mutex) {
    Process *process = caller(mutex);
    check_order(mutex, process);
    if (!mutex->locked) {
        assign_owner(mutex, process);
        return;
    }
    if (!process) misuse(mutex, "no-process context cannot wait");
    if (process->kernel_waiting || process->sync_wait_handle)
        misuse(mutex, "caller is already waiting");
    /*
     * SIE=0 serializes check -> enqueue -> PROCESS_WAITING -> switch on one
     * hart. The unlocker cannot run in this interval, so wakeup cannot be lost.
     * Separate PCB links leave the user mutex/semaphore queues untouched.
     */
    process->kernel_waiting = mutex;
    process->kernel_wait_next = 0;
    if (mutex->tail) mutex->tail->kernel_wait_next = process;
    else mutex->head = process;
    mutex->tail = process;
    ++mutex->waiters;
    ++mutex->contentions;
    process_block_sync();
    /* Unlock transfers ownership before waking; no newcomer may steal it. */
    if (mutex->owner != process || !mutex->locked || process->kernel_waiting ||
        process->kernel_locks != mutex) misuse(mutex, "wake without ownership");
}

void kmutex_unlock(KernelMutex *mutex) {
    Process *process = caller(mutex);
    if (!mutex->locked || mutex->owner != process || *held_stack(process) != mutex)
        misuse(mutex, "unlock requires owner and reverse lock order");
    *held_stack(process) = mutex->owner_previous;
    mutex->owner_previous = 0;
    mutex->owner = 0;
    mutex->locked = 0;
    Process *next = mutex->head;
    if (!next) return;
    mutex->head = next->kernel_wait_next;
    if (!mutex->head) mutex->tail = 0;
    --mutex->waiters;
    next->kernel_wait_next = 0;
    next->kernel_waiting = 0;
    assign_owner(mutex, next);
    process_wake_sync(next);
}

int kmutex_process_busy(const Process *process) {
    /* A handed-off owner is protected even before its kernel stack resumes. */
    return process->kernel_locks != 0;
}

void kmutex_process_cleanup(Process *process) {
    if (process->kernel_locks) misuse(process->kernel_locks, "terminating a kernel lock owner");
    KernelMutex *mutex = process->kernel_waiting;
    if (!mutex) return;
    Process *previous = 0;
    for (Process *entry = mutex->head; entry; entry = entry->kernel_wait_next) {
        if (entry == process) {
            if (previous) previous->kernel_wait_next = entry->kernel_wait_next;
            else mutex->head = entry->kernel_wait_next;
            if (mutex->tail == entry) mutex->tail = previous;
            --mutex->waiters;
            process->kernel_wait_next = 0;
            process->kernel_waiting = 0;
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
    else if (!mutex->owner) console_puts("no-process");
    else console_number(mutex->owner->pid);
    console_puts(" waiters="); console_number(mutex->waiters);
    console_puts(" acquired="); console_number((long)mutex->acquisitions);
    console_puts(" contended="); console_number((long)mutex->contentions); console_putc('\n');
}
