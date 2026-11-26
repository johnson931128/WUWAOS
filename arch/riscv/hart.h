#ifndef WUWAOS_HART_H
#define WUWAOS_HART_H

#define HART_LIMIT 8
#define HART_BOOT_STACK 16384
#ifndef __ASSEMBLER__
#include <stdint.h>

static inline unsigned hart_id(void) {
#ifdef __riscv
    unsigned long id;
    __asm__ volatile("mv %0, tp" : "=r"(id));
    return (unsigned)id;
#else
    return 0;
#endif
}

void hart_start_others(const void *fdt);
void kernel_lock_acquire(void);
void kernel_lock_release(void);
int kernel_lock_held(void);
void kernel_lock_print_stats(void);
unsigned long hart_online_mask(void);
void hart_invalidate_remote_tlbs(void);
#endif
#endif
