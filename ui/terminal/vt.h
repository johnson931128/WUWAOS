#ifndef WUWAOS_UI_TERMINAL_VT_H
#define WUWAOS_UI_TERMINAL_VT_H
#include <stdint.h>

#define TERMINAL_COLUMNS 100U
#define TERMINAL_ROWS 30U

typedef struct {
    uint8_t character, bold, color_fg, color_bg;
} TerminalCell;

typedef struct {
    TerminalCell normal[TERMINAL_COLUMNS * TERMINAL_ROWS];
    TerminalCell alternate[TERMINAL_COLUMNS * TERMINAL_ROWS];
    TerminalCell *cells;
    TerminalCell *saved_cells;
    unsigned int row, column, saved_row, saved_column;
    unsigned int parser, parameter_count, parameters[8];
    int private_mode, wrap_pending, cursor_enabled, bold, reverse_video;
    unsigned int foreground, background;
    unsigned int saved_foreground, saved_background;
    int saved_bold, saved_reverse_video;
} TerminalModel;

void terminal_model_init(TerminalModel *terminal);
void terminal_model_feed(TerminalModel *terminal, unsigned char byte);

#endif
