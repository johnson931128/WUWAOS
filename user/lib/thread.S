#include "syscall_numbers.h"
    .section .text
    .align 2
    .globl __thread_trampoline
__thread_trampoline:
    # Kernel enters with a0=entry and a1=argument.
    mv t0, a0
    mv a0, a1
    jalr ra, t0, 0
    # The entry's return value is the Thread exit status.
    li a7, SYS_THREAD_EXIT
    ecall
1:
    j 1b
