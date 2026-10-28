#include "api.h"
#include "syscall_numbers.h"

static long syscall3(long number, long first, long second, long third) {
    register long a0 __asm__("a0") = first;
    register long a1 __asm__("a1") = second;
    register long a2 __asm__("a2") = third;
    register long a7 __asm__("a7") = number;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

long write(long fd, const char *buffer, unsigned long length) {
    return syscall3(SYS_WRITE, fd, (long)buffer, (long)length);
}

long getpid(void) {
    return syscall3(SYS_GETPID, 0, 0, 0);
}

long spawn(long program) {
    return syscall3(SYS_SPAWN, program, 0, 0);
}

long yield(void) {
    return syscall3(SYS_YIELD, 0, 0, 0);
}

long sleep(long channel) {
    return syscall3(SYS_SLEEP, channel, 0, 0);
}

long wakeup(long channel) {
    return syscall3(SYS_WAKEUP, channel, 0, 0);
}

long wait(long pid, long *status) {
    return syscall3(SYS_WAIT, pid, (long)status, 0);
}

long read(long fd, char *buffer, unsigned long length) {
    return syscall3(SYS_READ, fd, (long)buffer, (long)length);
}

long ps(void) { return syscall3(SYS_PS, 0, 0, 0); }
long pcb(long pid) { return syscall3(SYS_PCB, pid, 0, 0); }
long kill(long pid) { return syscall3(SYS_KILL, pid, 0, 0); }
long pipeline(long pids[2]) { return syscall3(SYS_PIPELINE, (long)pids, 0, 0); }
long trace(long enabled) { return syscall3(SYS_TRACE, enabled, 0, 0); }
long fork(void) { return syscall3(SYS_FORK, 0, 0, 0); }
long exec(long program) { return syscall3(SYS_EXEC, program, 0, 0); }
long exec_arg(long program, const char *argument) {
    return syscall3(SYS_EXEC, program, (long)argument, 0);
}
long console_mode(long raw) { return syscall3(SYS_CONSOLE_MODE, raw, 0, 0); }
long sbrk(long increment) { return syscall3(SYS_SBRK, increment, 0, 0); }
long vmstat(long frame_limit) { return syscall3(SYS_VMSTAT, frame_limit, 0, 0); }
long workstate_snapshot(WorkstateSnapshot *snapshot) {
    return syscall3(SYS_WORKSTATE, (long)snapshot, sizeof(*snapshot), 0);
}
long sleep_ms(long milliseconds) { return syscall3(SYS_SLEEP_MS, milliseconds, 0, 0); }
long console_poll(void) { return syscall3(SYS_CONSOLE_POLL, 0, 0, 0); }
long chdir(const char *path) { return syscall3(SYS_CHDIR, (long)path, 0, 0); }
long getcwd(char *buffer, unsigned long capacity) { return syscall3(SYS_GETCWD, (long)buffer, (long)capacity, 0); }
long fs_list(const char *path, char *buffer, unsigned long capacity) {
    return syscall3(SYS_FS_LIST, (long)path, (long)buffer, (long)capacity);
}
long fs_mkdir(const char *path) { return syscall3(SYS_FS_MKDIR, (long)path, 0, 0); }
long fs_touch(const char *path) { return syscall3(SYS_FS_TOUCH, (long)path, 0, 0); }
long fs_remove(const char *path) { return syscall3(SYS_FS_REMOVE, (long)path, 0, 0); }
long fs_remove_tree(const char *path) { return syscall3(SYS_FS_REMOVE_TREE, (long)path, 0, 0); }
long fs_copy(const char *source, const char *destination) {
    return syscall3(SYS_FS_COPY, (long)source, (long)destination, 0);
}
long fs_readfile(const char *path, char *buffer, unsigned long capacity) {
    return syscall3(SYS_FS_READ, (long)path, (long)buffer, (long)capacity);
}
long fs_writefile(const char *path, const char *buffer, unsigned long length) {
    return syscall3(SYS_FS_WRITE, (long)path, (long)buffer, (long)length);
}

_Noreturn void exit(long status) {
    syscall3(SYS_EXIT, status, 0, 0);
    for (;;) {
    }
}
