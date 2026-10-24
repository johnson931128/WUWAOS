OUTPUT_ARCH(riscv)
ENTRY(_user_start)

SECTIONS
{
    . = 0x400000;
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
        *(.rodata .rodata.* .srodata .srodata.*)
    }
    ASSERT(. <= 0x404000, "User code exceeds four pages")
    . = 0x404000;
    .data : { *(.data .data.* .sdata .sdata.*) }
    .bss (NOLOAD) : {
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
    }
    ASSERT(. <= 0x405000, "User data exceeds one page")
}
