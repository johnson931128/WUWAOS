#include "syscall.h"
#include "console.h"
#include "process.h"
#include "syscall_numbers.h"
#include "vm.h"
#include <limits.h>

static long sys_write(unsigned long fd, uintptr_t buffer, size_t length) {
    if (fd != 1 || length > LONG_MAX) {
        return ERR_INVALID;
    }
    if (!user_readable(buffer, length)) {
        return ERR_ADDRESS;
    }
    for (size_t i = 0; i < length; ++i) {
        console_putc((char)user_read_byte(buffer + i));
    }
    return (long)length;
}

void syscall_dispatch(TrapFrame *frame) {
    const uint64_t number = frame->registers[17];
    console_puts("[syscall] from U-mode number=");
    console_hex(number);
    console_putc('\n');

    long result;
    switch (number) {
    case SYS_WRITE:
        result = sys_write(frame->registers[10], frame->registers[11],
                           frame->registers[12]);
        break;
    case SYS_GETPID:
        result = current_process.pid;
        break;
    case SYS_EXIT:
        process_exit((long)frame->registers[10]);
    default:
        result = ERR_NO_SYSCALL;
        break;
    }
    frame->registers[10] = (uint64_t)result;
    console_puts("[syscall] return=");
    console_hex((unsigned long)result);
    console_putc('\n');
}
