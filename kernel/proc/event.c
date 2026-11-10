#include "kernel/proc/event.h"
#include "kernel/proc/thread.h"
#include "syscall_numbers.h"

int event_source_init(KernelEventSource *source, uint32_t generation) {
    if (!source || generation == 0 || source->active || source->retired ||
        generation <= source->generation) return ERR_INVALID;
    source->generation = generation;
    source->sequence = 0;
    source->active = 1;
    return 0;
}

int event_source_snapshot(const KernelEventSource *source, uint32_t *generation,
                          uint64_t *sequence) {
    if (!source || !source->active || !generation || !sequence) return ERR_IO;
    *generation = source->generation;
    *sequence = source->sequence;
    return 0;
}

void event_source_notify(KernelEventSource *source, long result) {
    if (!source || !source->active) return;
    if (source->sequence == UINT64_MAX) {
        event_source_close(source, ERR_IO);
        return;
    }
    ++source->sequence;
    thread_event_notify(source, source->generation, result);
}

void event_source_close(KernelEventSource *source, long result) {
    if (!source || !source->active) return;
    const uint32_t generation = source->generation;
    thread_event_notify(source, generation, result);
    source->active = 0;
    if (source->sequence == UINT64_MAX) source->retired = 1;
    else ++source->sequence;
}
