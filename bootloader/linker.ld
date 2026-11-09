OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY
{
    FLASH (rx) : ORIGIN = 0x20000000, LENGTH = 32M
    RAM (rw) : ORIGIN = 0x81000000, LENGTH = 2M
}
SECTIONS
{
    .text : ALIGN(16) { KEEP(*(.text.entry)) *(.text .text.*) } > FLASH
    .rodata : ALIGN(16) { *(.rodata .rodata.* .srodata .srodata.*) } > FLASH
    .data : ALIGN(16) { *(.data .data.* .sdata .sdata.*) } > RAM AT> FLASH
    ASSERT(SIZEOF(.data) == 0, "Bootloader entry does not initialize writable data")
    .bss (NOLOAD) : ALIGN(16) {
        __boot_bss_start = .;
        *(.bss .bss.* .sbss .sbss.*) *(COMMON)
        . = ALIGN(16);
        __boot_stack_bottom = .;
        . += 32K;
        __boot_stack_top = .;
        __boot_bss_end = .;
    } > RAM
    ASSERT(__boot_bss_end <= 0x81200000, "Bootloader working RAM exceeds reservation")
    ASSERT(__boot_bss_end <= 0x81010000, "Bootloader state overlaps the ELF staging buffer")
}
