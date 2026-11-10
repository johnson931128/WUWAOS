#include "kernel/io/io.h"
#include "kernel/io/console.h"
#include "kernel/io/tty.h"
#include "kernel/fs/fs.h"
#include "kernel/proc/gate.h"
#include "syscall_numbers.h"
#include "kernel/mm/vm.h"
#include <limits.h>

#define OFD_LIMIT 16
#define PIPE_LIMIT 8
#define PIPE_CAPACITY 256
#define IO_PENDING LONG_MIN
#define CONSOLE_IN (-1)
#define CONSOLE_OUT (-2)
#define CONSOLE_ERR (-3)

typedef enum { OFD_RESERVED, OFD_FILE, OFD_PIPE_READ, OFD_PIPE_WRITE, OFD_TTY } OfdType;
typedef struct {
    int used;
    OfdType type;
    unsigned refs;
    unsigned readable, writable, append;
    uint32_t inode;
    int pipe;
    TtyId tty;
    size_t offset;
} OpenFile;
typedef struct {
    int used;
    uint32_t generation;
    unsigned readers, writers;
    unsigned char bytes[PIPE_CAPACITY];
    size_t read_at, write_at, count;
    KernelEventSource event;
} Pipe;

static OpenFile open_files[OFD_LIMIT];
static Pipe pipes[PIPE_LIMIT];
static KernelGate ofd_gates[OFD_LIMIT];
static KernelGate pipe_gates[PIPE_LIMIT];

static int fd_value(Process *process, long fd) {
    if (fd < 0 || fd >= PROCESS_FD_LIMIT) return -99;
    return process->fd_table[fd];
}

static OpenFile *get_ofd(int value) {
    return value >= 0 && value < OFD_LIMIT && open_files[value].used ? &open_files[value] : 0;
}

static TtyId fd_tty_id(int value) {
    if (value == CONSOLE_IN || value == CONSOLE_OUT || value == CONSOLE_ERR) return tty_uart_id();
    OpenFile *ofd = get_ofd(value);
    return ofd && ofd->type == OFD_TTY ? ofd->tty : TTY_ID_INVALID;
}

static int fd_tty_readable(int value) {
    if (value == CONSOLE_IN) return 1;
    OpenFile *ofd = get_ofd(value);
    return ofd && ofd->type == OFD_TTY && ofd->readable;
}

static int fd_tty_writable(int value) {
    if (value == CONSOLE_OUT || value == CONSOLE_ERR) return 1;
    OpenFile *ofd = get_ofd(value);
    return ofd && ofd->type == OFD_TTY && ofd->writable;
}

static int find_free_fd(Process *process, int minimum) {
    for (int i = minimum; i < PROCESS_FD_LIMIT; ++i) if (process->fd_table[i] == -99) return i;
    return -1;
}

static int allocate_ofd(OfdType type) {
    for (int i = 0; i < OFD_LIMIT; ++i) if (!open_files[i].used) {
        if (kernel_gate_init(&ofd_gates[i]) < 0) continue;
        open_files[i] = (OpenFile){ .used = 1, .type = type, .refs = 1, .pipe = -1 };
        return i;
    }
    return -1;
}

static int allocate_tty_ofd(TtyId id, int readable, int writable) {
    if (tty_retain(id) < 0) return -1;
    const int index = allocate_ofd(OFD_TTY);
    if (index < 0) { tty_release(id); return -1; }
    open_files[index].tty = id;
    open_files[index].readable = readable != 0;
    open_files[index].writable = writable != 0;
    return index;
}

static void ofd_release(int index) {
    OpenFile *ofd = get_ofd(index);
    if (!ofd || ofd->refs == 0) return;
    if (--ofd->refs != 0) return;
    if (ofd->type == OFD_PIPE_READ || ofd->type == OFD_PIPE_WRITE) {
        Pipe *pipe = &pipes[ofd->pipe];
        if (ofd->type == OFD_PIPE_READ && pipe->readers) --pipe->readers;
        if (ofd->type == OFD_PIPE_WRITE && pipe->writers) --pipe->writers;
        if (pipe->readers == 0 && pipe->writers == 0) {
            const uint32_t generation = pipe->generation;
            event_source_close(&pipe->event, ERR_BROKEN_PIPE);
            const KernelEventSource event = pipe->event;
            *pipe = (Pipe){ .generation = generation, .event = event };
        } else event_source_notify(&pipe->event, 0);
    } else if (ofd->type == OFD_TTY) tty_release(ofd->tty);
    *ofd = (OpenFile){0};
}

