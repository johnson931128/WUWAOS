#ifndef WUWAOS_IO_H
#define WUWAOS_IO_H
#include "kernel/proc/process.h"

#define IO_GUI_CONSOLE_IN (-4)
#define IO_GUI_CONSOLE_OUT (-5)
#define IO_GUI_CONSOLE_ERR (-6)

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
long io_console_route_pid(void);
void io_poll(void);
int io_console_waiting(void);
void io_close_process(Process *process);
void io_cleanup_process_terminal(Process *process, int restore_ansi, int end_gui_session);
void io_inherit(Process *child, const Process *parent);

#endif
