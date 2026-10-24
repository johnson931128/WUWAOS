#ifndef WUWAOS_PRINT_H
#define WUWAOS_PRINT_H

void user_puts(const char *text);
void user_number(long value);
int user_write_all(long fd, const char *text, unsigned long length);

#endif
