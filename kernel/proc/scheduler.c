#include "kernel/proc/scheduler.h"
#include "kernel/io/console.h"
#include "arch/riscv/timer.h"
#include "arch/riscv/irq.h"
#include "drivers/virtio/block.h"

typedef struct {
    Thread *head;
    Thread *tail;
    size_t count;
} ReadyQueue;

static ReadyQueue ready_queue;

void scheduler_init(void) {
    ready_queue = (ReadyQueue){0};
}

void scheduler_enqueue(Thread *thread) {
    if (!thread || thread->state != THREAD_READY) {
        console_puts("[scheduler] Only Ready threads may be queued\n");
        kernel_idle();
    }
    if (thread->ready_queued) {
        return;
    }
    thread->ready_next = 0;
    if (ready_queue.tail != 0) {
        ready_queue.tail->ready_next = thread;
    } else {
        ready_queue.head = thread;
    }
    ready_queue.tail = thread;
    thread->ready_queued = 1;
    ++ready_queue.count;
}

Thread *scheduler_next(void) {
    Thread *next = ready_queue.head;
    if (next == 0) {
        return 0;
    }
    ready_queue.head = next->ready_next;
    if (ready_queue.head == 0) {
        ready_queue.tail = 0;
    }
    next->ready_next = 0;
    next->ready_queued = 0;
    --ready_queue.count;
    return next;
}

size_t scheduler_ready_count(void) { return ready_queue.count; }

void scheduler_remove(Thread *thread) {
    if (!thread->ready_queued) {
        return;
    }
    Thread *previous = 0;
    Thread *entry = ready_queue.head;
    while (entry != 0 && entry != thread) {
        previous = entry;
        entry = entry->ready_next;
    }
    if (entry == 0) {
        console_puts("[scheduler] Inconsistent ready queue\n");
        kernel_idle();
    }
    if (previous != 0) {
        previous->ready_next = entry->ready_next;
    } else {
        ready_queue.head = entry->ready_next;
    }
    if (ready_queue.tail == entry) {
        ready_queue.tail = previous;
    }
    entry->ready_next = 0;
    entry->ready_queued = 0;
    --ready_queue.count;
}

void scheduler_print_ready(void) {
    console_puts("Scheduler: RR, quantum_ms=");
    console_number(SCHEDULER_QUANTUM_MS);
    console_puts(" timer_interrupts=");
    console_number((long)timer_interrupt_count());
    console_puts("\nReady TID queue (RR), count=");
    console_number((long)ready_queue.count);
    console_puts(": ");
    if (ready_queue.head == 0) {
        console_puts("(empty)");
    }
    for (const Thread *entry = ready_queue.head; entry != 0; entry = entry->ready_next) {
        console_number(entry->tid);
        if (entry->ready_next != 0) {
            console_puts(" -> ");
        }
    }
    console_putc('\n');
    irq_print_stats();
    block_print_stats();
}
