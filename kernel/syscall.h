#ifndef WUWAOS_SYSCALL_H
#define WUWAOS_SYSCALL_H
#include "trap.h"
void syscall_dispatch(TrapFrame *frame);
#endif
