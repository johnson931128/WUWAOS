OUTPUT_ARCH(riscv)
ENTRY(_user_start)

SECTIONS
{
    . = 0x400000;
    .text : { KEEP(*(.text.entry)) *(.text .text.*) }
    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) }
    . = 0x409000;
    .data : { *(.data .data.* .sdata .sdata.*) }
    .bss (NOLOAD) : { *(.bss .bss.* .sbss .sbss.*) *(COMMON) }
}
