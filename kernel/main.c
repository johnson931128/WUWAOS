#include "console.h"
#include "process.h"
#include "trap.h"
#include "vm.h"

void kernel_main(void) {
    console_puts("WUWAOS booting...\n");
    trap_init();
    console_puts("[trap] Direct entry installed\n");
    const uintptr_t page_table = vm_init();
    process_init(page_table);
    process_start();
}
