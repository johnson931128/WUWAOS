#include "page.h"
#include "console.h"

typedef struct FreePage {
    struct FreePage *next;
} FreePage;

static FreePage *free_head;
static uintptr_t pool_start, pool_end;
static size_t free_count, total_count;
static uint8_t allocated[(RAM_END - RAM_BASE) / PAGE_SIZE / 8];
extern unsigned char __kernel_end[];

static uint32_t big_endian(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static _Noreturn void page_panic(const char *message) {
    console_puts("[page] ");
    console_puts(message);
    console_putc('\n');
    kernel_idle();
}

void page_init(const void *fdt) {
    const uintptr_t address = (uintptr_t)fdt;
    pool_start = ((uintptr_t)__kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (address < pool_start || address > RAM_END - 40) {
        page_panic("FDT outside the supported 128 MiB board layout");
    }
    const uint8_t *header = fdt;
    const uint32_t length = big_endian(header + 4);
    if (big_endian(header) != 0xd00dfeedU || length < 40 || length > RAM_END - address) {
        page_panic("Invalid FDT header or extent");
    }
    /* Conservatively reserve the FDT and everything above it, not just its bytes. */
    pool_end = address & ~(PAGE_SIZE - 1);
    if (pool_end <= pool_start) page_panic("No allocatable RAM below FDT");
    for (uintptr_t page = pool_end; page > pool_start;) {
        page -= PAGE_SIZE;
        FreePage *node = (FreePage *)page;
        node->next = free_head;
        free_head = node;
        ++free_count;
    }
    total_count = free_count;
    console_puts("[page] Free RAM ");
    console_hex(pool_start);
    console_puts("..");
    console_hex(pool_end);
    console_puts(" (FDT/top RAM reserved)\n");
}

uintptr_t page_alloc(void) {
    if (free_head == 0) return 0;
    const uintptr_t address = (uintptr_t)free_head;
    free_head = free_head->next;
    const size_t index = (address - RAM_BASE) / PAGE_SIZE;
    allocated[index / 8] |= (uint8_t)(1U << (index % 8));
    --free_count;
    volatile uint8_t *bytes = (volatile uint8_t *)address;
    for (size_t i = 0; i < PAGE_SIZE; ++i) bytes[i] = 0;
    return address;
}

void page_free(uintptr_t address) {
    if ((address & (PAGE_SIZE - 1)) != 0 || address < pool_start || address >= pool_end) {
        page_panic("Invalid page free");
    }
    const size_t index = (address - RAM_BASE) / PAGE_SIZE;
    const uint8_t mask = (uint8_t)(1U << (index % 8));
    if ((allocated[index / 8] & mask) == 0) page_panic("Double page free");
    allocated[index / 8] &= (uint8_t)~mask;
    FreePage *node = (FreePage *)address;
    node->next = free_head;
    free_head = node;
    ++free_count;
}

void page_print_stats(void) {
    console_puts("Memory pages: free=");
    console_number((long)free_count);
    console_puts(" total=");
    console_number((long)total_count);
    console_puts(" page_size=4096\n");
}

void page_get_stats(size_t *free_pages, size_t *pool_pages) {
    *free_pages = free_count;
    *pool_pages = total_count;
}
