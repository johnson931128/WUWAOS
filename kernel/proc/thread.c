#include "kernel/proc/thread.h"
#include "kernel/proc/process.h"
#include "kernel/mm/vm.h"
#include "kernel/io/console.h"
#include <limits.h>

#define THREAD_KERNEL_STACK_SIZE (16UL * 1024UL)
#define THREAD_STACK_REGION_SIZE (PAGE_SIZE + THREAD_KERNEL_STACK_SIZE)

static Thread threads[THREAD_LIMIT];
static unsigned char kernel_stacks[THREAD_LIMIT][THREAD_STACK_REGION_SIZE]
    __attribute__((aligned(PAGE_SIZE)));
static Thread *current;
static long next_tid = 1;

static _Noreturn void thread_panic(const char *message) {
    console_puts("[thread] "); console_puts(message); console_putc('\n');
    kernel_idle();
}

void thread_init(void) {
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        if (vm_unmap_kernel_guard((uintptr_t)kernel_stacks[i]) < 0)
            thread_panic("Cannot install kernel stack guard");
    }
}

Thread *thread_allocate(Process *process) {
    /* Phase A: a PCB owns exactly one initial TCB; no extra-thread entry point. */
    if (!process || process->thread || next_tid == LONG_MAX) return 0;
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        Thread *thread = &threads[i];
        if (thread->state != THREAD_UNUSED) continue;
        *thread = (Thread){ .tid = next_tid++, .process = process, .state = THREAD_NEW };
        unsigned char *stack = kernel_stacks[i] + PAGE_SIZE;
        for (size_t j = 0; j < THREAD_KERNEL_STACK_SIZE; ++j) stack[j] = 0;
        thread->kernel_stack_top = (uintptr_t)(stack + THREAD_KERNEL_STACK_SIZE);
        return thread;
    }
    return 0;
}

void thread_release(Thread *thread) {
    if (!thread || thread == current || thread->ready_queued ||
        (thread->state != THREAD_NEW && thread->state != THREAD_ZOMBIE) ||
        thread->internal_gate_depth || thread->cancel_defer_depth ||
        thread->sync_wait_handle || thread->kernel_locks || thread->kernel_waiting ||
        thread->event_wait.active)
        thread_panic("Unsafe TCB release");
    *thread = (Thread){0};
}

Thread *thread_slot(int slot) {
    return slot >= 0 && slot < THREAD_LIMIT ? &threads[slot] : 0;
}
Thread *thread_current_or_null(void) { return current; }
Thread *thread_current(void) {
    if (!current) thread_panic("No running thread");
    return current;
}
void thread_set_current(Thread *thread) { current = thread; }

int thread_stack_guard_bounds(uintptr_t address, uintptr_t *bottom, uintptr_t *top) {
    if (!bottom || !top) return 0;
    const uintptr_t page = address & ~(uintptr_t)(PAGE_SIZE - 1U);
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        const uintptr_t guard = (uintptr_t)kernel_stacks[i];
        if (page != guard) continue;
        *bottom = guard + PAGE_SIZE;
        *top = *bottom + THREAD_KERNEL_STACK_SIZE;
        return 1;
    }
    return 0;
}
