#ifndef WUWAOS_CLOCK_H
#define WUWAOS_CLOCK_H

#include <stdint.h>

/* Fixed platform contract: QEMU virt, RV64, 10 MHz timebase. */
#define CLOCK_TIMEBASE_HZ 10000000UL

static inline uint64_t clock_ticks(void) {
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

static inline uint64_t clock_ms_ticks(unsigned long milliseconds) {
    return (CLOCK_TIMEBASE_HZ / 1000UL) * (uint64_t)milliseconds;
}

#endif
