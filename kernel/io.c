#include "io.h"
#include "console.h"
#include "syscall_numbers.h"
#include "vm.h"
#include <limits.h>

#define PIPE_LIMIT 2
#define PIPE_CAPACITY 16
#define IO_PENDING LONG_MIN

typedef struct {
    int used;
    int readers;
    int writers;
    unsigned char bytes[PIPE_CAPACITY];
    size_t read_at;
    size_t write_at;
    size_t count;
} Pipe;

static Pipe pipes[PIPE_LIMIT];
static int skip_lf;

int io_pipe_create(void) {
    for (int i = 0; i < PIPE_LIMIT; ++i) {
        Pipe *pipe = &pipes[i];
        if (!pipe->used) {
            pipe->used = 1;
            pipe->readers = 1;
            pipe->writers = 1;
            pipe->read_at = pipe->write_at = pipe->count = 0;
            return i;
        }
    }
    return -1;
}

void io_pipe_release(int pipe) {
    pipes[pipe].used = 0;
}

void io_inherit(Process *child, const Process *parent) {
    child->input_pipe = parent->input_pipe;
    child->output_pipe = parent->output_pipe;
    if (child->input_pipe >= 0) {
        ++pipes[child->input_pipe].readers;
    }
    if (child->output_pipe >= 0) {
        ++pipes[child->output_pipe].writers;
    }
}

void io_close_process(Process *process) {
    if (process->input_pipe >= 0) {
        --pipes[process->input_pipe].readers;
        process->input_pipe = -1;
    }
    if (process->output_pipe >= 0) {
        --pipes[process->output_pipe].writers;
        process->output_pipe = -1;
    }
    for (int i = 0; i < PIPE_LIMIT; ++i) {
        if (pipes[i].used && pipes[i].readers == 0 && pipes[i].writers == 0) {
            io_pipe_release(i);
        }
    }
}

static long pipe_read(Process *process, uintptr_t buffer, size_t length) {
    Pipe *pipe = &pipes[process->input_pipe];
    if (pipe->count == 0) {
        return pipe->writers == 0 ? 0 : IO_PENDING;
    }
    size_t copied = 0;
    while (copied < length && pipe->count > 0) {
        user_write_byte(process->page_table, buffer + copied, pipe->bytes[pipe->read_at]);
        pipe->read_at = (pipe->read_at + 1) % PIPE_CAPACITY;
        --pipe->count;
        ++copied;
    }
    return (long)copied;
}

static long pipe_write(Process *process, uintptr_t buffer, size_t length) {
    Pipe *pipe = &pipes[process->output_pipe];
    if (pipe->readers == 0) {
        return ERR_BROKEN_PIPE;
    }
    if (pipe->count == PIPE_CAPACITY) {
        return IO_PENDING;
    }
    size_t copied = 0;
    while (copied < length && pipe->count < PIPE_CAPACITY) {
        pipe->bytes[pipe->write_at] = user_read_byte(process->page_table, buffer + copied);
        pipe->write_at = (pipe->write_at + 1) % PIPE_CAPACITY;
        ++pipe->count;
        ++copied;
    }
    return (long)copied;
}

long io_read(long fd, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    if (fd != 0 || length > LONG_MAX) {
        return ERR_INVALID;
    }
    if (!user_writable(process->page_table, buffer, length)) {
        return ERR_ADDRESS;
    }
    if (length == 0) {
        return 0;
    }
    if (process->input_pipe < 0) {
        if (length < 2 || length > 128) {
            return ERR_INVALID;
        }
        process_block_io(IO_CONSOLE_READ, buffer, length);
        return 0; /* The saved return value is completed by io_poll. */
    }
    const long result = pipe_read(process, buffer, length);
    if (result == IO_PENDING) {
        process_block_io(IO_PIPE_READ, buffer, length);
        return 0;
    }
    return result;
}

long io_write(long fd, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    if (fd != 1 || length > LONG_MAX) {
        return ERR_INVALID;
    }
    if (!user_readable(process->page_table, buffer, length)) {
        return ERR_ADDRESS;
    }
    if (length == 0) {
        return 0;
    }
    if (process->output_pipe >= 0) {
        const long result = pipe_write(process, buffer, length);
        if (result == IO_PENDING) {
            process_block_io(IO_PIPE_WRITE, buffer, length);
            return 0;
        }
        return result;
    }
    for (size_t i = 0; i < length; ++i) {
        console_putc((char)user_read_byte(process->page_table, buffer + i));
    }
    return (long)length;
}

static long console_read_line(Process *process) {
    int character;
    while ((character = console_getc()) >= 0) {
        if (character == '\n' && skip_lf) {
            skip_lf = 0;
            continue;
        }
        skip_lf = character == '\r';
        if (character == '\r' || character == '\n') {
            console_putc('\n');
            if (process->io_overflow) {
                return ERR_INVALID;
            }
            user_write_byte(process->page_table, process->io_buffer + process->io_count, '\n');
            return (long)++process->io_count;
        }
        if (character == 8 || character == 127) {
            if (process->io_count > 0) {
                --process->io_count;
                console_puts("\b \b");
            }
        } else if (character == '\t' || (character >= 32 && character <= 126)) {
            if (process->io_count < process->io_length - 1 && !process->io_overflow) {
                user_write_byte(process->page_table, process->io_buffer + process->io_count++,
                                (unsigned char)character);
                console_putc((char)character);
            } else {
                process->io_overflow = 1;
            }
        }
    }
    return IO_PENDING;
}

void io_poll(void) {
    int progress;
    do {
        progress = 0;
        for (int i = 0; i < PROCESS_LIMIT; ++i) {
            Process *process = process_slot(i);
            if (process->state != PROCESS_WAITING || process->wait_reason != WAIT_IO) {
                continue;
            }
            long result = IO_PENDING;
            if (process->io_operation == IO_CONSOLE_READ) {
                result = console_read_line(process);
            } else if (process->io_operation == IO_PIPE_READ) {
                result = pipe_read(process, process->io_buffer, process->io_length);
            } else if (process->io_operation == IO_PIPE_WRITE) {
                result = pipe_write(process, process->io_buffer, process->io_length);
            }
            if (result != IO_PENDING) {
                process_complete_io(process, result);
                progress = 1;
            }
        }
    } while (progress);
}

int io_console_waiting(void) {
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        const Process *process = process_slot(i);
        if (process->state == PROCESS_WAITING && process->io_operation == IO_CONSOLE_READ) {
            return 1;
        }
    }
    return 0;
}
