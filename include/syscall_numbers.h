#ifndef WUWAOS_SYSCALL_NUMBERS_H
#define WUWAOS_SYSCALL_NUMBERS_H

/* WUWAOS ABI, intentionally independent of Linux syscall numbers. */
#define SYS_WRITE 1
#define SYS_GETPID 2
#define SYS_EXIT 3
#define ERR_INVALID (-22L)
#define ERR_ADDRESS (-14L)
#define ERR_NO_SYSCALL (-38L)

#endif