static void close_slot(Process *process, int fd) {
    const int value = process->fd_table[fd];
    process->fd_table[fd] = -99;
    if (value >= 0) ofd_release(value);
    if (fd == 0) tty_release_raw_owner(process->thread->tid);
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
    tty_release_raw_owner(process->thread->tid);
    for (int fd = 0; fd < PROCESS_FD_LIMIT; ++fd) close_slot(process, fd);
}

long io_attach_tty(Process *process, TtyId id) {
    const int input = allocate_tty_ofd(id, 1, 0);
    if (input < 0) return ERR_NO_SLOT;
    const int output = allocate_tty_ofd(id, 0, 1);
    if (output < 0) { ofd_release(input); return ERR_NO_SLOT; }
    for (int fd = 0; fd < 3; ++fd) close_slot(process, fd);
    process->fd_table[0] = input;
    process->fd_table[1] = output;
    process->fd_table[2] = output;
    ++open_files[output].refs;
    return 0;
}

void io_cleanup_process_terminal(Process *process, int restore_ansi, TtyId end_session) {
    const TtyId terminal = fd_tty_id(process->fd_table[1]);
    if (restore_ansi) {
        static const unsigned char restore[] = "\033[0m\033[?25h\033[?1049l";
        if (terminal) (void)tty_output_write(terminal, restore, sizeof(restore) - 1U);
        else console_puts((const char *)restore);
    }
    if (end_session) (void)tty_session_close(end_session);
    io_close_process(process);
}

static int reserve_pipe(int *read_ofd, int *write_ofd) {
    int pipe_number = -1;
    for (int i = 0; i < PIPE_LIMIT; ++i)
        if (!pipes[i].used && !pipes[i].event.retired && pipes[i].generation != UINT32_MAX) {
            pipe_number = i;
            break;
        }
    if (pipe_number < 0) return -1;
    if (kernel_gate_init(&pipe_gates[pipe_number]) < 0) return -1;
    const int r = allocate_ofd(OFD_PIPE_READ);
    if (r < 0) return -1;
    const int w = allocate_ofd(OFD_PIPE_WRITE);
    if (w < 0) { open_files[r] = (OpenFile){0}; return -1; }
    const uint32_t generation = pipes[pipe_number].generation + 1U;
    const KernelEventSource previous_event = pipes[pipe_number].event;
    pipes[pipe_number] = (Pipe){
        .used = 1, .generation = generation, .readers = 1, .writers = 1,
        .event = previous_event
    };
    if (event_source_init(&pipes[pipe_number].event, generation) < 0) {
        pipes[pipe_number] = (Pipe){ .generation = generation, .event = previous_event };
        open_files[w] = (OpenFile){0};
        open_files[r] = (OpenFile){0};
        return -1;
    }
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
    const int index = allocate_ofd(OFD_RESERVED);
    if (index < 0) return ERR_NO_SLOT;
    int scope_error = fs_scope_enter();
    if (scope_error < 0) { ofd_release(index); return scope_error; }
    uint32_t inode;
    long result = fs_open_inode(path, !!(flags & FD_O_CREAT), &inode);
    if (result < 0) goto open_failed;
    if ((flags & FD_O_TRUNC) && io_inode_is_open(inode)) { result = ERR_BUSY; goto open_failed; }
    OpenFile *ofd = &open_files[index];
    ofd->inode = inode;
    ofd->readable = access != FD_O_WRONLY;
    ofd->writable = access != FD_O_RDONLY;
    ofd->append = !!(flags & FD_O_APPEND);
    if (flags & FD_O_TRUNC) {
        result = fs_truncate_inode(inode);
        if (result < 0) goto open_failed;
    }
    if (ofd->append) {
        result = fs_inode_size(inode);
        if (result < 0) goto open_failed;
        ofd->offset = (size_t)result;
    }
    ofd->type = OFD_FILE;
    process->fd_table[fd] = index;
    scope_error = fs_scope_leave();
    if (scope_error < 0) return scope_error;
    return fd;

open_failed:
    (void)fs_scope_leave();
    ofd_release(index);
    return result;
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
    return fd_tty_writable(value);
}

