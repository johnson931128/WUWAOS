#include "io.h"
#include "console.h"
#include "fs.h"
#include "syscall_numbers.h"
#include "vm.h"
#include "desktop.h"
#include <limits.h>

#define OFD_LIMIT 16
#define PIPE_LIMIT 8
#define PIPE_CAPACITY 256
#define IO_PENDING LONG_MIN
#define CONSOLE_IN (-1)
#define CONSOLE_OUT (-2)
#define CONSOLE_ERR (-3)

typedef enum { OFD_FILE, OFD_PIPE_READ, OFD_PIPE_WRITE } OfdType;
typedef struct {
    int used;
    OfdType type;
    unsigned refs;
    unsigned readable, writable, append;
    uint32_t inode;
    int pipe;
    size_t offset;
} OpenFile;
typedef struct {
    int used;
    unsigned readers, writers;
    unsigned char bytes[PIPE_CAPACITY];
    size_t read_at, write_at, count;
} Pipe;

static OpenFile open_files[OFD_LIMIT];
static Pipe pipes[PIPE_LIMIT];
static int skip_lf_uart, skip_lf_gui;
static long raw_owner_uart, raw_owner_gui;

static int fd_value(Process *process, long fd) {
    if (fd < 0 || fd >= PROCESS_FD_LIMIT) return -99;
    return process->fd_table[fd];
}

static OpenFile *get_ofd(int value) {
    return value >= 0 && value < OFD_LIMIT && open_files[value].used ? &open_files[value] : 0;
}

static int find_free_fd(Process *process, int minimum) {
    for (int i = minimum; i < PROCESS_FD_LIMIT; ++i) if (process->fd_table[i] == -99) return i;
    return -1;
}

static int allocate_ofd(OfdType type) {
    for (int i = 0; i < OFD_LIMIT; ++i) if (!open_files[i].used) {
        open_files[i] = (OpenFile){ .used = 1, .type = type, .refs = 1, .pipe = -1 };
        return i;
    }
    return -1;
}

static void ofd_release(int index) {
    OpenFile *ofd = get_ofd(index);
    if (!ofd || ofd->refs == 0) return;
    if (--ofd->refs != 0) return;
    if (ofd->type == OFD_PIPE_READ || ofd->type == OFD_PIPE_WRITE) {
        Pipe *pipe = &pipes[ofd->pipe];
        if (ofd->type == OFD_PIPE_READ && pipe->readers) --pipe->readers;
        if (ofd->type == OFD_PIPE_WRITE && pipe->writers) --pipe->writers;
        if (pipe->readers == 0 && pipe->writers == 0) pipe->used = 0;
    }
    *ofd = (OpenFile){0};
}

static void close_slot(Process *process, int fd) {
    const int value = process->fd_table[fd];
    process->fd_table[fd] = -99;
    if (value >= 0) ofd_release(value);
    if (fd == 0) {
        if (raw_owner_uart == process->pid) raw_owner_uart = 0;
        if (raw_owner_gui == process->pid) raw_owner_gui = 0;
    }
}

int io_inode_is_open(uint32_t inode) {
    for (int i = 0; i < OFD_LIMIT; ++i)
        if (open_files[i].used && open_files[i].type == OFD_FILE && open_files[i].inode == inode) return 1;
    return 0;
}

void io_inherit(Process *child, const Process *parent) {
    for (int fd = 0; fd < PROCESS_FD_LIMIT; ++fd) {
        const int value = parent->fd_table[fd];
        child->fd_table[fd] = value;
        if (value >= 0 && value < OFD_LIMIT && open_files[value].used) ++open_files[value].refs;
    }
}

void io_close_process(Process *process) {
    if (raw_owner_uart == process->pid) raw_owner_uart = 0;
    if (raw_owner_gui == process->pid) raw_owner_gui = 0;
    for (int fd = 0; fd < PROCESS_FD_LIMIT; ++fd) close_slot(process, fd);
}

