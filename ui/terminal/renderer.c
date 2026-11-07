#include "ui/terminal/renderer.h"
#include "ui/assets/font_terminal.h"
#include "ui/graphics/primitives.h"

static const uint32_t palette[16] = {
    0x00131b25U, 0x00cc5555U, 0x004caf50U, 0x00c89b38U,
    0x005b8dd9U, 0x00b25cc7U, 0x003ca6a6U, 0x00d8e0e8U,
    0x00637482U, 0x00ff7777U, 0x0076d67aU, 0x00f2c66dU,
    0x007ba8f2U, 0x00d58be8U, 0x0061ceceU, 0x00ffffffU
};

void terminal_renderer_draw(uint32_t *framebuffer, const TerminalModel *terminal,
                            unsigned int x, unsigned int y, int show_cursor) {
    if (!framebuffer || !terminal) return;
    for (unsigned int row = 0; row < TERMINAL_ROWS; ++row) {
        for (unsigned int column = 0; column < TERMINAL_COLUMNS; ++column) {
            const TerminalCell *cell = &terminal->cells[row * TERMINAL_COLUMNS + column];
            uint32_t foreground = palette[cell->color_fg & 15U];
            const uint32_t background = palette[cell->color_bg & 15U];
            if (cell->bold && cell->color_fg < 8U) foreground = palette[cell->color_fg + 8U];
            graphics_draw_glyph(framebuffer, x + column * TERMINAL_FONT_WIDTH,
                y + row * TERMINAL_FONT_HEIGHT, cell->character, foreground, background);
        }
    }
    if (show_cursor && terminal->cursor_enabled) {
        unsigned int column = terminal->column;
        if (terminal->wrap_pending) column = TERMINAL_COLUMNS - 1U;
        const unsigned int py = y + terminal->row * TERMINAL_FONT_HEIGHT + TERMINAL_FONT_HEIGHT - 2U;
        graphics_fill_rect(framebuffer, x + column * TERMINAL_FONT_WIDTH, py, TERMINAL_FONT_WIDTH, 2U, 0x00f4f7faU);
    }
}
