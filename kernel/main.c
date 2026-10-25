#include "console.h"
#include "process.h"
#include "trap.h"
#include "vm.h"
#include "splash.h"
#include "block.h"
#include "fs.h"

void kernel_main(unsigned long hart_id, const void *fdt) {
    (void)hart_id;
    trap_init();
    boot_splash();
    console_puts("WUWAOS booting...\n");
    console_puts("[trap] Direct entry installed\n");
    uintptr_t block_base = 0;
    const int block_ready = block_init_from_fdt(fdt, &block_base) == 0;
    if (block_ready) {
        vm_map_mmio(block_base);
    }
    vm_init(fdt);
    if (block_ready && fs_init() < 0) {
        console_puts("[fs] Disk present but WUFS could not be mounted\n");
    }
    process_init();
    process_start();
}
