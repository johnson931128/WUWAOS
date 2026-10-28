#ifndef WUWAOS_PAGE_H
#define WUWAOS_PAGE_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096UL
/* Board contract: QEMU virt -m 128M, matching Makefile and linker.ld. */
#define RAM_BASE 0x80000000UL
#define RAM_END 0x88000000UL

void page_init(const void *fdt);
uintptr_t page_alloc(void);
void page_free(uintptr_t address);
void page_print_stats(void);
void page_get_stats(size_t *free_pages, size_t *pool_pages);

#endif
