#ifndef WUWAOS_CONSOLE_H
#define WUWAOS_CONSOLE_H

void console_putc(char character);
void console_puts(const char *text);
void console_hex(unsigned long value);
_Noreturn void kernel_idle(void);

#endif
