#include "kernel/io/console.h"
#include "kernel/io/tty.h"
#include "kernel/proc/process.h"
#include "drivers/uart/uart.h"

void console_set_route_tty(TtyId id, long owner_pid) {
    Thread *thread = thread_current_or_null();
    if (!thread || (owner_pid != 0 && thread->process->pid != owner_pid)) return;
    thread->console_route_tty = id;
}

void console_putc(char character) {
    Thread *thread = thread_current_or_null();
    if (thread && thread->console_route_tty != TTY_ID_INVALID) {
        const unsigned char byte = (unsigned char)character;
        (void)tty_output_write(thread->console_route_tty, &byte, 1);
        return;
    }
    if (character == '\n') {
        console_putc('\r');
    }
    uart_putc_raw(character);
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

void console_number(long value) {
    char digits[20];
    unsigned long number = (unsigned long)value;
    if (value < 0) {
        console_putc('-');
        number = 0UL - number;
    }
    int length = 0;
    do {
        digits[length++] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    while (length > 0) {
        console_putc(digits[--length]);
    }
}

int console_getc(void) {
    return uart_getc_raw();
}

_Noreturn void kernel_idle(void) {
    for (;;) {
        __asm__ volatile("wfi");
    }
}
