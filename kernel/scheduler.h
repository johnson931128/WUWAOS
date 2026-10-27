#ifndef WUWAOS_SCHEDULER_H
#define WUWAOS_SCHEDULER_H

#include "process.h"

#define SCHEDULER_QUANTUM_MS 50UL

/* Called in the single-core kernel with interrupts disabled.
   process.c owns state transitions; enqueue requires PROCESS_READY. */
void scheduler_init(void);
void scheduler_enqueue(Process *process);
void scheduler_remove(Process *process);
Process *scheduler_next(void);
void scheduler_print_ready(void);

#endif
