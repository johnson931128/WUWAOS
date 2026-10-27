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
    if (address > UINTPTR_MAX - FS_PATH_MAX) return ERR_ADDRESS;
    for (size_t i = 0; i < FS_PATH_MAX; ++i) {
        const int error = user_read_byte(root, address + i, (unsigned char *)&path[i]);
        if (error < 0) return error;
        if (path[i] == 0) return i > 0 ? 0 : ERR_ADDRESS;
    }
    return ERR_ADDRESS;
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
    case SYS_EXEC: {
        char argument[FS_PATH_MAX];
        const long program = (long)frame->registers[10];
        if (program == PROGRAM_FAN) {
            result = copy_path(process_current()->page_table, frame->registers[11], argument);
            if (result < 0) break;
        }
        result = process_exec(program, program == PROGRAM_FAN ? argument : 0);
        if (result == 0) {
            return; /* Preserve the new image's initial a0 and entry point. */
        }
        break;
    }
    case SYS_CONSOLE_MODE:
        result = io_console_mode((long)frame->registers[10]);
        break;
    case SYS_SBRK:
        result = vm_sbrk(process_current()->page_table, (long)frame->registers[10]);
        break;
    case SYS_VMSTAT:
        result = vm_control((long)frame->registers[10]);
        break;
    case SYS_FS_LIST: {
        Process *process = process_current();
        char path[FS_PATH_MAX];
        char output[FS_OUTPUT_MAX];
        const uintptr_t user_buffer = frame->registers[11];
        const size_t capacity = (size_t)frame->registers[12];
        const int path_error = copy_path(process->page_table, frame->registers[10], path);
        if (path_error < 0) {
            result = path_error;
        } else if (capacity > sizeof(output) || !user_writable(process->page_table, user_buffer, capacity)) {
            result = ERR_ADDRESS;
        } else {
            result = fs_list(path, output, capacity);
            if (result >= 0) {
                const int error = user_copy_to(process->page_table, user_buffer, output, (size_t)result);
                if (error < 0) result = error;
            }
        }
        break;
    }
    case SYS_FS_MKDIR:
    case SYS_FS_TOUCH: {
        char path[FS_PATH_MAX];
        result = copy_path(process_current()->page_table, frame->registers[10], path);
        if (result == 0) result = number == SYS_FS_MKDIR ? fs_mkdir(path) : fs_touch(path);
        break;
    }
    case SYS_FS_READ: {
        Process *process = process_current();
        char path[FS_PATH_MAX];
        uintptr_t user_buffer = frame->registers[11];
        size_t length = (size_t)frame->registers[12];
        const int path_error = copy_path(process->page_table, frame->registers[10], path);
        if (path_error < 0 || !user_writable(process->page_table, user_buffer, length)) {
            result = path_error < 0 ? path_error : ERR_ADDRESS;
            break;
        }
        size_t total = 0;
        while (total < length) {
            size_t chunk = length - total;
            if (chunk > FS_IO_CHUNK) chunk = FS_IO_CHUNK;
            const long read_count = fs_read(path, total, fs_staging, chunk);
            if (read_count < 0) { total = (size_t)-1; result = read_count; break; }
            const int error = user_copy_to(process->page_table, user_buffer + total, fs_staging, (size_t)read_count);
            if (error < 0) { total = (size_t)-1; result = error; break; }
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
        const int path_error = copy_path(process->page_table, frame->registers[10], path);
        if (path_error < 0 ||
            length > sizeof(fs_staging) || !user_readable(process->page_table, user_buffer, length)) {
            result = path_error < 0 ? path_error : ERR_ADDRESS;
            break;
        }
        result = user_copy_from(process->page_table, fs_staging, user_buffer, length);
        if (result == 0) result = fs_write(path, fs_staging, length);
        break;
    }
    default:
        result = ERR_NO_SYSCALL;
        break;
    }
    frame->registers[10] = (uint64_t)result;
}
