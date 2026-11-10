#ifndef WUWAOS_SYSCALL_H
#define WUWAOS_SYSCALL_H
#include "arch/riscv/trap.h"
void syscall_dispatch(TrapFrame *frame);
#endif
