#include "kernel/proc/gate.h"
#include "kernel/proc/thread.h"
#include "syscall_numbers.h"
#include <limits.h>

int kernel_gate_init(KernelGate *gate) {
    if (!gate) return ERR_INVALID;
    if (gate->initialized) return 0;
    if (gate->owner || gate->depth) return ERR_INVALID;
    if (event_source_init(&gate->event, 1) < 0) return ERR_INVALID;
    gate->initialized = 1;
    return 0;
}

static int owner_matches(const KernelGate *gate, const Thread *thread) {
    return gate->owner == thread && gate->owner_tid == (thread ? thread->tid : 0);
}

int kernel_gate_try_enter(KernelGate *gate) {
    if (!gate || kernel_gate_init(gate) < 0) return ERR_INVALID;
    Thread *thread = thread_current_or_null();
    if (thread && thread->kill_pending && !thread->cancel_defer_depth) return ERR_INTERRUPTED;
    if (gate->depth) {
        if (!owner_matches(gate, thread)) return ERR_BUSY;
        if (gate->depth == UINT_MAX) return ERR_INVALID;
        ++gate->depth;
        thread_gate_acquired(thread);
        return 0;
    }
    gate->owner = thread;
    gate->owner_tid = thread ? thread->tid : 0;
    gate->depth = 1;
    thread_gate_acquired(thread);
    return 0;
}

int kernel_gate_enter(KernelGate *gate) {
    if (!gate || kernel_gate_init(gate) < 0) return ERR_INVALID;
    for (;;) {
        Thread *thread = thread_current_or_null();
        if (thread && thread->kill_pending && !thread->cancel_defer_depth) return ERR_INTERRUPTED;
        const int result = kernel_gate_try_enter(gate);
        if (result != ERR_BUSY) return result;
        if (!thread_wait_allowed()) return ERR_BUSY;
        uint32_t generation;
        uint64_t sequence;
        if (event_source_snapshot(&gate->event, &generation, &sequence) < 0) return ERR_IO;
        if (gate->depth == 0) continue;
        const long wait = thread_wait_event(&gate->event, generation, sequence, 0, ERR_TIMED_OUT);
        if (wait < 0) return wait;
    }
}

int kernel_gate_leave(KernelGate *gate) {
    if (!gate || !gate->initialized) return ERR_INVALID;
    Thread *thread = thread_current_or_null();
    if (!gate->depth || !owner_matches(gate, thread)) return ERR_INVALID;
    --gate->depth;
    thread_gate_released(thread);
    if (gate->depth == 0) {
        gate->owner = 0;
        gate->owner_tid = 0;
        event_source_notify(&gate->event, 0);
    }
    return 0;
}
