#ifndef WUWAOS_UI_GRAPHICS_PRIMITIVES_H
#define WUWAOS_UI_GRAPHICS_PRIMITIVES_H
#include <stdint.h>

void graphics_fill_rect(uint32_t *framebuffer, unsigned int x, unsigned int y,
                        unsigned int width, unsigned int height, uint32_t color);
void graphics_paint_wallpaper(uint32_t *framebuffer, unsigned int x, unsigned int y,
                              unsigned int width, unsigned int height);
void graphics_draw_glyph(uint32_t *framebuffer, unsigned int x, unsigned int y,
                         unsigned char character, uint32_t foreground, uint32_t background);
void graphics_draw_text(uint32_t *framebuffer, unsigned int x, unsigned int y,
                        const char *text, uint32_t foreground, uint32_t background);
void graphics_draw_logo(uint32_t *framebuffer);
void graphics_draw_terminal_icon(uint32_t *framebuffer);

#endif
