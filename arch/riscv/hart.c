#include "arch/riscv/hart.h"
#include "arch/riscv/fdt.h"
#include "arch/riscv/clock.h"
#include "arch/riscv/sbi.h"
#include "arch/riscv/timer.h"
#include "arch/riscv/trap.h"
#include "kernel/io/console.h"
#include "kernel/mm/vm.h"
#include "kernel/proc/process.h"

unsigned char hart_boot_stacks[HART_LIMIT][HART_BOOT_STACK] __attribute__((aligned(16)));
static unsigned long online_mask;
static unsigned kernel_lock;

extern void hart_secondary_entry(void);

void kernel_lock_acquire(void) {
    while (__atomic_exchange_n(&kernel_lock, 1, __ATOMIC_ACQUIRE)) {}
}

void kernel_lock_release(void) {
    __atomic_store_n(&kernel_lock, 0, __ATOMIC_RELEASE);
}

unsigned long hart_online_mask(void) {
    return __atomic_load_n(&online_mask, __ATOMIC_ACQUIRE);
}

void hart_invalidate_remote_tlbs(void) {
    unsigned long remote = hart_online_mask() & ~(1UL << hart_id());
    if (remote && sbi_remote_sfence(remote) != 0) {
        console_puts("[smp] SBI remote SFENCE failed\n");
        kernel_idle();
    }
}

void hart_start_others(const void *fdt) {
    __atomic_store_n(&online_mask, 1UL << hart_id(), __ATOMIC_RELEASE);
    if (!sbi_smp_available()) return;
    Fdt tree;
    if (fdt_open(&tree, fdt) < 0) return;
    for (unsigned i = 0; i < tree.count; ++i) {
        uint32_t length;
        const uint8_t *type = fdt_property(&tree, i, "device_type", &length);
        if (!type || length != 4 || type[0] != 'c' || type[1] != 'p' || type[2] != 'u' || type[3]) continue;
        uint64_t id, size;
        if (fdt_reg(&tree, i, &id, &size) < 0 || id >= HART_LIMIT || id == hart_id()) continue;
        if (sbi_hart_start(id, (uintptr_t)hart_secondary_entry) != 0) continue;
        const uint64_t deadline = clock_ticks() + clock_ms_ticks(1000);
        while (!(hart_online_mask() & (1UL << id)) && (int64_t)(clock_ticks() - deadline) < 0) {}
    }
    console_puts("[smp] Online hart mask="); console_hex(hart_online_mask()); console_putc('\n');
}

_Noreturn void hart_secondary_main(void) {
    trap_init();
    timer_init();
    __atomic_fetch_or(&online_mask, 1UL << hart_id(), __ATOMIC_RELEASE);
    kernel_lock_acquire();
    vm_use_kernel();
    process_start();
}
