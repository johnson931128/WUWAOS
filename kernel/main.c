#include "console.h"
#include "process.h"
#include "trap.h"
#include "vm.h"
#include "splash.h"
#include "block.h"
#include "fs.h"
#include "timer.h"
#include "gpu.h"
#include "desktop.h"
#include "input.h"
#include "irq.h"

void kernel_main(unsigned long hart_id, const void *fdt) {
    trap_init();
    boot_splash();
    console_puts("WUWAOS booting...\n");
    console_puts("[trap] Direct entry installed\n");
    const int irq_ready = irq_probe(fdt, hart_id) == 0;
    for (unsigned int i = 0; i < irq_mmio_count(); ++i) vm_map_mmio(irq_mmio_base(i));
    const int block_ready = block_init_from_fdt(fdt) == 0;
    for (unsigned int i = 0; i < block_mmio_count(); ++i) vm_map_mmio(block_mmio_base(i));
    const int gpu_found = gpu_probe(fdt) == 0;
    if (gpu_found) vm_map_mmio(gpu_mmio_base());
    const int input_found = input_probe(fdt) == 0;
    if (input_found) {
        for (unsigned int i = 0; i < input_device_count(); ++i) {
            if (input_mmio_base(i)) vm_map_mmio(input_mmio_base(i));
        }
    }
    vm_init(fdt);
    const int desktop_ready = gpu_found && gpu_init() == 0;
    const int tablet_ready = input_found && input_init() == 0;
    if (desktop_ready) desktop_draw();
    if (block_ready && fs_init() < 0) {
        console_puts("[fs] Disk present but WUFS could not be mounted\n");
    }
    timer_init();
    process_init();
    if (irq_ready) {
        /* Register only initialized disks; SEIE is enabled after callbacks exist. */
        block_register_irqs();
        irq_start();
    }
    if (desktop_ready && tablet_ready) desktop_start_cursor();
    process_start();
}
