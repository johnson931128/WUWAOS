#ifndef WUWAOS_SYNC_H
#define WUWAOS_SYNC_H
#include "kernel/proc/thread.h"
long sync_call(long operation, long argument);
void sync_thread_cleanup(Thread *thread);
void sync_thread_cancel_wait(Thread *thread);
#endif
