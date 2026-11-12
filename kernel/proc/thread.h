#ifndef WUWAOS_THREAD_H
#define WUWAOS_THREAD_H

#include "arch/riscv/trap.h"
#include "arch/riscv/context.h"
#include "kernel/proc/event.h"
#include "kernel/io/tty.h"
#include "thread_limits.h"

struct ProcessControlBlock;

typedef enum {
    THREAD_UNUSED, THREAD_NEW, THREAD_READY, THREAD_RUNNING,
    THREAD_WAITING, THREAD_ZOMBIE
} ThreadState;
typedef enum { WAIT_NONE, WAIT_CHANNEL, WAIT_CHILD, WAIT_SYNC, WAIT_EVENT, WAIT_JOIN } WaitReason;

typedef struct ThreadControlBlock {
    long tid;
    struct ProcessControlBlock *process;
    ThreadState state;
    struct ThreadControlBlock *ready_next;
    int ready_queued;
    uint64_t dispatches, preemptions, cpu_ticks, run_started;
    struct ThreadControlBlock *sync_next;
    long sync_wait_handle;
    /* Separate links for the retained standalone mutex prototype. */
    struct KernelMutex *kernel_locks, *kernel_waiting;
    struct ThreadControlBlock *kernel_wait_next;
    uintptr_t kernel_stack_top, user_stack_top;
    int user_stack_slot; /* -1 for the original process stack. */
    TrapFrame *frame;
    KernelContext context;
    WaitReason wait_reason;
    long channel, wait_pid;
    uintptr_t wait_status;
    KernelEventWait event_wait;
    unsigned kernel_call_active : 1;
    unsigned kill_pending : 1;
    unsigned join_claimed : 1;
    long joiner_tid;
    long joining_tid;
    long exit_status;
    unsigned cancel_defer_depth, internal_gate_depth;
    TtyId console_route_tty;
} Thread;

/* TCB storage and lifecycle; user create/join/exit ABI is implemented below. */
void thread_init(void);
Thread *thread_allocate(struct ProcessControlBlock *process);
Thread *thread_allocate_extra(struct ProcessControlBlock *process, int stack_slot);
void thread_release(Thread *thread);
Thread *thread_slot(int slot);
Thread *thread_current(void);
Thread *thread_current_or_null(void);
void thread_set_current(Thread *thread);
int thread_stack_guard_bounds(uintptr_t address, uintptr_t *bottom, uintptr_t *top);
int thread_kernel_stack_bounds(uintptr_t address, uintptr_t *bottom, uintptr_t *top, int *scheduler);
long thread_yield(void);
void thread_preempt(void);
long thread_sleep(long channel);
long thread_sleep_ms(long milliseconds);
long thread_wakeup(long channel);
TrapFrame *thread_resume(void);
const char *thread_state_name(ThreadState state);
long thread_block_sync(void);
void thread_wake_sync(Thread *thread);
long thread_wait_event(KernelEventSource *source, uint32_t generation, uint64_t sequence,
                       uint64_t deadline, long timeout_result);
void thread_event_notify(KernelEventSource *source, uint32_t generation, long result);
void thread_event_poll_timeouts(void);
void thread_event_cancel(Thread *thread, long result);
void thread_kernel_call_enter(void);
void thread_kernel_call_return(void);
int thread_wait_allowed(void);
int thread_runtime_nonblocking(void);
void thread_gate_acquired(Thread *thread);
void thread_gate_released(Thread *thread);
int thread_cancel_defer_enter(void);
int thread_cancel_defer_leave(void);
long thread_create(uintptr_t entry, uintptr_t argument, uintptr_t trampoline);
long thread_join(long tid, uintptr_t status_address);
_Noreturn void thread_exit(long status);
long thread_gettid(void);
int thread_process_slot_count(struct ProcessControlBlock *process);

#endif
