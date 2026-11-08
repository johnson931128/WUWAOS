#ifndef WUWAOS_UI_TERMINAL_RENDERER_H
#define WUWAOS_UI_TERMINAL_RENDERER_H
#include <stdint.h>
#include "ui/terminal/vt.h"

void terminal_renderer_draw(uint32_t *framebuffer, const TerminalModel *terminal,
                            unsigned int x, unsigned int y, int show_cursor);
void terminal_renderer_draw_viewport(uint32_t *framebuffer, const TerminalModel *terminal,
                                     unsigned int x, unsigned int y,
                                     unsigned int viewport_width, unsigned int viewport_height,
                                     int show_cursor);

#endif