static int reserve_pipe(int *read_ofd, int *write_ofd) {
    int pipe_number = -1;
    for (int i = 0; i < PIPE_LIMIT; ++i) if (!pipes[i].used) { pipe_number = i; break; }
    if (pipe_number < 0) return -1;
    const int r = allocate_ofd(OFD_PIPE_READ);
    if (r < 0) return -1;
    const int w = allocate_ofd(OFD_PIPE_WRITE);
    if (w < 0) { open_files[r] = (OpenFile){0}; return -1; }
    pipes[pipe_number] = (Pipe){ .used = 1, .readers = 1, .writers = 1 };
    open_files[r].pipe = pipe_number;
    open_files[r].readable = 1;
    open_files[w].pipe = pipe_number;
    open_files[w].writable = 1;
    *read_ofd = r; *write_ofd = w;
    return 0;
}

long io_pipe(uintptr_t descriptors) {
    Process *process = process_current();
    if (!user_writable(process->page_table, descriptors, 2 * sizeof(long))) return ERR_ADDRESS;
    const int read_fd = find_free_fd(process, 0);
    if (read_fd < 0) return ERR_NO_SLOT;
    const int write_fd = find_free_fd(process, read_fd + 1);
    if (write_fd < 0) return ERR_NO_SLOT;
    int read_ofd, write_ofd;
    if (reserve_pipe(&read_ofd, &write_ofd) < 0) return ERR_NO_SLOT;
    process->fd_table[read_fd] = read_ofd;
    process->fd_table[write_fd] = write_ofd;
    const long result[2] = { read_fd, write_fd };
    const int error = user_copy_to(process->page_table, descriptors, result, sizeof(result));
    if (error < 0) { close_slot(process, read_fd); close_slot(process, write_fd); return error; }
    return 0;
}

long io_open(const char *path, long flags) {
    Process *process = process_current();
    const int access = (int)(flags & 3);
    const long allowed = 3 | FD_O_CREAT | FD_O_TRUNC | FD_O_APPEND;
    if ((flags & ~allowed) != 0 || access < FD_O_RDONLY || access > FD_O_RDWR ||
        ((flags & FD_O_TRUNC) && access == FD_O_RDONLY) ||
        ((flags & FD_O_APPEND) && access == FD_O_RDONLY)) return ERR_INVALID;
    const int fd = find_free_fd(process, 0);
    if (fd < 0) return ERR_NO_SLOT;
    uint32_t inode;
    long result = fs_open_inode(path, !!(flags & FD_O_CREAT), &inode);
    if (result < 0) return result;
    if ((flags & FD_O_TRUNC) && io_inode_is_open(inode)) return ERR_BUSY;
    const int index = allocate_ofd(OFD_FILE);
    if (index < 0) return ERR_NO_SLOT;
    OpenFile *ofd = &open_files[index];
    ofd->inode = inode;
    ofd->readable = access != FD_O_WRONLY;
    ofd->writable = access != FD_O_RDONLY;
    ofd->append = !!(flags & FD_O_APPEND);
    if (flags & FD_O_TRUNC) {
        result = fs_truncate_inode(inode);
        if (result < 0) { ofd_release(index); return result; }
    }
    if (ofd->append) {
        result = fs_inode_size(inode);
        if (result < 0) { ofd_release(index); return result; }
        ofd->offset = (size_t)result;
    }
    process->fd_table[fd] = index;
    return fd;
}

long io_close(long fd) {
    Process *process = process_current();
    if (fd < 0 || fd >= PROCESS_FD_LIMIT || process->fd_table[fd] == -99) return ERR_BAD_FD;
    close_slot(process, (int)fd);
    return 0;
}

long io_dup2(long oldfd, long newfd) {
    Process *process = process_current();
    if (oldfd < 0 || oldfd >= PROCESS_FD_LIMIT || newfd < 0 || newfd >= PROCESS_FD_LIMIT ||
        process->fd_table[oldfd] == -99) return ERR_BAD_FD;
    if (oldfd == newfd) return newfd;
    const int value = process->fd_table[oldfd];
    if (value >= 0) {
        if (!get_ofd(value)) return ERR_BAD_FD;
        ++open_files[value].refs;
    }
    close_slot(process, (int)newfd);
    process->fd_table[newfd] = value;
    return newfd;
}

long io_dup(long oldfd) {
    Process *process = process_current();
    if (oldfd < 0 || oldfd >= PROCESS_FD_LIMIT || process->fd_table[oldfd] == -99) return ERR_BAD_FD;
    const int fd = find_free_fd(process, 0);
    return fd < 0 ? ERR_NO_SLOT : io_dup2(oldfd, fd);
}

