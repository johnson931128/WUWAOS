#include "desktop.h"
#include "gpu.h"
#include "input.h"
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
#define CURSOR_WIDTH 16U
#define CURSOR_HEIGHT 16U

/* Five-bit rows for W, U, A, O and S, shared conceptually with the UART splash. */
static const uint8_t glyphs[5][GLYPH_HEIGHT] = {
    { 17, 17, 17, 21, 21, 21, 10 },
    { 17, 17, 17, 17, 17, 17, 14 },
    { 14, 17, 17, 31, 17, 17, 17 },
    { 14, 17, 17, 17, 17, 17, 14 },
    { 15, 16, 16, 14,  1,  1, 30 }
};
static const uint8_t letters[LETTERS] = { 0, 1, 0, 2, 3, 4 };
static const char cursor[CURSOR_HEIGHT][CURSOR_WIDTH + 1U] = {
    "X...............", "XX..............", "XWX.............", "XWWX............",
    "XWWWX...........", "XWWWWX..........", "XWWWWWX.........", "XWWWWWWX........",
    "XWWWWWWWX.......", "XWWXWWWWX.......", "XWX.XWWX........", "XX..XWWX........",
    "....XWWX........", ".....XWWX.......", "......XX........", "................"
};
static uint32_t cursor_saved[CURSOR_HEIGHT][CURSOR_WIDTH];
static uint32_t cursor_x, cursor_y;
static int cursor_visible;

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

static void draw_cursor(void) {
    uint32_t *framebuffer = gpu_framebuffer();
    for (unsigned int y = 0; y < CURSOR_HEIGHT; ++y) {
        for (unsigned int x = 0; x < CURSOR_WIDTH; ++x) {
            const size_t index = (cursor_y + y) * GPU_WIDTH + cursor_x + x;
            cursor_saved[y][x] = framebuffer[index];
            if (cursor[y][x] == 'X') framebuffer[index] = 0x00000000U;
            else if (cursor[y][x] == 'W') framebuffer[index] = 0x00ffffffU;
        }
    }
}

static void move_cursor(uint32_t x, uint32_t y) {
    if (x > GPU_WIDTH - CURSOR_WIDTH) x = GPU_WIDTH - CURSOR_WIDTH;
    if (y > GPU_HEIGHT - CURSOR_HEIGHT) y = GPU_HEIGHT - CURSOR_HEIGHT;
    if (cursor_visible && x == cursor_x && y == cursor_y) return;
    uint32_t old_x = cursor_x, old_y = cursor_y;
    uint32_t *framebuffer = gpu_framebuffer();
    if (cursor_visible) {
        for (unsigned int row = 0; row < CURSOR_HEIGHT; ++row)
            for (unsigned int column = 0; column < CURSOR_WIDTH; ++column)
                framebuffer[(old_y + row) * GPU_WIDTH + old_x + column] = cursor_saved[row][column];
    }
    cursor_x = x; cursor_y = y; draw_cursor(); cursor_visible = 1;
    uint32_t left = x, top = y, right = x + CURSOR_WIDTH, bottom = y + CURSOR_HEIGHT;
    if (old_x < left) left = old_x;
    if (old_y < top) top = old_y;
    if (old_x + CURSOR_WIDTH > right) right = old_x + CURSOR_WIDTH;
    if (old_y + CURSOR_HEIGHT > bottom) bottom = old_y + CURSOR_HEIGHT;
    (void)gpu_present(left, top, right - left, bottom - top);
}

void desktop_start_cursor(void) {
    move_cursor(GPU_WIDTH / 2U, GPU_HEIGHT / 2U);
}

void desktop_poll(void) {
    uint32_t x, y;
    if (input_poll(&x, &y) > 0) move_cursor(x, y);
}
