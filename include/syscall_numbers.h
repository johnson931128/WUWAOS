#ifndef WUWAOS_SYSCALL_NUMBERS_H
#define WUWAOS_SYSCALL_NUMBERS_H

/* WUWAOS ABI, intentionally independent of Linux syscall numbers. */
#define SYS_WRITE 1
#define SYS_GETPID 2
#define SYS_EXIT 3
#define SYS_SPAWN 4
#define SYS_YIELD 5
#define SYS_SLEEP 6
#define SYS_WAKEUP 7
#define SYS_WAIT 8
#define SYS_PS 9
#define SYS_PCB 10
#define SYS_KILL 11
#define SYS_READ 12
#define SYS_PIPELINE 13
#define SYS_TRACE 14
#define SYS_FORK 15
#define SYS_EXEC 16
#define SYS_FS_LIST 17
#define SYS_FS_MKDIR 18
#define SYS_FS_TOUCH 19
#define SYS_FS_READ 20
#define SYS_FS_WRITE 21
#define SYS_CONSOLE_MODE 22
#define SYS_SBRK 23
#define SYS_VMSTAT 24
#define SYS_WORKSTATE 25
#define SYS_SLEEP_MS 26
#define SYS_CONSOLE_POLL 27
#define SYS_FS_REMOVE 28
#define SYS_FS_COPY 29
#define SYS_CHDIR 30
#define SYS_GETCWD 31
#define SYS_FS_REMOVE_TREE 32
#define SYS_SYNC 33
#define SYS_EXEC_PATH 34
#define SYS_OPEN 35
#define SYS_CLOSE 36
#define SYS_DUP2 37
#define SYS_PIPE 38
#define SYS_DUP 39
#define SYS_IS_CONSOLE 40
#define SYS_FS_REPLACE 41
#define SYS_THREAD_CREATE 42
#define SYS_THREAD_JOIN 43
#define SYS_THREAD_EXIT 44
#define SYS_GETTID 45

#define FD_O_RDONLY 0x01
#define FD_O_WRONLY 0x02
#define FD_O_RDWR 0x03
#define FD_O_CREAT 0x10
#define FD_O_TRUNC 0x20
#define FD_O_APPEND 0x40

#define PROGRAM_WORKER 1
#define PROGRAM_QUICK 2
#define PROGRAM_WRITER 3
#define PROGRAM_READER 4
#define PROGRAM_COUNTER 5
#define PROGRAM_FAN 6
#define PROGRAM_ELF 7
#define PROGRAM_WORKSTATE 8

#define ERR_NO_CHILD (-10L)
#define ERR_NO_SLOT (-11L)
#define ERR_NO_MEMORY (-12L)
#define ERR_IO (-5L)
#define ERR_BUSY (-16L)
#define ERR_INVALID (-22L)
#define ERR_ADDRESS (-14L)
#define ERR_NO_SYSCALL (-38L)
#define ERR_BROKEN_PIPE (-32L)
#define ERR_BAD_FD (-9L)
#define ERR_INTERRUPTED (-4L)
#define ERR_TIMED_OUT (-110L)
#define ERR_UNCERTAIN (-200L)

#endif
