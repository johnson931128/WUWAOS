#include "console.h"
#include "process.h"
#include "trap.h"
#include "vm.h"
#include "splash.h"
#include "block.h"
#include "fs.h"
#include "timer.h"
#include "gpu.h"

void kernel_main(unsigned long hart_id, const void *fdt) {
    (void)hart_id;
    trap_init();
    boot_splash();
    console_puts("WUWAOS booting...\n");
    console_puts("[trap] Direct entry installed\n");
    const int block_ready = block_init_from_fdt(fdt) == 0;
    for (unsigned int i = 0; i < block_mmio_count(); ++i) vm_map_mmio(block_mmio_base(i));
    const int gpu_found = gpu_probe(fdt) == 0;
    if (gpu_found) vm_map_mmio(gpu_mmio_base());
    vm_init(fdt);
    if (gpu_found) (void)gpu_init();
    if (block_ready && fs_init() < 0) {
        console_puts("[fs] Disk present but WUFS could not be mounted\n");
    }
    timer_init();
    process_init();
    process_start();
}
