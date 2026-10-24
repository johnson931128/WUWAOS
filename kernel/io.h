#ifndef WUWAOS_IO_H
#define WUWAOS_IO_H
#include "process.h"

long io_read(long fd, uintptr_t buffer, size_t length);
long io_write(long fd, uintptr_t buffer, size_t length);
void io_poll(void);
int io_console_waiting(void);
void io_close_process(Process *process);
void io_inherit(Process *child, const Process *parent);
int io_pipe_create(void);
void io_pipe_release(int pipe);

#endif