long io_console_poll(void) {
    Process *process = process_current();
    const int input = fd_value(process, 0);
    const TtyId id = fd_tty_id(input);
    if (!fd_tty_readable(input) || !tty_session_active(id)) return ERR_BAD_FD;
    if (tty_raw_owner(id) != process->pid) return ERR_BUSY;
    int character;
    while ((character = tty_input_read_byte(id)) >= 0) {
        if (tty_filter_crlf(id, character, 1)) continue;
        return character + 1;
    }
    return 0;
}

long io_console_mode(long raw) {
    Process *process = process_current();
    if (raw != 0 && raw != 1) return ERR_INVALID;
    const int input = fd_value(process, 0);
    if (!fd_tty_readable(input)) return raw ? ERR_INVALID : 0;
    return tty_set_raw_owner(fd_tty_id(input), process->thread->tid, (int)raw);
}

TtyId io_console_route_tty(void) {
    Process *process = process_current();
    const int output = fd_value(process, 1);
    return fd_tty_writable(output) ? fd_tty_id(output) : TTY_ID_INVALID;
}

static long pipe_read(Process *process, Pipe *pipe, uintptr_t buffer, size_t length) {
    if (pipe->count == 0) return pipe->writers == 0 ? 0 : IO_PENDING;
    unsigned char staging[PIPE_CAPACITY];
    size_t amount = length < pipe->count ? length : pipe->count;
    if (amount > sizeof(staging)) amount = sizeof(staging);
    for (size_t i = 0; i < amount; ++i)
        staging[i] = pipe->bytes[(pipe->read_at + i) % PIPE_CAPACITY];
    const int error = user_copy_to(process->page_table, buffer, staging, amount);
    if (error < 0) return error;
    pipe->read_at = (pipe->read_at + amount) % PIPE_CAPACITY;
    pipe->count -= amount;
    event_source_notify(&pipe->event, 0);
    return (long)amount;
}

static long pipe_write(Process *process, Pipe *pipe, uintptr_t buffer, size_t length) {
    if (pipe->readers == 0) return ERR_BROKEN_PIPE;
    if (pipe->count == PIPE_CAPACITY) return IO_PENDING;
    unsigned char staging[PIPE_CAPACITY];
    size_t amount = PIPE_CAPACITY - pipe->count;
    if (amount > length) amount = length;
    const int error = user_copy_from(process->page_table, staging, buffer, amount);
    if (error < 0) return error;
    if (pipe->readers == 0) return ERR_BROKEN_PIPE;
    for (size_t i = 0; i < amount; ++i)
        pipe->bytes[(pipe->write_at + i) % PIPE_CAPACITY] = staging[i];
    pipe->write_at = (pipe->write_at + amount) % PIPE_CAPACITY;
    pipe->count += amount;
    event_source_notify(&pipe->event, 0);
    return (long)amount;
}

static long pipe_read_wait(Process *process, int fd, uintptr_t buffer, size_t length) {
    for (;;) {
        const int value = fd_value(process, fd);
        OpenFile *ofd = get_ofd(value);
        if (!ofd || ofd->type != OFD_PIPE_READ || ofd->pipe < 0 || ofd->pipe >= PIPE_LIMIT)
            return ERR_BAD_FD;
        Pipe *pipe = &pipes[ofd->pipe];
        uint32_t generation;
        uint64_t sequence;
        if (!pipe->used || event_source_snapshot(&pipe->event, &generation, &sequence) < 0)
            return ERR_IO;
        const int gate_error = kernel_gate_enter(&pipe_gates[ofd->pipe]);
        if (gate_error < 0) return gate_error;
        if (!pipe->used || pipe->generation != generation) {
            (void)kernel_gate_leave(&pipe_gates[ofd->pipe]);
            return ERR_IO;
        }
        const long result = pipe_read(process, pipe, buffer, length);
        const int leave_error = kernel_gate_leave(&pipe_gates[ofd->pipe]);
        if (leave_error < 0) return leave_error;
        if (result != IO_PENDING) return result;
        const long wait = thread_wait_event(&pipe->event, generation, sequence, 0, ERR_TIMED_OUT);
        if (wait < 0) return wait;
    }
}

