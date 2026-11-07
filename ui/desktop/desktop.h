#ifndef WUWAOS_DESKTOP_H
#define WUWAOS_DESKTOP_H
#include <stddef.h>
#include <stdint.h>

void desktop_draw(void);
void desktop_start_cursor(void);
void desktop_poll(void);
int desktop_event_source_active(void);
int desktop_terminal_visible(void);
void desktop_terminal_output(const unsigned char *bytes, size_t length);

#endif
