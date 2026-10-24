#ifndef WUWAOS_BLOCK_H
#define WUWAOS_BLOCK_H

#include <stdint.h>

int block_init_from_fdt(const void *fdt, uintptr_t *mmio_base);
int block_read(uint32_t sector, void *buffer);
int block_write(uint32_t sector, const void *buffer);
uint32_t block_sector_count(void);

#endif
