#include "trap.h"
#include "console.h"
#include "process.h"
#include "syscall.h"
#include "vm.h"
#include "timer.h"

#define SSTATUS_SPP (1UL << 8)
#define USER_ECALL 8
#define SUPERVISOR_TIMER ((1UL << 63) | 5UL)

extern void trap_entry(void);

void trap_init(void) {
    __asm__ volatile("csrw stvec, %0\ncsrw sscratch, zero"
                     : : "r"((uintptr_t)trap_entry) : "memory");
}

TrapFrame *trap_handler(TrapFrame *frame) {
    const int from_user = (frame->sstatus & SSTATUS_SPP) == 0;
    if (from_user) {
        process_current()->frame = frame;
    }
    if (from_user && frame->scause == SUPERVISOR_TIMER) {
        timer_interrupt();
        process_preempt();
        return process_resume(); /* Interrupt sepc is already the resume address. */
    }
    if (from_user && frame->scause == USER_ECALL) {
        frame->sepc += 4; /* ECALL is a 32-bit instruction. */
        syscall_dispatch(frame);
        return process_resume();
    }
    if (from_user && (frame->scause == 12 || frame->scause == 13 || frame->scause == 15)) {
        const int error = vm_handle_fault(process_current()->page_table, frame->stval, frame->scause, frame->sepc);
        if (error == 0) return process_resume(); /* Retry the same instruction, not sepc + 4. */
        console_puts("[fault] PID="); console_number(process_current()->pid);
        console_puts(" cause="); console_number((long)frame->scause);
        console_puts(" address="); console_hex(frame->stval);
        console_puts(" error="); console_number(error); console_putc('\n');
        process_exit(error);
        return process_resume();
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
        return process_resume();
    }
    kernel_idle();
}
