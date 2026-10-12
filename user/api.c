#include "api.h"
#include "syscall_numbers.h"

static long syscall3(long number, long first, long second, long third) {
    register long a0 __asm__("a0") = first;
    register long a1 __asm__("a1") = second;
    register long a2 __asm__("a2") = third;
    register long a7 __asm__("a7") = number;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

long write(long fd, const char *buffer, unsigned long length) {
    return syscall3(SYS_WRITE, fd, (long)buffer, (long)length);
}

long getpid(void) {
    return syscall3(SYS_GETPID, 0, 0, 0);
}

_Noreturn void exit(long status) {
    syscall3(SYS_EXIT, status, 0, 0);
    for (;;) {
    }
}
