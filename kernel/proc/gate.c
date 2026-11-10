#include "kernel/proc/gate.h"
#include "kernel/proc/process.h"
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

static int owner_matches(const KernelGate *gate, const Process *process) {
    return gate->owner == process && gate->owner_pid == (process ? process->pid : 0);
}

int kernel_gate_try_enter(KernelGate *gate) {
    if (!gate || kernel_gate_init(gate) < 0) return ERR_INVALID;
    Process *process = process_current_or_null();
    if (process && process->kill_pending && !process->cancel_defer_depth) return ERR_INTERRUPTED;
    if (gate->depth) {
        if (!owner_matches(gate, process)) return ERR_BUSY;
        if (gate->depth == UINT_MAX) return ERR_INVALID;
        ++gate->depth;
        process_gate_acquired(process);
        return 0;
    }
    gate->owner = process;
    gate->owner_pid = process ? process->pid : 0;
    gate->depth = 1;
    process_gate_acquired(process);
    return 0;
}

int kernel_gate_enter(KernelGate *gate) {
    if (!gate || kernel_gate_init(gate) < 0) return ERR_INVALID;
    for (;;) {
        Process *process = process_current_or_null();
        if (process && process->kill_pending && !process->cancel_defer_depth) return ERR_INTERRUPTED;
        const int result = kernel_gate_try_enter(gate);
        if (result != ERR_BUSY) return result;
        if (!process_wait_allowed()) return ERR_BUSY;
        uint32_t generation;
        uint64_t sequence;
        if (event_source_snapshot(&gate->event, &generation, &sequence) < 0) return ERR_IO;
        if (gate->depth == 0) continue;
        const long wait = process_wait_event(&gate->event, generation, sequence, 0, ERR_TIMED_OUT);
        if (wait < 0) return wait;
    }
}

int kernel_gate_leave(KernelGate *gate) {
    if (!gate || !gate->initialized) return ERR_INVALID;
    Process *process = process_current_or_null();
    if (!gate->depth || !owner_matches(gate, process)) return ERR_INVALID;
    --gate->depth;
    process_gate_released(process);
    if (gate->depth == 0) {
        gate->owner = 0;
        gate->owner_pid = 0;
        event_source_notify(&gate->event, 0);
    }
    return 0;
}
