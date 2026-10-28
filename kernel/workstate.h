#ifndef WUWAOS_KERNEL_WORKSTATE_H
#define WUWAOS_KERNEL_WORKSTATE_H

#include "process.h"
void workstate_init(void);
void workstate_dispatch(Process *process);
void workstate_pause(Process *process);
long workstate_snapshot(uintptr_t address, size_t length);

#endif
