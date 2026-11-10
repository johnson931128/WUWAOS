#ifndef WUWAOS_SCHEDULER_H
#define WUWAOS_SCHEDULER_H

#include "kernel/proc/thread.h"

#define SCHEDULER_QUANTUM_MS 50UL

/* Called in the single-core kernel with interrupts disabled.
   thread.c owns state transitions; enqueue requires THREAD_READY. */
void scheduler_init(void);
void scheduler_enqueue(Thread *thread);
void scheduler_remove(Thread *thread);
Thread *scheduler_next(void);
size_t scheduler_ready_count(void);
void scheduler_print_ready(void);

#endif
