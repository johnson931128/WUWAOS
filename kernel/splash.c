#include "splash.h"
#include "console.h"
#include "clock.h"
#include <stdint.h>

#define LOGO_HEIGHT 7
#define GLYPH_WIDTH 5
#define PIXEL_WIDTH 2
#define LETTER_GAP 2
#define LETTER_COUNT 6
#define LETTER_STRIDE (GLYPH_WIDTH * PIXEL_WIDTH + LETTER_GAP)
#define LOGO_WIDTH (LETTER_COUNT * LETTER_STRIDE - LETTER_GAP)
#define LEFT_MARGIN ((80 - LOGO_WIDTH) / 2)

/* Five-bit rows for W, U, A, O, S. Color two spaces per pixel: no UTF-8 font dependency. */
static const uint8_t glyphs[5][LOGO_HEIGHT] = {
    { 17, 17, 17, 21, 21, 21, 10 },
    { 17, 17, 17, 17, 17, 17, 14 },
    { 14, 17, 17, 31, 17, 17, 17 },
    { 14, 17, 17, 17, 17, 17, 14 },
    { 15, 16, 16, 14,  1,  1, 30 }
};
static const uint8_t letters[LETTER_COUNT] = { 0, 1, 0, 2, 3, 4 };
static const char *const row_colors[LOGO_HEIGHT] = {
    "\033[48;5;154m", "\033[48;5;118m", "\033[48;5;82m", "\033[48;5;46m",
    "\033[48;5;47m", "\033[48;5;48m", "\033[48;5;49m"
};

static int logo_pixel(int row, int column) {
    const int within = column % LETTER_STRIDE;
    if (within >= GLYPH_WIDTH * PIXEL_WIDTH) return 0;
    const uint8_t bits = glyphs[letters[column / LETTER_STRIDE]][row];
    return (bits >> (GLYPH_WIDTH - 1 - within / PIXEL_WIDTH)) & 1;
}

static void pause_ms(unsigned long milliseconds) {
    const uint64_t start = clock_ticks();
    const uint64_t duration = clock_ms_ticks(milliseconds);
    while (clock_ticks() - start < duration) {
    }
}

void boot_splash(void) {
    /* Clear the firmware banner, hide the cursor, then redraw in place. */
    console_puts("\033[0m\033[2J\033[H\033[?25l");
    for (int radius = 1; radius <= LOGO_WIDTH / 2; ++radius) {
        const int left = LOGO_WIDTH / 2 - radius;
        const int right = LOGO_WIDTH / 2 + radius;
        console_puts("\033[H\n");
        for (int row = 0; row < LOGO_HEIGHT; ++row) {
            for (int margin = 0; margin < LEFT_MARGIN; ++margin) {
                console_putc(' ');
            }
            int painted = 0;
            for (int column = 0; column < LOGO_WIDTH; ++column) {
                const int filled = column >= left && column < right && logo_pixel(row, column);
                if (filled != painted) {
                    console_puts(filled ? row_colors[row] : "\033[0m");
                    painted = filled;
                }
                console_putc(' ');
            }
            console_puts("\033[0m");
            console_putc('\n');
        }
        pause_ms(32);
    }
    pause_ms(200);
    console_puts("\n     \033[38;5;120mRISC-V 64-bit\033[0m / a small operating system\n\n\033[0m\033[?25h");
}
