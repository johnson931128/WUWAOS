#ifndef WUWAOS_KERNEL_WORKSTATE_H
#define WUWAOS_KERNEL_WORKSTATE_H

#include "kernel/proc/process.h"
void workstate_init(void);
void workstate_dispatch(Thread *process);
void workstate_pause(Thread *process);
long workstate_snapshot(uintptr_t address, size_t length);

#endif
