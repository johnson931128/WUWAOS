#ifndef WUWAOS_TIMER_H
#define WUWAOS_TIMER_H

#include <stdint.h>

/* S-mode calls these with global interrupts disabled. */
void timer_init(void);
void timer_start_slice(unsigned long milliseconds);
void timer_stop(void);
void timer_interrupt(void);
uint64_t timer_interrupt_count(void);

#endif