static long pipe_write_wait(Process *process, int fd, uintptr_t buffer, size_t length) {
    for (;;) {
        const int value = fd_value(process, fd);
        OpenFile *ofd = get_ofd(value);
        if (!ofd || ofd->type != OFD_PIPE_WRITE || ofd->pipe < 0 || ofd->pipe >= PIPE_LIMIT)
            return ERR_BAD_FD;
        Pipe *pipe = &pipes[ofd->pipe];
        uint32_t generation;
        uint64_t sequence;
        if (!pipe->used || event_source_snapshot(&pipe->event, &generation, &sequence) < 0)
            return ERR_BROKEN_PIPE;
        const int gate_error = kernel_gate_enter(&pipe_gates[ofd->pipe]);
        if (gate_error < 0) return gate_error;
        if (!pipe->used || pipe->generation != generation) {
            (void)kernel_gate_leave(&pipe_gates[ofd->pipe]);
            return ERR_BROKEN_PIPE;
        }
        const long result = pipe_write(process, pipe, buffer, length);
        const int leave_error = kernel_gate_leave(&pipe_gates[ofd->pipe]);
        if (leave_error < 0) return leave_error;
        if (result != IO_PENDING) return result;
        const long wait = thread_wait_event(&pipe->event, generation, sequence, 0, ERR_TIMED_OUT);
        if (wait < 0) return wait;
    }
}

static long tty_wait_byte(Process *process, TtyId id, int raw, int *character) {
    for (;;) {
        if (!tty_session_active(id)) return ERR_IO;
        const long owner = tty_raw_owner(id);
        if (owner < 0) return owner;
        if ((raw && owner != process->thread->tid) || (!raw && owner != 0)) return ERR_BUSY;
        uint32_t generation;
        uint64_t sequence;
        KernelEventSource *source = tty_input_event(id, &generation);
        if (!source || event_source_snapshot(source, &generation, &sequence) < 0) return ERR_IO;
        const int byte = tty_input_read_byte(id);
        if (byte >= 0) { *character = byte; return 0; }
        const long wait = thread_wait_event(source, generation, sequence, 0, ERR_TIMED_OUT);
        if (wait < 0) return wait;
    }
}

static long console_read_line(Process *process, TtyId id, uintptr_t buffer, size_t length);
static long console_read_raw(Process *process, TtyId id, uintptr_t buffer);

long io_read(long fd, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    const int value = fd_value(process, fd);
    OpenFile *ofd = get_ofd(value);
    if (value == -99 || fd_tty_writable(value) || (ofd && !ofd->readable) ||
        length > LONG_MAX) return ERR_BAD_FD;
    if (!user_writable(process->page_table, buffer, length)) return ERR_ADDRESS;
    if (length == 0) return 0;
    if (fd_tty_readable(value)) {
        const TtyId id = fd_tty_id(value);
        if (!tty_session_active(id)) return ERR_IO;
        const long owner = tty_raw_owner(id);
        if (owner < 0) return owner;
        if (owner != 0) {
            if (owner != process->thread->tid) return ERR_BUSY;
            if (length > 128) return ERR_INVALID;
            return console_read_raw(process, id, buffer);
        }
        if (length < 2 || length > 128) return ERR_INVALID;
        return console_read_line(process, id, buffer, length);
    }
    if (ofd && ofd->type == OFD_PIPE_READ) {
        return pipe_read_wait(process, (int)fd, buffer, length);
    }
    if (ofd && ofd->type == OFD_FILE) {
        const int gate_error = kernel_gate_enter(&ofd_gates[value]);
        if (gate_error < 0) return gate_error;
        unsigned char staging[512];
        if (length > sizeof(staging)) length = sizeof(staging);
        const long amount = fs_read_inode(ofd->inode, ofd->offset, staging, length);
        long result = amount;
        if (amount >= 0) {
            const int error = user_copy_to(process->page_table, buffer, staging, (size_t)amount);
            if (error < 0) result = error;
            else ofd->offset += (size_t)amount;
        }
        const int leave_error = kernel_gate_leave(&ofd_gates[value]);
        return leave_error < 0 ? leave_error : result;
    }
    return ERR_INVALID;
}

