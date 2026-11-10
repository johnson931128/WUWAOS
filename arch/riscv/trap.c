#include "arch/riscv/trap.h"
#include "kernel/io/console.h"
#include "kernel/proc/process.h"
#include "kernel/syscall/syscall.h"
#include "kernel/mm/vm.h"
#include "arch/riscv/timer.h"
#include "arch/riscv/irq.h"

#define SSTATUS_SPP (1UL << 8)
#define USER_ECALL 8
#define SUPERVISOR_EXTERNAL ((1UL << 63) | 9UL)
#define SUPERVISOR_TIMER ((1UL << 63) | 5UL)

extern void trap_entry(void);

void trap_init(void) {
    __asm__ volatile("csrw stvec, %0\ncsrw sscratch, zero"
                     : : "r"((uintptr_t)trap_entry) : "memory");
}

TrapFrame *trap_handler(TrapFrame *frame) {
    const int from_user = (frame->sstatus & SSTATUS_SPP) == 0;
    if (from_user) {
        thread_current()->frame = frame;
    }
    if (!from_user && (frame->scause == 12 || frame->scause == 13 || frame->scause == 15) &&
        vm_is_kernel_guard(frame->stval)) {
        uintptr_t bottom = 0, top = 0;
        int scheduler_stack = 0;
        const Process *process = process_current_or_null();
        (void)thread_kernel_stack_bounds(frame->stval, &bottom, &top, &scheduler_stack);
        console_puts("[guard] Kernel stack fault PID=");
        console_number(process ? process->pid : 0);
        console_puts(scheduler_stack ? " scheduler stack=[" : " process stack=[");
        console_hex(bottom);
        console_puts(", ");
        console_hex(top);
        console_puts("] address=");
        console_hex(frame->stval);
        console_putc('\n');
        kernel_idle();
    }
    if (frame->scause == SUPERVISOR_EXTERNAL) {
        if (from_user) thread_kernel_call_enter();
        irq_dispatch();
        if (!from_user) return frame;
        TrapFrame *resume = thread_resume();
        thread_kernel_call_return();
        return resume; /* External IRQs preserve sepc. */
    }
    if (from_user && frame->scause == SUPERVISOR_TIMER) {
        thread_kernel_call_enter();
        timer_interrupt();
        thread_preempt();
        TrapFrame *resume = thread_resume(); /* Interrupt sepc is already the resume address. */
        thread_kernel_call_return();
        return resume;
    }
    if (from_user && frame->scause == USER_ECALL) {
        thread_kernel_call_enter();
        frame->sepc += 4; /* ECALL is a 32-bit instruction. */
        syscall_dispatch(frame);
        TrapFrame *resume = thread_resume();
        thread_kernel_call_return();
        return resume;
    }
    if (from_user && (frame->scause == 12 || frame->scause == 13 || frame->scause == 15)) {
        thread_kernel_call_enter();
        const int error = vm_handle_fault(process_current()->page_table, frame->stval, frame->scause, frame->sepc);
        if (error == 0) {
            TrapFrame *resume = thread_resume(); /* Retry the same instruction, not sepc + 4. */
            thread_kernel_call_return();
            return resume;
        }
        /* A cancelled Gate wait must unwind to the normal kill boundary first. */
        thread_kernel_call_return();
        console_puts("[fault] PID="); console_number(process_current()->pid);
        console_puts(" cause="); console_number((long)frame->scause);
        console_puts(" address="); console_hex(frame->stval);
        console_puts(" error="); console_number(error); console_putc('\n');
        process_exit(error);
        return thread_resume();
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
        return thread_resume();
    }
    kernel_idle();
}
