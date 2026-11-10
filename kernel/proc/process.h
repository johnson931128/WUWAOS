#ifndef WUWAOS_PROCESS_H
#define WUWAOS_PROCESS_H

#include "kernel/proc/thread.h"
#include "process_limits.h"
#include "path_limits.h"
#include "kernel/io/tty.h"

typedef enum {
    PROCESS_UNUSED, PROCESS_NEW, PROCESS_LIVE, PROCESS_DYING, PROCESS_ZOMBIE
} ProcessState;

#define PROCESS_FD_LIMIT 8

typedef struct ProcessControlBlock {
    long pid;
    long parent_pid;
    char cwd[WUWA_PATH_MAX];
    char previous_cwd[WUWA_PATH_MAX];
    ProcessState state;
    Thread *thread; /* Stage A: exactly one initial thread; owned until reap. */
    long exit_status;
    uintptr_t page_table;
    long program;
    int fd_table[PROCESS_FD_LIMIT];
    TtyId terminal_session;
} Process;

Process *process_current(void);
Process *process_current_or_null(void);
Process *process_slot(int slot);
void process_init(void);
int process_runtime_started(void);
_Noreturn void process_start(void);
long process_chdir(const char *path);
long process_spawn(long program);
long process_fork(void);
long process_exec(long program, const char *argument);
long process_exec_path(const char *path);
long process_wait(long pid, uintptr_t status_address);
_Noreturn void process_exit(long status);
long process_kill(long pid);
long process_list(void);
long process_inspect(long pid);
long process_trace(long enabled);
long process_pipeline(uintptr_t result_address);
const char *process_name(const Process *process);
const char *process_state_name(ProcessState state);

#endif
