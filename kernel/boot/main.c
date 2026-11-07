#include "kernel/io/console.h"
#include "kernel/proc/process.h"
#include "arch/riscv/trap.h"
#include "kernel/mm/vm.h"
#include "ui/boot/splash.h"
#include "drivers/virtio/block.h"
#include "kernel/fs/fs.h"
#include "arch/riscv/timer.h"
#include "drivers/virtio/gpu.h"
#include "ui/desktop/desktop.h"
#include "drivers/virtio/input.h"
#include "kernel/io/tty.h"
#include "kernel/proc/event_hooks.h"

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
    const int input_found = input_probe(fdt) == 0;
    if (input_found) {
        for (unsigned int i = 0; i < input_device_count(); ++i) {
            if (input_mmio_base(i)) vm_map_mmio(input_mmio_base(i));
        }
    }
    vm_init(fdt);
    const int desktop_ready = gpu_found && gpu_init() == 0;
    input_set_pointer_bounds(GPU_WIDTH - 1U, GPU_HEIGHT - 1U);
    const int tablet_ready = input_found && input_init() == 0;
    if (desktop_ready) desktop_draw();
    tty_init();
    if (block_ready && fs_init() < 0) {
        console_puts("[fs] Disk present but WUFS could not be mounted\n");
    }
    timer_init();
    process_init();
    if (desktop_ready && tablet_ready) {
        process_register_ui_events(desktop_poll, desktop_event_source_active);
        desktop_start_cursor();
    }
    process_start();
}
