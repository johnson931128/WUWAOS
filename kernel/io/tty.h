#ifndef WUWAOS_KERNEL_IO_TTY_H
#define WUWAOS_KERNEL_IO_TTY_H
#include <stddef.h>

typedef void (*TtyGuiOutputConsumer)(const unsigned char *bytes, size_t length);

void tty_gui_set_output_consumer(TtyGuiOutputConsumer consumer);
int tty_gui_session_active(void);
void tty_gui_session_begin(void);
void tty_gui_session_end(void);
int tty_gui_input_push(const unsigned char *bytes, size_t length);
int tty_gui_input_read_byte(void);
long tty_gui_output_write(const unsigned char *bytes, size_t length);

#endif
