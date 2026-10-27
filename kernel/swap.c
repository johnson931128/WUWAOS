#include "swap.h"
#include "block.h"
#include "console.h"
#include "page.h"

#define MAX_SLOTS 4096U
#define SECTORS_PER_PAGE 8U
static uint8_t bitmap[MAX_SLOTS / 8];
static unsigned int slots, used;

void swap_init(void) {
    uint32_t header[128];
    if (block_swap_read(0, header) < 0) {
        console_puts("[swap] No dedicated swap disk; dirty eviction unavailable\n");
        return;
    }
    static const char magic[] = "WUSWAP01";
    for (unsigned int i = 0; i < 8; ++i) {
        if (((const char *)header)[i] != magic[i]) return;
    }
    if (block_swap_sector_count() < SECTORS_PER_PAGE ||
        header[2] != PAGE_SIZE || header[3] == 0 || header[3] > MAX_SLOTS ||
        header[3] > block_swap_sector_count() / SECTORS_PER_PAGE - 1) return;
    slots = header[3];
    console_puts("[swap] Dedicated disk, 4 KiB slots=");
    console_number(slots);
    console_putc('\n');
}

unsigned int swap_alloc(void) {
    for (unsigned int i = 0; i < slots; ++i) {
        const uint8_t mask = (uint8_t)(1U << (i % 8));
        if ((bitmap[i / 8] & mask) == 0) {
            bitmap[i / 8] |= mask;
            ++used;
            return i + 1;
        }
    }
    return 0;
}

void swap_free(unsigned int slot) {
    if (slot == 0 || slot > slots || !(bitmap[(slot - 1) / 8] & (1U << ((slot - 1) % 8)))) {
        console_puts("[swap] Invalid slot release\n");
        kernel_idle();
    }
    --slot;
    bitmap[slot / 8] &= (uint8_t)~(1U << (slot % 8));
    --used;
}

static int transfer(unsigned int slot, uintptr_t page, int write) {
    if (slot == 0 || slot > slots || (page & (PAGE_SIZE - 1)) != 0 ||
        !(bitmap[(slot - 1) / 8] & (1U << ((slot - 1) % 8)))) return -1;
    for (unsigned int i = 0; i < SECTORS_PER_PAGE; ++i) {
        const uint32_t sector = SECTORS_PER_PAGE * slot + i;
        void *buffer = (void *)(page + i * 512U);
        if ((write ? block_swap_write(sector, buffer) : block_swap_read(sector, buffer)) < 0) return -1;
    }
    return 0;
}

int swap_read(unsigned int slot, uintptr_t page) { return transfer(slot, page, 0); }
int swap_write(unsigned int slot, uintptr_t page) { return transfer(slot, page, 1); }
unsigned int swap_used(void) { return used; }
unsigned int swap_total(void) { return slots; }
