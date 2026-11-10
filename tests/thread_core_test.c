/* Exercise real TCB allocation, Ready queue, Gate, user sync and event sources.
 * Only CPU/scheduling/device boundaries are replaced for this native host test. */
#include "kernel/proc/process.h"
#include "kernel/proc/scheduler.h"
#include "kernel/proc/gate.h"
#include "kernel/ipc/sync.h"
#include "sync_ops.h"
#include "syscall_numbers.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); \
} } while (0)

static unsigned guards, notifications, waits;
static void (*on_wait)(void);
static Thread *owner, *waiter;
static KernelGate gate;
static long mutex;

void console_puts(const char *s) { (void)s; }
void console_putc(char c) { (void)c; }
void console_number(long n) { (void)n; }
_Noreturn void kernel_idle(void) { fputs("Unexpected kernel panic\n", stderr); exit(1); }
int vm_unmap_kernel_guard(uintptr_t address) {
    CHECK((address & 4095U) == 0); ++guards; return 0;
}
uint64_t timer_interrupt_count(void) { return 0; }
void irq_print_stats(void) { }
void block_print_stats(void) { }
void thread_gate_acquired(Thread *t) { if (t) ++t->internal_gate_depth; }
void thread_gate_released(Thread *t) {
    if (t) { CHECK(t->internal_gate_depth); --t->internal_gate_depth; }
}
int thread_wait_allowed(void) { return thread_current_or_null() != 0; }
void thread_event_notify(KernelEventSource *s, uint32_t generation, long result) {
    (void)s; (void)generation; (void)result; ++notifications;
}
long thread_wait_event(KernelEventSource *s, uint32_t generation, uint64_t sequence,
                       uint64_t deadline, long result) {
    (void)deadline; (void)result;
    CHECK(s->generation == generation && s->sequence == sequence && on_wait);
    ++waits; on_wait(); return 0;
}
long thread_block_sync(void) {
    Thread *t = thread_current();
    t->state = THREAD_WAITING; t->wait_reason = WAIT_SYNC;
    CHECK(on_wait); ++waits; on_wait();
    CHECK(t->state == THREAD_READY); t->state = THREAD_RUNNING;
    return t->kill_pending ? ERR_INTERRUPTED : 0;
}
void thread_wake_sync(Thread *t) {
    CHECK(t->state == THREAD_WAITING && t->wait_reason == WAIT_SYNC);
    t->wait_reason = WAIT_NONE; t->state = THREAD_READY;
}

static void release_gate(void) {
    CHECK(thread_current() == waiter);
    thread_set_current(owner);
    CHECK(kernel_gate_leave(&gate) == 0);
    thread_set_current(waiter);
}
static void release_mutex(void) {
    CHECK(thread_current() == waiter);
    thread_set_current(owner);
    CHECK(sync_call(SYNC_MUTEX_UNLOCK, mutex) == 0);
    thread_set_current(waiter);
}

static void cancel_sync_wait(void) {
    Thread *thread = thread_current();
    thread->kill_pending = 1;
    sync_thread_cancel_wait(thread);
}

