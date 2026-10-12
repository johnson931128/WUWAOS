#include "trap.h"
#include "console.h"
#include "process.h"
#include "syscall.h"

#define SSTATUS_SPP (1UL << 8)
#define USER_ECALL 8

extern void trap_entry(void);

void trap_init(void) {
    __asm__ volatile("csrw stvec, %0\ncsrw sscratch, zero"
                     : : "r"((uintptr_t)trap_entry) : "memory");
}

void trap_handler(TrapFrame *frame) {
    const int from_user = (frame->sstatus & SSTATUS_SPP) == 0;
    if (from_user && frame->scause == USER_ECALL) {
        frame->sepc += 4; /* ECALL is a 32-bit instruction. */
        syscall_dispatch(frame);
        return;
    }

    console_puts(from_user ? "[trap] User trap" : "[trap] Kernel trap");
    console_puts(" scause=");
    console_hex(frame->scause);
    console_puts(" sepc=");
    console_hex(frame->sepc);
    console_puts(" stval=");
    console_hex(frame->stval);
    console_putc('\n');
    if (from_user) {
        process_exit(-1);
    }
    kernel_idle();
}
