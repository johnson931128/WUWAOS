#include "console.h"

#define UART_BASE 0x10000000UL
#define UART_LINE_STATUS 5
#define UART_TRANSMIT_READY (1U << 5)

void console_putc(char character) {
    volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;
    if (character == '\n') {
        console_putc('\r');
    }
    while ((uart[UART_LINE_STATUS] & UART_TRANSMIT_READY) == 0) {
    }
    uart[0] = (unsigned char)character;
}

void console_puts(const char *text) {
    while (*text != '\0') {
        console_putc(*text++);
    }
}

void console_hex(unsigned long value) {
    static const char digits[] = "0123456789abcdef";
    console_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        console_putc(digits[(value >> shift) & 15]);
    }
}

_Noreturn void kernel_idle(void) {
    for (;;) {
        __asm__ volatile("wfi");
    }
}
