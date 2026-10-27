#ifndef WUWAOS_SWAP_H
#define WUWAOS_SWAP_H

#include <stdint.h>

void swap_init(void);
unsigned int swap_alloc(void); /* 1-based slot; 0 means unavailable/full. */
void swap_free(unsigned int slot);
int swap_read(unsigned int slot, uintptr_t page);
int swap_write(unsigned int slot, uintptr_t page);
unsigned int swap_used(void);
unsigned int swap_total(void);

#endif
