#ifndef WUWAOS_CONSOLE_H
#define WUWAOS_CONSOLE_H
#include "kernel/io/tty.h"

void console_putc(char character);
void console_puts(const char *text);
void console_hex(unsigned long value);
void console_number(long value);
int console_getc(void);
void console_set_route_tty(TtyId id, long owner_pid);
_Noreturn void kernel_idle(void);

#endif
