#include "drivers/uart/uart.h"

#define UART_BASE 0x10000000UL
#define UART_LINE_STATUS 5
#define UART_TRANSMIT_READY (1U << 5)

void uart_putc_raw(char character) {
    volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;
    while ((uart[UART_LINE_STATUS] & UART_TRANSMIT_READY) == 0) { }
    uart[0] = (unsigned char)character;
}

int uart_getc_raw(void) {
    volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;
    return (uart[UART_LINE_STATUS] & 1U) ? uart[0] : -1;
}