long io_is_console(long fd) {
    const int value = fd_value(process_current(), fd);
    if (value == -99) return ERR_BAD_FD;
    return value == CONSOLE_OUT || value == CONSOLE_ERR ||
        value == IO_GUI_CONSOLE_OUT || value == IO_GUI_CONSOLE_ERR;
}

long io_console_poll(void) {
    Process *process = process_current();
    const int input = fd_value(process, 0);
    const int graphical = input == IO_GUI_CONSOLE_IN;
    if ((!graphical && input != CONSOLE_IN) ||
        (graphical ? raw_owner_gui : raw_owner_uart) != process->pid) return ERR_BUSY;
    int character;
    while ((character = graphical ? desktop_terminal_read_byte() : console_getc()) >= 0) {
        int *skip_lf = graphical ? &skip_lf_gui : &skip_lf_uart;
        if (character == '\n' && *skip_lf) { *skip_lf = 0; continue; }
        *skip_lf = 0;
        return character + 1;
    }
    return 0;
}

long io_console_mode(long raw) {
    Process *process = process_current();
    if (raw != 0 && raw != 1) return ERR_INVALID;
    const int input = fd_value(process, 0);
    long *owner;
    if (input == CONSOLE_IN) owner = &raw_owner_uart;
    else if (input == IO_GUI_CONSOLE_IN) owner = &raw_owner_gui;
    else return raw ? ERR_INVALID : 0;
    if (*owner != 0 && *owner != process->pid) return ERR_BUSY;
    *owner = raw ? process->pid : 0;
    return 0;
}

long io_console_route_pid(void) {
    Process *process = process_current();
    return fd_value(process, 0) == IO_GUI_CONSOLE_IN &&
        fd_value(process, 1) == IO_GUI_CONSOLE_OUT ? process->pid : 0;
}

static long pipe_read(Process *process, OpenFile *ofd, uintptr_t buffer, size_t length) {
    Pipe *pipe = &pipes[ofd->pipe];
    if (pipe->count == 0) return pipe->writers == 0 ? 0 : IO_PENDING;
    size_t copied = 0;
    while (copied < length && pipe->count > 0) {
        const int error = user_write_byte(process->page_table, buffer + copied, pipe->bytes[pipe->read_at]);
        if (error < 0) return copied ? (long)copied : error;
        pipe->read_at = (pipe->read_at + 1) % PIPE_CAPACITY;
        --pipe->count;
        ++copied;
    }
    return (long)copied;
}

static long pipe_write(Process *process, OpenFile *ofd, uintptr_t buffer, size_t length) {
    Pipe *pipe = &pipes[ofd->pipe];
    if (pipe->readers == 0) return ERR_BROKEN_PIPE;
    if (pipe->count == PIPE_CAPACITY) return IO_PENDING;
    size_t copied = 0;
    while (copied < length && pipe->count < PIPE_CAPACITY) {
        const int error = user_read_byte(process->page_table, buffer + copied, &pipe->bytes[pipe->write_at]);
        if (error < 0) return copied ? (long)copied : error;
        pipe->write_at = (pipe->write_at + 1) % PIPE_CAPACITY;
        ++pipe->count;
        ++copied;
    }
    return (long)copied;
}

