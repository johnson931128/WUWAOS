#include "scheduler.h"
#include "console.h"

typedef struct {
    Process *head;
    Process *tail;
    size_t count;
} ReadyQueue;

static ReadyQueue ready_queue;

void scheduler_init(void) {
    ready_queue = (ReadyQueue){0};
}

void scheduler_enqueue(Process *process) {
    if (process->ready_queued) {
        return;
    }
    process->ready_next = 0;
    if (ready_queue.tail != 0) {
        ready_queue.tail->ready_next = process;
    } else {
        ready_queue.head = process;
    }
    ready_queue.tail = process;
    process->ready_queued = 1;
    ++ready_queue.count;
}

Process *scheduler_next(void) {
    Process *next = ready_queue.head;
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

void scheduler_remove(Process *process) {
    if (!process->ready_queued) {
        return;
    }
    Process *previous = 0;
    Process *entry = ready_queue.head;
    while (entry != 0 && entry != process) {
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
    console_puts("Ready queue (FIFO), count=");
    console_number((long)ready_queue.count);
    console_puts(": ");
    if (ready_queue.head == 0) {
        console_puts("(empty)");
    }
    for (const Process *entry = ready_queue.head; entry != 0; entry = entry->ready_next) {
        console_number(entry->pid);
        if (entry->ready_next != 0) {
            console_puts(" -> ");
        }
    }
    console_putc('\n');
}
