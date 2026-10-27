#include "timer.h"
#include "clock.h"
#include "sbi.h"
#include "console.h"

static uint64_t interrupts;
static int armed;

static void set_deadline(uint64_t deadline) {
    const long error = sbi_set_timer(deadline);
    if (error != 0) {
        console_puts("[timer] SBI set_timer failed, error=");
        console_number(error);
        console_putc('\n');
        kernel_idle();
    }
}

void timer_stop(void) {
    if (armed) {
        set_deadline(UINT64_MAX);
        armed = 0;
    }
}

void timer_init(void) {
    if (!sbi_timer_available()) {
        console_puts("[timer] OpenSBI TIME extension required\n");
        kernel_idle();
    }
    set_deadline(UINT64_MAX); /* Clear any firmware timer before enabling STIE. */
    __asm__ volatile("csrs sie, %0" : : "r"(1UL << 5) : "memory");
    console_puts("[timer] SBI TIME ready; QEMU virt timebase=10000000 Hz\n");
}

void timer_start_slice(unsigned long milliseconds) {
    set_deadline(clock_ticks() + clock_ms_ticks(milliseconds));
    armed = 1;
}

void timer_interrupt(void) {
    set_deadline(UINT64_MAX); /* Acknowledge even an unexpected pending interrupt. */
    armed = 0;
    ++interrupts;
}

uint64_t timer_interrupt_count(void) { return interrupts; }
