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
    ASSERT(. <= 0x401000, "User code exceeds one page")
    . = 0x401000;
    .data : { *(.data .data.* .sdata .sdata.*) }
    .bss (NOLOAD) : {
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
    }
    ASSERT(. <= 0x402000, "User data exceeds one page")
}
