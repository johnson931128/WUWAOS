#ifndef WUWAOS_UART_H
#define WUWAOS_UART_H

#define UART_MMIO_BASE 0x10000000UL
void uart_enable_rx_irq(void);
void uart_putc_raw(char character);
int uart_getc_raw(void);

#endif
