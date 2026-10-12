#ifndef WUWAOS_PROCESS_H
#define WUWAOS_PROCESS_H

#include "trap.h"

typedef enum { PROCESS_READY, PROCESS_RUNNING, PROCESS_EXITED } ProcessState;

typedef struct {
    long pid;
    ProcessState state;
    long exit_status;
    uintptr_t page_table;
    uintptr_t kernel_stack_top;
    uintptr_t user_stack_top;
    TrapFrame *frame;
} Process;

extern Process current_process;
void process_init(uintptr_t page_table);
_Noreturn void process_start(void);
_Noreturn void process_exit(long status);

#endif
