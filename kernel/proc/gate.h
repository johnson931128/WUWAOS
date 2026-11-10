#ifndef WUWAOS_KERNEL_PROC_GATE_H
#define WUWAOS_KERNEL_PROC_GATE_H

#include "kernel/proc/event.h"

typedef struct KernelGate {
    struct ThreadControlBlock *owner;
    long owner_tid;
    unsigned depth;
    KernelEventSource event;
    unsigned initialized : 1;
} KernelGate;

/* Gate storage must be zero-initialized and outlive every waiter. Operations
   run with SIE clear on the single hart; wait-capable callers use enter(). */
int kernel_gate_init(KernelGate *gate);
int kernel_gate_try_enter(KernelGate *gate);
int kernel_gate_enter(KernelGate *gate);
int kernel_gate_leave(KernelGate *gate);

#endif
