#ifndef WUWAOS_KERNEL_IO_DIAGNOSTIC_H
#define WUWAOS_KERNEL_IO_DIAGNOSTIC_H

#include "drivers/uart/uart.h"
#include <stdint.h>

static inline void kernel_diagnostic_text(const char *text) {
    if (!text) return;
    while (*text) uart_putc_raw(*text++);
}

static inline void kernel_diagnostic_unsigned(unsigned long value) {
    char digits[3 * sizeof(unsigned long)];
    unsigned int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10UL);
        value /= 10UL;
    } while (value && count < sizeof(digits));
    while (count) uart_putc_raw(digits[--count]);
}

static inline void kernel_diagnostic_signed(long value) {
    if (value < 0) {
        uart_putc_raw('-');
        kernel_diagnostic_unsigned(0UL - (unsigned long)value);
    } else {
        kernel_diagnostic_unsigned((unsigned long)value);
    }
}

static inline void kernel_diagnostic_hex(uintptr_t value) {
    static const char digits[] = "0123456789abcdef";
    for (int shift = (int)(sizeof(value) * 8U) - 4; shift >= 0; shift -= 4)
        uart_putc_raw(digits[(value >> shift) & 0xfU]);
}

static inline void kernel_diagnostic(const char *subsystem, const char *operation,
                                     long error, int uncertain, uintptr_t detail) {
    kernel_diagnostic_text("[diag] subsystem=");
    kernel_diagnostic_text(subsystem);
    kernel_diagnostic_text(" operation=");
    kernel_diagnostic_text(operation);
    kernel_diagnostic_text(" error=");
    kernel_diagnostic_signed(error);
    kernel_diagnostic_text(" uncertain=");
    uart_putc_raw(uncertain ? '1' : '0');
    kernel_diagnostic_text(" detail=0x");
    kernel_diagnostic_hex(detail);
    uart_putc_raw('\n');
}

#endif
