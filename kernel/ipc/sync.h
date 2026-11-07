#ifndef WUWAOS_SYNC_H
#define WUWAOS_SYNC_H
#include "kernel/proc/process.h"
long sync_call(long operation, long argument);
void sync_process_cleanup(Process *process);
#endif
