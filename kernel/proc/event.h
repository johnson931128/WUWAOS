#ifndef WUWAOS_KERNEL_PROC_EVENT_H
#define WUWAOS_KERNEL_PROC_EVENT_H

#include <stdint.h>

typedef struct KernelEventSource {
    uint32_t generation;
    uint64_t sequence;
    unsigned active : 1;
    unsigned retired : 1;
} KernelEventSource;

typedef struct {
    KernelEventSource *source;
    uint32_t generation;
    uint64_t sequence;
    uint64_t deadline;
    long timeout_result;
    long result;
    unsigned active : 1;
    unsigned completed : 1;
} KernelEventWait;

/* Source storage must remain valid while registered; close wakes waiters before reuse. */
int event_source_init(KernelEventSource *source, uint32_t generation);
int event_source_snapshot(const KernelEventSource *source, uint32_t *generation,
                         uint64_t *sequence);
void event_source_notify(KernelEventSource *source, long result);
void event_source_close(KernelEventSource *source, long result);

#endif
