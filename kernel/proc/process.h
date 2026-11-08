#ifndef WUWAOS_PROCESS_H
#define WUWAOS_PROCESS_H

#include "arch/riscv/trap.h"
#include "arch/riscv/context.h"
#include "process_limits.h"
#include "path_limits.h"
#include "kernel/io/tty.h"
#include "kernel/proc/event.h"

typedef enum {
    PROCESS_UNUSED, PROCESS_NEW, PROCESS_READY, PROCESS_RUNNING,
    PROCESS_WAITING, PROCESS_ZOMBIE
} ProcessState;

typedef enum { WAIT_NONE, WAIT_CHANNEL, WAIT_CHILD, WAIT_SYNC, WAIT_EVENT } WaitReason;

#define PROCESS_FD_LIMIT 8

typedef struct ProcessControlBlock {
    long pid;
    long parent_pid;
    char cwd[WUWA_PATH_MAX];
    char previous_cwd[WUWA_PATH_MAX];
    ProcessState state;
    /* Intrusive FIFO links; only Ready processes belong to the queue. */
    struct ProcessControlBlock *ready_next;
    int ready_queued;
    uint64_t dispatches;
    uint64_t preemptions;
    uint64_t cpu_ticks, run_started;
    struct ProcessControlBlock *sync_next;
    long sync_wait_handle;
    long exit_status;
    uintptr_t page_table;
    uintptr_t kernel_stack_top;
    uintptr_t user_stack_top;
    TrapFrame *frame;
    KernelContext context;
    WaitReason wait_reason;
    long channel;
    long wait_pid;
    uintptr_t wait_status;
    long program;
    int fd_table[PROCESS_FD_LIMIT];
    KernelEventWait event_wait;
    unsigned kernel_call_active : 1;
    unsigned kill_pending : 1;
    unsigned cancel_defer_depth;
    unsigned internal_gate_depth;
    TtyId console_route_tty;
    TtyId terminal_session;
} Process;

Process *process_current(void);
Process *process_current_or_null(void);
Process *process_slot(int slot);
int process_kernel_stack_bounds(uintptr_t address, uintptr_t *bottom, uintptr_t *top,
                                int *scheduler);
void process_init(void);
int process_runtime_started(void);
_Noreturn void process_start(void);
TrapFrame *process_resume(void);
long process_chdir(const char *path);
long process_spawn(long program);
long process_fork(void);
long process_exec(long program, const char *argument);
long process_exec_path(const char *path);
long process_yield(void);
void process_preempt(void);
long process_sleep(long channel);
long process_sleep_ms(long milliseconds);
long process_wakeup(long channel);
long process_wait(long pid, uintptr_t status_address);
_Noreturn void process_exit(long status);
long process_kill(long pid);
long process_list(void);
long process_inspect(long pid);
long process_trace(long enabled);
long process_pipeline(uintptr_t result_address);
long process_block_sync(void);
void process_wake_sync(Process *process);
long process_wait_event(KernelEventSource *source, uint32_t generation, uint64_t sequence,
                        uint64_t deadline, long timeout_result);
void process_event_notify(KernelEventSource *source, uint32_t generation, long result);
void process_event_poll_timeouts(void);
void process_event_cancel(Process *process, long result);
void process_kernel_call_enter(void);
void process_kernel_call_return(void);
int process_wait_allowed(void);
int process_runtime_nonblocking(void);
void process_gate_acquired(Process *process);
void process_gate_released(Process *process);
int process_cancel_defer_enter(void);
int process_cancel_defer_leave(void);
const char *process_name(const Process *process);
const char *process_state_name(ProcessState state);

#endif
