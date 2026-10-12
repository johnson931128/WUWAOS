#include "process.h"
#include "console.h"
#include "vm.h"

static unsigned char kernel_stack[16 * 1024] __attribute__((aligned(16)));
Process current_process;

void process_init(uintptr_t page_table) {
    current_process.pid = 1;
    current_process.state = PROCESS_READY;
    current_process.page_table = page_table;
    current_process.kernel_stack_top = (uintptr_t)(kernel_stack + sizeof(kernel_stack));
    current_process.user_stack_top = USER_STACK_TOP;
    current_process.frame = (TrapFrame *)(current_process.kernel_stack_top - TF_SIZE);
    /* kernel_stack starts in zero-initialized BSS. */
    current_process.frame->registers[2] = USER_STACK_TOP;
    current_process.frame->sepc = USER_ENTRY;
    /* Select U-mode without interrupts, SUM/MXR, floating-point or vectors. */
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    current_process.frame->sstatus = status &
        ~((1UL << 8) | (1UL << 5) | (1UL << 1) | (1UL << 18) | (1UL << 19) |
          (3UL << 13) | (3UL << 9));
    console_puts("[process] PID=1 Ready\n");
}

_Noreturn void process_start(void) {
    current_process.state = PROCESS_RUNNING;
    vm_enable(current_process.page_table);
    console_puts("[process] PID=1 Running; entering U-mode\n");
    user_enter(current_process.frame);
}

_Noreturn void process_exit(long status) {
    current_process.exit_status = status;
    current_process.state = PROCESS_EXITED;
    console_puts("[process] PID=1 Exited status=");
    console_hex((unsigned long)status);
    console_putc('\n');
    kernel_idle();
}
