    .section .text.entry
    .globl _start
_start:
    csrw sie, zero
    csrci sstatus, 2
    .option push
    .option norelax
    la sp, __boot_stack_top
    la t0, __boot_bss_start
    la t1, __boot_bss_end
    .option pop
1:
    bgeu t0, t1, 2f
    sb zero, 0(t0)
    addi t0, t0, 1
    j 1b
2:
    call boot_main
3:
    wfi
    j 3b
