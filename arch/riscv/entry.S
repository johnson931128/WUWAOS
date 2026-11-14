#include "arch/riscv/hart.h"

    .section .text.entry
    .globl _start
_start:
    # OpenSBI enters in S-mode; timer interrupts stay masked until initialization.
    li t0, HART_LIMIT
    bgeu a0, t0, 3f
    mv tp, a0
    csrw sie, zero
    csrci sstatus, 2

    .option push
    .option norelax
    la sp, __stack_top
    la t0, __bss_start
    la t1, __bss_end
    .option pop

1:
    bgeu t0, t1, 2f
    sb zero, 0(t0)
    addi t0, t0, 1
    j 1b
2:
    call kernel_main
3:
    wfi
    j 3b

    .globl hart_secondary_entry
    .balign 4
hart_secondary_entry:
    csrw sie, zero
    csrci sstatus, 2
    mv tp, a0
    la sp, hart_boot_stacks
    addi t0, a0, 1
    li t1, HART_BOOT_STACK
    mul t0, t0, t1
    add sp, sp, t0
    tail hart_secondary_main
