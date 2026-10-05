/* QEMU virt's NS16550 UART. This milestone uses a fixed platform layout. */
#define UART_BASE 0x10000000UL
#define UART_LINE_STATUS 5
#define UART_TRANSMIT_READY (1U << 5)

static void uart_putc(char character) {
    volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;

    while ((uart[UART_LINE_STATUS] & UART_TRANSMIT_READY) == 0) {
    }
    uart[0] = (unsigned char)character;
}

static void uart_puts(const char *text) {
    while (*text != '\0') {
        if (*text == '\n') {
            uart_putc('\r');
        }
        uart_putc(*text++);
    }
}

void kernel_main(void) {
    uart_puts("WUWAOS booting...\n");

    /* No scheduler or interrupts yet. Keep the booted kernel idle. */
    for (;;) {
        __asm__ volatile("wfi");
    }
}
