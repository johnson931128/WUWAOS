OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    /* First 2 MiB of QEMU's 128 MiB RAM are reserved for OpenSBI. */
    RAM (rwx) : ORIGIN = 0x80200000, LENGTH = 126M
}

SECTIONS
{
    .text : ALIGN(4096) {
        KEEP(*(.text.entry))
        *(.text .text.*)
        . = ALIGN(4096);
        __text_end = .;
    } > RAM

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4096);
        __rodata_end = .;
    } > RAM

    .data : ALIGN(16) {
        *(.data .data.* .sdata .sdata.*)
    } > RAM

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    .stack (NOLOAD) : ALIGN(16) {
        . += 16K;
        __stack_top = .;
    } > RAM
    . = ALIGN(4096);
    __kernel_end = .;
    ASSERT(__kernel_end <= 0x80400000, "Kernel exceeds reserved low-RAM boot budget")
}
