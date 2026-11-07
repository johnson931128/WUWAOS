#include "ui/graphics/primitives.h"
#include "drivers/virtio/gpu.h"
#include "ui/assets/font_terminal.h"

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
#define ICON_X 20U
#define ICON_Y 18U

static const uint8_t logo_glyphs[5][GLYPH_HEIGHT] = {
    { 17, 17, 17, 21, 21, 21, 10 }, { 17, 17, 17, 17, 17, 17, 14 },
    { 14, 17, 17, 31, 17, 17, 17 }, { 14, 17, 17, 17, 17, 17, 14 },
    { 15, 16, 16, 14,  1,  1, 30 }
};
static const uint8_t logo_letters[LETTERS] = { 0, 1, 0, 2, 3, 4 };

static int logo_pixel(unsigned int x, unsigned int y) {
    const unsigned int letter_column = x / (LETTER_STRIDE * LOGO_SCALE);
    const unsigned int within_letter = (x / LOGO_SCALE) % LETTER_STRIDE;
    if (letter_column >= LETTERS || within_letter >= GLYPH_WIDTH) return 0;
    const uint8_t row = logo_glyphs[logo_letters[letter_column]][y / LOGO_SCALE];
    return (row >> (GLYPH_WIDTH - 1U - within_letter)) & 1U;
}

void graphics_fill_rect(uint32_t *framebuffer, unsigned int x, unsigned int y,
                        unsigned int width, unsigned int height, uint32_t color) {
    if (!framebuffer || x >= GPU_WIDTH || y >= GPU_HEIGHT) return;
    if (x + width > GPU_WIDTH) width = GPU_WIDTH - x;
    if (y + height > GPU_HEIGHT) height = GPU_HEIGHT - y;
    for (unsigned int row = 0; row < height; ++row) {
        uint32_t *pixel = framebuffer + (y + row) * GPU_WIDTH + x;
        for (unsigned int column = 0; column < width; ++column) pixel[column] = color;
    }
}

void graphics_paint_wallpaper(uint32_t *framebuffer, unsigned int x, unsigned int y,
                              unsigned int width, unsigned int height) {
    if (!framebuffer || x >= GPU_WIDTH || y >= GPU_HEIGHT) return;
    if (x + width > GPU_WIDTH) width = GPU_WIDTH - x;
    if (y + height > GPU_HEIGHT) height = GPU_HEIGHT - y;
    for (unsigned int row = 0; row < height; ++row) {
        for (unsigned int column = 0; column < width; ++column) {
            const unsigned int px = x + column, py = y + row;
            uint32_t color = DESKTOP_PIXEL;
            if (px >= LOGO_X && px < LOGO_X + LOGO_WIDTH && py >= LOGO_Y && py < LOGO_Y + LOGO_HEIGHT &&
                logo_pixel(px - LOGO_X, py - LOGO_Y)) color = LOGO_PIXEL;
            else if (px > LOGO_X && px <= LOGO_X + LOGO_WIDTH && py > LOGO_Y && py <= LOGO_Y + LOGO_HEIGHT &&
                     logo_pixel(px - LOGO_X - 1U, py - LOGO_Y - 1U)) color = SHADOW_PIXEL;
            framebuffer[py * GPU_WIDTH + px] = color;
        }
    }
}

static uint32_t blend_pixel(uint32_t foreground, uint32_t background, unsigned int coverage) {
    uint32_t pixel = 0;
    for (unsigned int shift = 0; shift < 24U; shift += 8U) {
        const unsigned int fg = (foreground >> shift) & 0xffU;
        const unsigned int bg = (background >> shift) & 0xffU;
        pixel |= ((fg * coverage + bg * (15U - coverage) + 7U) / 15U) << shift;
    }
    return pixel;
}

void graphics_draw_glyph(uint32_t *framebuffer, unsigned int x, unsigned int y,
                         unsigned char character, uint32_t foreground, uint32_t background) {
    if (!framebuffer || x + TERMINAL_FONT_WIDTH > GPU_WIDTH || y + TERMINAL_FONT_HEIGHT > GPU_HEIGHT) return;
    if (character < 0x20U || character > 0x7eU) character = '?';
    const uint8_t *glyph = terminal_font[character - 0x20U];
    uint32_t colors[16];
    for (unsigned int coverage = 0; coverage < 16U; ++coverage)
        colors[coverage] = blend_pixel(foreground, background, coverage);
    for (unsigned int row = 0; row < TERMINAL_FONT_HEIGHT; ++row) {
        for (unsigned int column = 0; column < TERMINAL_FONT_WIDTH; ++column) {
            const unsigned int sample = row * TERMINAL_FONT_WIDTH + column;
            const unsigned int packed = glyph[sample / 2U];
            const unsigned int coverage = (sample & 1U) ? (packed & 15U) : (packed >> 4U);
            framebuffer[(y + row) * GPU_WIDTH + x + column] = colors[coverage];
        }
    }
}

void graphics_draw_text(uint32_t *framebuffer, unsigned int x, unsigned int y,
                        const char *text, uint32_t foreground, uint32_t background) {
    while (text && *text) {
        graphics_draw_glyph(framebuffer, x, y, (unsigned char)*text++, foreground, background);
        x += TERMINAL_FONT_WIDTH;
    }
}

void graphics_draw_logo(uint32_t *framebuffer) {
    for (unsigned int y = 0; y < LOGO_HEIGHT; ++y)
        for (unsigned int x = 0; x < LOGO_WIDTH; ++x)
            if (logo_pixel(x, y)) framebuffer[(LOGO_Y + y + 1U) * GPU_WIDTH + LOGO_X + x + 1U] = SHADOW_PIXEL;
    for (unsigned int y = 0; y < LOGO_HEIGHT; ++y)
        for (unsigned int x = 0; x < LOGO_WIDTH; ++x)
            if (logo_pixel(x, y)) framebuffer[(LOGO_Y + y) * GPU_WIDTH + LOGO_X + x] = LOGO_PIXEL;
}

void graphics_draw_terminal_icon(uint32_t *framebuffer) {
    graphics_fill_rect(framebuffer, ICON_X + 4U, ICON_Y + 5U, 60U, 45U, 0x007d9ec0U);
    graphics_fill_rect(framebuffer, ICON_X + 8U, ICON_Y + 2U, 24U, 10U, 0x007d9ec0U);
    graphics_fill_rect(framebuffer, ICON_X + 1U, ICON_Y + 1U, 60U, 45U, 0x002f6fa3U);
    graphics_fill_rect(framebuffer, ICON_X + 5U, ICON_Y + 6U, 52U, 33U, 0x00101b28U);
    graphics_draw_glyph(framebuffer, ICON_X + 16U, ICON_Y + 14U, '>', 0x0078dbffU, 0x00101b28U);
    graphics_fill_rect(framebuffer, ICON_X + 34U, ICON_Y + 27U, 8U, 2U, 0x00d4e7f4U);
    graphics_fill_rect(framebuffer, ICON_X + 1U, ICON_Y + 47U, 60U, 3U, 0x00d7e2eaU);
    graphics_fill_rect(framebuffer, ICON_X + 17U, ICON_Y + 50U, 28U, 3U, 0x00b0bec7U);
    graphics_draw_text(framebuffer, ICON_X - 1U, ICON_Y + 58U, "Terminal", 0x001b3446U, DESKTOP_PIXEL);
}
