#ifndef WUWAOS_PROCESS_H
#define WUWAOS_PROCESS_H

#include "trap.h"
#include "context.h"
#include "process_limits.h"

typedef enum {
    PROCESS_UNUSED, PROCESS_NEW, PROCESS_READY, PROCESS_RUNNING,
    PROCESS_WAITING, PROCESS_ZOMBIE
} ProcessState;

typedef enum { WAIT_NONE, WAIT_CHANNEL, WAIT_CHILD, WAIT_IO } WaitReason;
typedef enum { IO_NONE, IO_CONSOLE_READ, IO_CONSOLE_RAW, IO_PIPE_READ, IO_PIPE_WRITE } IoOperation;

typedef struct ProcessControlBlock {
    long pid;
    long parent_pid;
    ProcessState state;
    /* Intrusive FIFO links; only Ready processes belong to the queue. */
    struct ProcessControlBlock *ready_next;
    int ready_queued;
    uint64_t dispatches;
    uint64_t preemptions;
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
    int input_pipe;
    int output_pipe;
    IoOperation io_operation;
    uintptr_t io_buffer;
    size_t io_length;
    size_t io_count;
    int io_overflow;
    long io_result;
} Process;

Process *process_current(void);
Process *process_slot(int slot);
void process_init(void);
_Noreturn void process_start(void);
TrapFrame *process_resume(void);
long process_spawn(long program);
long process_fork(void);
long process_exec(long program, const char *argument);
long process_yield(void);
void process_preempt(void);
long process_sleep(long channel);
long process_wakeup(long channel);
long process_wait(long pid, uintptr_t status_address);
_Noreturn void process_exit(long status);
long process_kill(long pid);
long process_list(void);
long process_inspect(long pid);
long process_trace(long enabled);
long process_pipeline(uintptr_t result_address);
long process_block_io(IoOperation operation, uintptr_t buffer, size_t length);
void process_complete_io(Process *process, long result);

#endif