int main(void) {
    Process a = { .pid = 5, .state = PROCESS_NEW };
    Process b = { .pid = 9, .state = PROCESS_NEW };
    thread_init(); CHECK(guards == THREAD_LIMIT);
    owner = a.thread = thread_allocate(&a);
    waiter = b.thread = thread_allocate(&b);
    CHECK(owner && waiter && owner != waiter && owner->tid != waiter->tid);
    CHECK(owner->process == &a && waiter->process == &b);
    CHECK(thread_allocate(&a) == 0); /* Stage A enforces one initial TCB. */
    CHECK(owner->kernel_stack_top != waiter->kernel_stack_top);
    uintptr_t bottom, top;
    CHECK(thread_stack_guard_bounds(owner->kernel_stack_top - 16384U - 4096U, &bottom, &top));
    CHECK(top == owner->kernel_stack_top && top - bottom == 16384U);
    CHECK(thread_current_or_null() == 0);
    puts("PASS: separate PCB/TCB identity, guarded stacks and single-thread allocation");

    scheduler_init(); owner->state = waiter->state = THREAD_READY;
    scheduler_enqueue(owner); scheduler_enqueue(waiter); scheduler_enqueue(owner);
    CHECK(scheduler_ready_count() == 2);
    CHECK(scheduler_next() == owner && !owner->ready_queued);
    scheduler_enqueue(owner);
    CHECK(scheduler_next() == waiter && scheduler_next() == owner);
    CHECK(!scheduler_next() && !scheduler_ready_count());
    scheduler_enqueue(owner); scheduler_enqueue(waiter); scheduler_remove(owner);
    CHECK(scheduler_next() == waiter && !scheduler_ready_count());
    scheduler_enqueue(owner); scheduler_remove(owner);
    CHECK(!scheduler_next() && !owner->ready_queued);
    puts("PASS: TCB Ready queue FIFO, duplicate enqueue, rotation and removal");

    /* Synthetic same-PCB threads exercise identity even before user create exists. */
    waiter->process = &a;
    owner->state = waiter->state = THREAD_RUNNING;
    thread_set_current(owner);
    CHECK(kernel_gate_enter(&gate) == 0 && kernel_gate_enter(&gate) == 0);
    CHECK(gate.owner == owner && gate.owner_tid == owner->tid && gate.depth == 2);
    thread_set_current(waiter);
    CHECK(kernel_gate_try_enter(&gate) == ERR_BUSY);
    CHECK(kernel_gate_leave(&gate) == ERR_INVALID);
    thread_set_current(owner); CHECK(kernel_gate_leave(&gate) == 0);
    thread_set_current(waiter); on_wait = release_gate;
    CHECK(kernel_gate_enter(&gate) == 0 && gate.owner == waiter);
    CHECK(kernel_gate_leave(&gate) == 0);
    CHECK(!owner->internal_gate_depth && !waiter->internal_gate_depth);
    thread_set_current(owner);
    CHECK(kernel_gate_enter(&gate) == 0);
    long held_tid = owner->tid;
    ++owner->tid; /* A stale pointer alone must not authorize ownership. */
    CHECK(kernel_gate_leave(&gate) == ERR_INVALID);
    owner->tid = held_tid;
    CHECK(kernel_gate_leave(&gate) == 0);
    thread_set_current(0);
    CHECK(kernel_gate_enter(&gate) == 0); /* Distinct no-thread boot owner. */
    thread_set_current(owner); CHECK(kernel_gate_try_enter(&gate) == ERR_BUSY);
    thread_set_current(0); CHECK(kernel_gate_leave(&gate) == 0);
    puts("PASS: Gate recursion and blocking distinguish TIDs sharing one PCB");

    thread_set_current(owner);
    mutex = sync_call(SYNC_MUTEX_CREATE, 0); CHECK(mutex > 0);
    CHECK(sync_call(SYNC_MUTEX_LOCK, mutex) == 0);
    thread_set_current(waiter); CHECK(sync_call(SYNC_MUTEX_UNLOCK, mutex) < 0);
    on_wait = release_mutex;
    CHECK(sync_call(SYNC_MUTEX_LOCK, mutex) == 0);
    CHECK(sync_call(SYNC_MUTEX_UNLOCK, mutex) == 0);
    CHECK(sync_call(SYNC_DESTROY, mutex) == 0);
    CHECK(waits == 2 && notifications > 0);
    long sem = sync_call(SYNC_SEM_CREATE, 0);
    CHECK(sem > 0);
    on_wait = cancel_sync_wait;
    CHECK(sync_call(SYNC_SEM_WAIT, sem) == ERR_INTERRUPTED);
    CHECK(!waiter->sync_wait_handle && !waiter->sync_next);
    waiter->kill_pending = 0;
    CHECK(sync_call(SYNC_DESTROY, sem) == 0);
    mutex = sync_call(SYNC_MUTEX_CREATE, 0);
    thread_set_current(owner);
    CHECK(sync_call(SYNC_MUTEX_LOCK, mutex) == 0);
    sync_thread_cleanup(owner);
    thread_set_current(waiter);
    CHECK(sync_call(SYNC_MUTEX_LOCK, mutex) == SYNC_ERR_OWNER_DIED);
    CHECK(sync_call(SYNC_DESTROY, mutex) == 0);
    puts("PASS: user mutex owner and wait queue use TCBs, not shared PID");

    thread_set_current(0); waiter->process = &b;
    long old_tid = owner->tid;
    owner->state = waiter->state = THREAD_ZOMBIE;
    thread_release(owner); a.thread = 0;
    thread_release(waiter); b.thread = 0;
    Thread *reused = a.thread = thread_allocate(&a);
    CHECK(reused == owner && reused->tid > old_tid && !reused->event_wait.active);
    thread_release(reused); a.thread = 0;
    puts("PASS: safe TCB reuse assigns a new TID and clears execution state");
    return 0;
}