long io_read(long fd, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    const int value = fd_value(process, fd);
    OpenFile *ofd = get_ofd(value);
    if (value == -99 || value == CONSOLE_OUT || value == CONSOLE_ERR ||
        value == IO_GUI_CONSOLE_OUT || value == IO_GUI_CONSOLE_ERR || (ofd && !ofd->readable) ||
        length > LONG_MAX) return ERR_BAD_FD;
    if (!user_writable(process->page_table, buffer, length)) return ERR_ADDRESS;
    if (length == 0) return 0;
    if (value == CONSOLE_IN || value == IO_GUI_CONSOLE_IN) {
        const long owner = value == IO_GUI_CONSOLE_IN ? raw_owner_gui : raw_owner_uart;
        if (owner != 0) {
            if (owner != process->pid) return ERR_BUSY;
            if (length > 128) return ERR_INVALID;
            process->io_fd = (int)fd;
            return process_block_io(value == IO_GUI_CONSOLE_IN ? IO_GUI_CONSOLE_RAW : IO_CONSOLE_RAW,
                                    (int)fd, buffer, length);
        }
        if (length < 2 || length > 128) return ERR_INVALID;
        process->io_fd = (int)fd;
        return process_block_io(value == IO_GUI_CONSOLE_IN ? IO_GUI_CONSOLE_READ : IO_CONSOLE_READ,
                                (int)fd, buffer, length);
    }
    if (ofd && ofd->type == OFD_PIPE_READ) {
        const long result = pipe_read(process, ofd, buffer, length);
        if (result == IO_PENDING) return process_block_io(IO_PIPE_READ, (int)fd, buffer, length);
        return result;
    }
    if (ofd && ofd->type == OFD_FILE) {
        unsigned char staging[512];
        if (length > sizeof(staging)) length = sizeof(staging);
        const long amount = fs_read_inode(ofd->inode, ofd->offset, staging, length);
        if (amount < 0) return amount;
        const int error = user_copy_to(process->page_table, buffer, staging, (size_t)amount);
        if (error < 0) return error;
        ofd->offset += (size_t)amount;
        return amount;
    }
    return ERR_INVALID;
}

long io_write(long fd, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    const int value = fd_value(process, fd);
    OpenFile *ofd = get_ofd(value);
    if (value == -99 || value == CONSOLE_IN || value == IO_GUI_CONSOLE_IN ||
        (ofd && !ofd->writable) || length > LONG_MAX) return ERR_BAD_FD;
    if (!user_readable(process->page_table, buffer, length)) return ERR_ADDRESS;
    if (length == 0) return 0;
    if (value == CONSOLE_OUT || value == CONSOLE_ERR ||
        value == IO_GUI_CONSOLE_OUT || value == IO_GUI_CONSOLE_ERR) {
        unsigned char staging[256];
        if (value == IO_GUI_CONSOLE_OUT || value == IO_GUI_CONSOLE_ERR) {
            if (length > sizeof(staging)) length = sizeof(staging);
            const int error = user_copy_from(process->page_table, staging, buffer, length);
            if (error < 0) return error;
            return desktop_terminal_write(staging, length);
        }
        unsigned char byte;
        for (size_t i = 0; i < length; ++i) {
            const int error = user_read_byte(process->page_table, buffer + i, &byte);
            if (error < 0) return i ? (long)i : error;
            console_putc((char)byte);
        }
        return (long)length;
    }
    if (ofd && ofd->type == OFD_PIPE_WRITE) {
        const long result = pipe_write(process, ofd, buffer, length);
        if (result == IO_PENDING) return process_block_io(IO_PIPE_WRITE, (int)fd, buffer, length);
        return result;
    }
    if (ofd && ofd->type == OFD_FILE) {
        unsigned char staging[512];
        if (length > sizeof(staging)) length = sizeof(staging);
        const int error = user_copy_from(process->page_table, staging, buffer, length);
        if (error < 0) return error;
        if (ofd->append) {
            const long end = fs_inode_size(ofd->inode);
            if (end < 0) return end;
            ofd->offset = (size_t)end;
        }
        if (ofd->offset > SIZE_MAX - length) return ERR_INVALID;
        const long amount = fs_write_inode(ofd->inode, ofd->offset, staging, length);
        if (amount > 0) ofd->offset += (size_t)amount;
        return amount;
    }
    return ERR_INVALID;
}

static int console_input_byte(int graphical) {
    return graphical ? desktop_terminal_read_byte() : console_getc();
}

static void console_echo(int graphical, unsigned char byte) {
    if (graphical) desktop_terminal_echo_byte(byte);
    else console_putc((char)byte);
}

