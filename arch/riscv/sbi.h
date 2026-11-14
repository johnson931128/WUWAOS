#ifndef WUWAOS_SBI_H
#define WUWAOS_SBI_H

#include <stdint.h>

int sbi_timer_available(void);
int sbi_smp_available(void);
long sbi_hart_start(unsigned long id, uintptr_t address);
long sbi_remote_sfence(unsigned long mask);
long sbi_set_timer(uint64_t deadline);

#endif
