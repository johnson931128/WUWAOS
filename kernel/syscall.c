#include "syscall.h"
#include "console.h"
#include "process.h"
#include "syscall_numbers.h"
#include "vm.h"
#include "io.h"
#include "fs.h"

#define FS_PATH_MAX 128
#define FS_IO_CHUNK 512
#define FS_OUTPUT_MAX 512
#define FS_FILE_MAX ((8U + 8U * 128U) * 512U)

static unsigned char fs_staging[FS_FILE_MAX];

static int copy_path(uintptr_t root, uintptr_t address, char path[FS_PATH_MAX]) {
    if (address > UINTPTR_MAX - FS_PATH_MAX) return -1;
    for (size_t i = 0; i < FS_PATH_MAX; ++i) {
        if (!user_readable(root, address + i, 1)) return -1;
        path[i] = (char)user_read_byte(root, address + i);
        if (path[i] == 0) return i > 0 ? 0 : -1;
    }
    return -1;
}

void syscall_dispatch(TrapFrame *frame) {
    const uint64_t number = frame->registers[17];

    long result;
    switch (number) {
    case SYS_WRITE:
        result = io_write((long)frame->registers[10], frame->registers[11],
                           frame->registers[12]);
        break;
    case SYS_GETPID:
        result = process_current()->pid;
        break;
    case SYS_EXIT:
        process_exit((long)frame->registers[10]);
        return;
    case SYS_SPAWN:
        result = process_spawn((long)frame->registers[10]);
        break;
    case SYS_YIELD:
        result = process_yield();
        break;
    case SYS_SLEEP:
        result = process_sleep((long)frame->registers[10]);
        break;
    case SYS_WAKEUP:
        result = process_wakeup((long)frame->registers[10]);
        break;
    case SYS_WAIT:
        result = process_wait((long)frame->registers[10], frame->registers[11]);
        break;
    case SYS_READ:
        result = io_read((long)frame->registers[10], frame->registers[11], frame->registers[12]);
        break;
    case SYS_PS:
        result = process_list();
        break;
    case SYS_PCB:
        result = process_inspect((long)frame->registers[10]);
        break;
    case SYS_KILL:
        result = process_kill((long)frame->registers[10]);
        break;
    case SYS_PIPELINE:
        result = process_pipeline(frame->registers[10]);
        break;
    case SYS_TRACE:
        result = process_trace((long)frame->registers[10]);
        break;
    case SYS_FORK:
        result = process_fork();
        break;
    case SYS_EXEC:
        result = process_exec((long)frame->registers[10]);
        if (result == 0) {
            return; /* Preserve the new image's initial a0 and entry point. */
        }
        break;
    case SYS_FS_LIST: {
        Process *process = process_current();
        char path[FS_PATH_MAX];
        char output[FS_OUTPUT_MAX];
        const uintptr_t user_buffer = frame->registers[11];
        const size_t capacity = (size_t)frame->registers[12];
        if (copy_path(process->page_table, frame->registers[10], path) < 0) {
            result = ERR_ADDRESS;
        } else if (capacity > sizeof(output) || !user_writable(process->page_table, user_buffer, capacity)) {
            result = ERR_ADDRESS;
        } else {
            result = fs_list(path, output, capacity);
            if (result >= 0) {
                for (long i = 0; i < result; ++i) user_write_byte(process->page_table, user_buffer + (uintptr_t)i, (unsigned char)output[i]);
            }
        }
        break;
    }
    case SYS_FS_MKDIR:
    case SYS_FS_TOUCH: {
        char path[FS_PATH_MAX];
        if (copy_path(process_current()->page_table, frame->registers[10], path) < 0) result = ERR_ADDRESS;
        else result = number == SYS_FS_MKDIR ? fs_mkdir(path) : fs_touch(path);
        break;
    }
    case SYS_FS_READ: {
        Process *process = process_current();
        char path[FS_PATH_MAX];
        uintptr_t user_buffer = frame->registers[11];
        size_t length = (size_t)frame->registers[12];
        if (copy_path(process->page_table, frame->registers[10], path) < 0 ||
            !user_writable(process->page_table, user_buffer, length)) {
            result = ERR_ADDRESS;
            break;
        }
        size_t total = 0;
        while (total < length) {
            size_t chunk = length - total;
            if (chunk > FS_IO_CHUNK) chunk = FS_IO_CHUNK;
            const long read_count = fs_read(path, total, fs_staging, chunk);
            if (read_count < 0) { total = (size_t)-1; result = read_count; break; }
            for (long i = 0; i < read_count; ++i) user_write_byte(process->page_table, user_buffer + total + (uintptr_t)i, fs_staging[i]);
            total += (size_t)read_count;
            if ((size_t)read_count < chunk) break;
        }
        if (total != (size_t)-1) result = (long)total;
        break;
    }
    case SYS_FS_WRITE: {
        Process *process = process_current();
        char path[FS_PATH_MAX];
        uintptr_t user_buffer = frame->registers[11];
        size_t length = (size_t)frame->registers[12];
        if (copy_path(process->page_table, frame->registers[10], path) < 0 ||
            length > sizeof(fs_staging) || !user_readable(process->page_table, user_buffer, length)) {
            result = ERR_ADDRESS;
            break;
        }
        for (size_t i = 0; i < length; ++i) fs_staging[i] = user_read_byte(process->page_table, user_buffer + i);
        result = fs_write(path, fs_staging, length);
        break;
    }
    default:
        result = ERR_NO_SYSCALL;
        break;
    }
    frame->registers[10] = (uint64_t)result;
}
