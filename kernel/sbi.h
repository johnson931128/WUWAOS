#ifndef WUWAOS_SBI_H
#define WUWAOS_SBI_H

#include <stdint.h>

int sbi_timer_available(void);
long sbi_set_timer(uint64_t deadline);

#endif
