#ifndef WUWAOS_IO_H
#define WUWAOS_IO_H
#include "process.h"

long io_read(long fd, uintptr_t buffer, size_t length);
long io_write(long fd, uintptr_t buffer, size_t length);
long io_open(const char *path, long flags);
long io_close(long fd);
long io_dup2(long oldfd, long newfd);
long io_dup(long oldfd);
long io_is_console(long fd);
long io_pipe(uintptr_t descriptors);
int io_inode_is_open(uint32_t inode);
long io_console_mode(long raw);
long io_console_poll(void);
void io_poll(void);
int io_console_waiting(void);
void io_close_process(Process *process);
void io_inherit(Process *child, const Process *parent);

#endif
