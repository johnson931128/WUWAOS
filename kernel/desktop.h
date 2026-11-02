#ifndef WUWAOS_DESKTOP_H
#define WUWAOS_DESKTOP_H
#include <stddef.h>

void desktop_draw(void);
void desktop_start_cursor(void);
void desktop_poll(void);
int desktop_event_source_active(void);
void desktop_terminal_set_session(int active);
int desktop_terminal_session_active(void);
int desktop_terminal_visible(void);
int desktop_terminal_read_byte(void);
long desktop_terminal_write(const unsigned char *bytes, size_t length);
void desktop_terminal_echo_byte(unsigned char byte);

#endif
