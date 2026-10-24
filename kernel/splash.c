#include "splash.h"
#include "console.h"
#include <stdint.h>

#define LOGO_HEIGHT 7
#define LOGO_WIDTH 52
#define LEFT_MARGIN 14
#define TIMEBASE_HZ 10000000UL /* QEMU virt, without a DTB parser yet. */

static const char logo[LOGO_HEIGHT][LOGO_WIDTH + 1] = {
    "#     #  #     #  #     #    ###     #####    ######",
    "#     #  #     #  #     #   #   #   #     #  #      ",
    "#     #  #     #  #     #  #     #  #     #  #      ",
    "#  #  #  #     #  #  #  #  #######  #     #   ##### ",
    "#  #  #  #     #  #  #  #  #     #  #     #        #",
    " # # #   #     #   # # #   #     #  #     #        #",
    "  # #     #####     # #    #     #   #####   ###### "
};

static uint64_t hardware_time(void) {
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

static void pause_ms(unsigned long milliseconds) {
    const uint64_t start = hardware_time();
    const uint64_t duration = (TIMEBASE_HZ / 1000) * milliseconds;
    while (hardware_time() - start < duration) {
    }
}

void boot_splash(void) {
    /* Clear the firmware banner, hide the cursor, then redraw in place. */
    console_puts("\033[2J\033[H\033[?25l");
    for (int radius = 1; radius <= LOGO_WIDTH / 2; ++radius) {
        const int left = LOGO_WIDTH / 2 - radius;
        const int right = LOGO_WIDTH / 2 + radius;
        console_puts("\033[H\n");
        for (int row = 0; row < LOGO_HEIGHT; ++row) {
            for (int margin = 0; margin < LEFT_MARGIN; ++margin) {
                console_putc(' ');
            }
            for (int column = 0; column < LOGO_WIDTH; ++column) {
                console_putc(column >= left && column < right ? logo[row][column] : ' ');
            }
            console_putc('\n');
        }
        pause_ms(45);
    }
    pause_ms(200);
    console_puts("\n              RISC-V 64-bit / a small operating system\n\n\033[?25h");
}
