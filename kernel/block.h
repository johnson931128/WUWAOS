#ifndef WUWAOS_BLOCK_H
#define WUWAOS_BLOCK_H

#include <stdint.h>

/* Maximum wait for one submitted 512-byte request, not an entire file/page. */
#define BLOCK_IO_TIMEOUT_MS 5000UL

int block_init_from_fdt(const void *fdt);
unsigned int block_mmio_count(void);
uintptr_t block_mmio_base(unsigned int index);
int block_read(uint32_t sector, void *buffer);
int block_write(uint32_t sector, const void *buffer);
uint32_t block_sector_count(void);
int block_swap_read(uint32_t sector, void *buffer);
int block_swap_write(uint32_t sector, const void *buffer);
uint32_t block_swap_sector_count(void);

#endif
