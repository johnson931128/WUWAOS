#include "desktop.h"
#include "gpu.h"
#include <stddef.h>
#include <stdint.h>

#define LOGO_SCALE 12U
#define GLYPH_WIDTH 5U
#define GLYPH_HEIGHT 7U
#define LETTER_GAP 2U
#define LETTERS 6U
#define LETTER_STRIDE (GLYPH_WIDTH + LETTER_GAP)
#define LOGO_WIDTH ((LETTERS * LETTER_STRIDE - LETTER_GAP) * LOGO_SCALE)
#define LOGO_HEIGHT (GLYPH_HEIGHT * LOGO_SCALE)
#define LOGO_X ((GPU_WIDTH - LOGO_WIDTH) / 2U)
#define LOGO_Y ((GPU_HEIGHT - LOGO_HEIGHT) / 2U)
#define DESKTOP_PIXEL 0x00b8dcf5U
#define LOGO_PIXEL 0x00ffffffU
#define SHADOW_PIXEL 0x00806450U

/* Five-bit rows for W, U, A, O and S, shared conceptually with the UART splash. */
static const uint8_t glyphs[5][GLYPH_HEIGHT] = {
    { 17, 17, 17, 21, 21, 21, 10 },
    { 17, 17, 17, 17, 17, 17, 14 },
    { 14, 17, 17, 31, 17, 17, 17 },
    { 14, 17, 17, 17, 17, 17, 14 },
    { 15, 16, 16, 14,  1,  1, 30 }
};
static const uint8_t letters[LETTERS] = { 0, 1, 0, 2, 3, 4 };

static int glyph_pixel(unsigned int x, unsigned int y) {
    const unsigned int letter_column = x / (LETTER_STRIDE * LOGO_SCALE);
    const unsigned int within_letter = (x / LOGO_SCALE) % LETTER_STRIDE;
    if (letter_column >= LETTERS || within_letter >= GLYPH_WIDTH) return 0;
    const uint8_t row = glyphs[letters[letter_column]][y / LOGO_SCALE];
    return (row >> (GLYPH_WIDTH - 1U - within_letter)) & 1U;
}

void desktop_draw(void) {
    uint32_t *framebuffer = gpu_framebuffer();
    if (!framebuffer) return;
    for (size_t index = 0; index < GPU_WIDTH * GPU_HEIGHT; ++index) framebuffer[index] = DESKTOP_PIXEL;
    /* A one-pixel offset gives the centered white wordmark a subtle dark edge. */
    for (unsigned int y = 0; y < LOGO_HEIGHT; ++y) {
        for (unsigned int x = 0; x < LOGO_WIDTH; ++x) {
            if (glyph_pixel(x, y)) framebuffer[(LOGO_Y + y + 1U) * GPU_WIDTH + LOGO_X + x + 1U] = SHADOW_PIXEL;
        }
    }
    for (unsigned int y = 0; y < LOGO_HEIGHT; ++y) {
        for (unsigned int x = 0; x < LOGO_WIDTH; ++x) {
            if (glyph_pixel(x, y)) framebuffer[(LOGO_Y + y) * GPU_WIDTH + LOGO_X + x] = LOGO_PIXEL;
        }
    }
    (void)gpu_present(0, 0, GPU_WIDTH, GPU_HEIGHT);
}