long io_write(long fd, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    const int value = fd_value(process, fd);
    OpenFile *ofd = get_ofd(value);
    if (value == -99 || fd_tty_readable(value) ||
        (ofd && !ofd->writable) || length > LONG_MAX) return ERR_BAD_FD;
    if (!user_readable(process->page_table, buffer, length)) return ERR_ADDRESS;
    if (length == 0) return 0;
    if (fd_tty_writable(value)) {
        unsigned char staging[256];
        if (length > sizeof(staging)) length = sizeof(staging);
        const int error = user_copy_from(process->page_table, staging, buffer, length);
        if (error < 0) return error;
        const TtyId id = fd_tty_id(value);
        return tty_output_write(id, staging, length);
    }
    if (ofd && ofd->type == OFD_PIPE_WRITE) {
        return pipe_write_wait(process, (int)fd, buffer, length);
    }
    if (ofd && ofd->type == OFD_FILE) {
        unsigned char staging[512];
        if (length > sizeof(staging)) length = sizeof(staging);
        const int error = user_copy_from(process->page_table, staging, buffer, length);
        if (error < 0) return error;
        const int gate_error = kernel_gate_enter(&ofd_gates[value]);
        if (gate_error < 0) return gate_error;
        long result;
        if (ofd->append) {
            const int scope_error = fs_scope_enter();
            if (scope_error < 0) {
                (void)kernel_gate_leave(&ofd_gates[value]);
                return scope_error;
            }
            size_t append_next_offset = 0;
            const long end = fs_inode_size(ofd->inode);
            if (end < 0) result = end;
            else {
                const size_t append_offset = (size_t)end;
                result = append_offset > SIZE_MAX - length ? ERR_INVALID :
                         fs_write_inode(ofd->inode, append_offset, staging, length);
                if (result > 0) append_next_offset = append_offset + (size_t)result;
            }
            const int scope_leave = fs_scope_leave();
            if (result >= 0 && scope_leave < 0) result = scope_leave;
            if (result > 0) ofd->offset = append_next_offset;
        } else if (ofd->offset > SIZE_MAX - length) {
            result = ERR_INVALID;
        } else {
            result = fs_write_inode(ofd->inode, ofd->offset, staging, length);
            if (result > 0) ofd->offset += (size_t)result;
        }
        const int leave_error = kernel_gate_leave(&ofd_gates[value]);
        return leave_error < 0 ? leave_error : result;
    }
    return ERR_INVALID;
}

static void console_echo(TtyId id, unsigned char byte) {
    (void)tty_output_write(id, &byte, 1);
}

static long console_read_line(Process *process, TtyId id, uintptr_t buffer, size_t length) {
    unsigned char line[128];
    size_t count = 0;
    int overflow = 0;
    for (;;) {
        int character;
        const long input = tty_wait_byte(process, id, 0, &character);
        if (input < 0) return input;
        if (tty_filter_crlf(id, character, 0)) continue;
        if (character == 4) {
            if (overflow) return ERR_INVALID;
            if (count == 0) return 0;
            const int error = user_copy_to(process->page_table, buffer, line, count);
            return error < 0 ? error : (long)count;
        }
        if (character == '\r' || character == '\n') {
            console_echo(id, '\n');
            if (overflow) return ERR_INVALID;
            if (count >= length) return ERR_INVALID;
            line[count++] = '\n';
            const int error = user_copy_to(process->page_table, buffer, line, count);
            return error < 0 ? error : (long)count;
        }
        if (character == 8 || character == 127) {
            if (count > 0) {
                --count;
                console_echo(id, '\b'); console_echo(id, ' '); console_echo(id, '\b');
            }
        } else if (character == '\t' || (character >= 32 && character <= 126)) {
            if (count < length - 1 && !overflow) {
                line[count++] = (unsigned char)character;
                console_echo(id, (unsigned char)character);
            } else overflow = 1;
        }
    }
}

static long console_read_raw(Process *process, TtyId id, uintptr_t buffer) {
    for (;;) {
        int character;
        const long input = tty_wait_byte(process, id, 1, &character);
        if (input < 0) return input;
        if (tty_filter_crlf(id, character, 1)) continue;
        const int error = user_write_byte(process->page_table, buffer, (unsigned char)character);
        return error < 0 ? error : 1;
    }
}

void io_poll(void) {
    /* Scheduler context is current_index == -1: only harvest UART bytes here. */
    tty_poll();
}