static long console_read_line(Process *process, int graphical) {
    int *skip_lf = graphical ? &skip_lf_gui : &skip_lf_uart;
    int character;
    while ((character = console_input_byte(graphical)) >= 0) {
        if (character == '\n' && *skip_lf) { *skip_lf = 0; continue; }
        if (character == 4) {
            *skip_lf = 0;
            return process->io_overflow ? ERR_INVALID : (long)process->io_count;
        }
        *skip_lf = character == '\r';
        if (character == '\r' || character == '\n') {
            console_echo(graphical, '\n');
            if (process->io_overflow) return ERR_INVALID;
            const int error = user_write_byte(process->page_table, process->io_buffer + process->io_count, '\n');
            if (error < 0) return error;
            return (long)++process->io_count;
        }
        if (character == 8 || character == 127) {
            if (process->io_count > 0) {
                --process->io_count;
                console_echo(graphical, '\b'); console_echo(graphical, ' '); console_echo(graphical, '\b');
            }
        } else if (character == '\t' || (character >= 32 && character <= 126)) {
            if (process->io_count < process->io_length - 1 && !process->io_overflow) {
                const int error = user_write_byte(process->page_table, process->io_buffer + process->io_count,
                                                  (unsigned char)character);
                if (error < 0) return error;
                ++process->io_count; console_echo(graphical, (unsigned char)character);
            } else process->io_overflow = 1;
        }
    }
    return IO_PENDING;
}

static long console_read_raw(Process *process, int graphical) {
    int *skip_lf = graphical ? &skip_lf_gui : &skip_lf_uart;
    int character;
    while ((character = console_input_byte(graphical)) >= 0) {
        if (character == '\n' && *skip_lf) { *skip_lf = 0; continue; }
        *skip_lf = 0;
        const int error = user_write_byte(process->page_table, process->io_buffer, (unsigned char)character);
        return error < 0 ? error : 1;
    }
    return IO_PENDING;
}

void io_poll(void) {
    int progress;
    do {
        progress = 0;
        for (int i = 0; i < PROCESS_LIMIT; ++i) {
            Process *process = process_slot(i);
            if (process->state != PROCESS_WAITING || process->wait_reason != WAIT_IO) continue;
            const int value = fd_value(process, process->io_fd);
            OpenFile *ofd = get_ofd(value);
            long result = IO_PENDING;
            const int graphical = value == IO_GUI_CONSOLE_IN;
            const int console_reading = process->io_operation == IO_CONSOLE_READ ||
                process->io_operation == IO_CONSOLE_RAW || process->io_operation == IO_GUI_CONSOLE_READ ||
                process->io_operation == IO_GUI_CONSOLE_RAW;
            const long raw_owner = graphical ? raw_owner_gui : raw_owner_uart;
            if (console_reading && (value != CONSOLE_IN && value != IO_GUI_CONSOLE_IN)) result = ERR_BUSY;
            else if (console_reading && raw_owner != 0 && raw_owner != process->pid) result = ERR_BUSY;
            else if (process->io_operation == IO_CONSOLE_READ && graphical) result = ERR_BUSY;
            else if (process->io_operation == IO_GUI_CONSOLE_READ && !graphical) result = ERR_BUSY;
            else if (process->io_operation == IO_CONSOLE_RAW && graphical) result = ERR_BUSY;
            else if (process->io_operation == IO_GUI_CONSOLE_RAW && !graphical) result = ERR_BUSY;
            else if ((process->io_operation == IO_CONSOLE_RAW || process->io_operation == IO_GUI_CONSOLE_RAW) &&
                     raw_owner != process->pid) result = ERR_BUSY;
            else if (process->io_operation == IO_CONSOLE_READ || process->io_operation == IO_GUI_CONSOLE_READ)
                result = console_read_line(process, graphical);
            else if (process->io_operation == IO_CONSOLE_RAW || process->io_operation == IO_GUI_CONSOLE_RAW)
                result = console_read_raw(process, graphical);
            else if (process->io_operation == IO_PIPE_READ && ofd && ofd->type == OFD_PIPE_READ)
                result = pipe_read(process, ofd, process->io_buffer, process->io_length);
            else if (process->io_operation == IO_PIPE_WRITE && ofd && ofd->type == OFD_PIPE_WRITE)
                result = pipe_write(process, ofd, process->io_buffer, process->io_length);
            else result = ERR_INVALID;
            if (result != IO_PENDING) { process_complete_io(process, result); progress = 1; }
        }
    } while (progress);
}

int io_console_waiting(void) {
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        const Process *process = process_slot(i);
        if (process->state == PROCESS_WAITING &&
            (process->io_operation == IO_CONSOLE_READ || process->io_operation == IO_CONSOLE_RAW ||
             process->io_operation == IO_GUI_CONSOLE_READ || process->io_operation == IO_GUI_CONSOLE_RAW)) return 1;
    }
    return 0;
}
