#ifndef WUWAOS_WORKSTATE_INFO_H
#define WUWAOS_WORKSTATE_INFO_H

#include <stdint.h>
#include "process_limits.h"

#define WORKSTATE_VERSION 1UL
typedef struct {
    int64_t pid, parent_pid;
    char name[12], state[12];
    uint64_t cpu_ticks, resident_pages, swapped_pages, heap_bytes;
} WorkstateProcess;

typedef struct {
    uint64_t version, ticks, timebase_hz, busy_ticks;
    uint64_t ram_bytes, free_bytes, reserved_bytes, page_bytes;
    uint64_t swap_used_pages, swap_total_pages, count;
    WorkstateProcess processes[PROCESS_LIMIT];
} WorkstateSnapshot;

#endif
