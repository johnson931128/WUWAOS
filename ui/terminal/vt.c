#include "ui/terminal/vt.h"

void terminal_model_init(TerminalModel *terminal) {
    terminal->row = terminal->column = terminal->saved_row = terminal->saved_column = 0;
    terminal->parser = terminal->parameter_count = 0;
    terminal->private_mode = terminal->wrap_pending = terminal->bold = terminal->reverse_video = 0;
    terminal->saved_bold = terminal->saved_reverse_video = 0;
    terminal->foreground = terminal->saved_foreground = 7U;
    terminal->background = terminal->saved_background = 0U;
    terminal->cells = terminal->normal;
    terminal->saved_cells = terminal->normal;
    terminal->cursor_enabled = 1;
    for (unsigned int i = 0; i < TERMINAL_COLUMNS * TERMINAL_ROWS; ++i) {
        terminal->normal[i] = (TerminalCell){ ' ', 0, 7, 0 };
        terminal->alternate[i] = (TerminalCell){ ' ', 0, 7, 0 };
    }
}

static TerminalCell *cell_at(TerminalModel *terminal, unsigned int row, unsigned int column) {
    return &terminal->cells[row * TERMINAL_COLUMNS + column];
}

static TerminalCell current_blank(TerminalModel *terminal, unsigned char character) {
    const unsigned int foreground = terminal->reverse_video ? terminal->background : terminal->foreground;
    const unsigned int background = terminal->reverse_video ? terminal->foreground : terminal->background;
    return (TerminalCell){ character, (uint8_t)terminal->bold, (uint8_t)foreground, (uint8_t)background };
}

static void terminal_clear_line(TerminalModel *terminal, unsigned int row, unsigned int from, unsigned int to) {
    if (row >= TERMINAL_ROWS || from >= TERMINAL_COLUMNS) return;
    if (to >= TERMINAL_COLUMNS) to = TERMINAL_COLUMNS - 1U;
    for (unsigned int column = from; column <= to; ++column)
        *cell_at(terminal, row, column) = current_blank(terminal, ' ');
}

static void terminal_scroll(TerminalModel *terminal) {
    const unsigned int cells = TERMINAL_COLUMNS * (TERMINAL_ROWS - 1U);
    for (unsigned int i = 0; i < cells; ++i) terminal->cells[i] = terminal->cells[i + TERMINAL_COLUMNS];
    terminal_clear_line(terminal, TERMINAL_ROWS - 1U, 0, TERMINAL_COLUMNS - 1U);
}

static void terminal_line_feed(TerminalModel *terminal) {
    terminal->wrap_pending = 0;
    terminal->column = 0;
    if (terminal->row + 1U < TERMINAL_ROWS) ++terminal->row;
    else terminal_scroll(terminal);
}

static void terminal_put_printable(TerminalModel *terminal, unsigned char character) {
    if (terminal->wrap_pending) terminal_line_feed(terminal);
    *cell_at(terminal, terminal->row, terminal->column) = current_blank(terminal, character);
    if (terminal->column + 1U == TERMINAL_COLUMNS) terminal->wrap_pending = 1;
    else ++terminal->column;
}

static unsigned int parameter(TerminalModel *terminal, unsigned int index, unsigned int fallback) {
    if (index >= terminal->parameter_count || terminal->parameters[index] == 0) return fallback;
    return terminal->parameters[index];
}

static void terminal_erase_display(TerminalModel *terminal, unsigned int mode) {
    if (mode == 2U) {
        for (unsigned int row = 0; row < TERMINAL_ROWS; ++row) terminal_clear_line(terminal, row, 0, TERMINAL_COLUMNS - 1U);
    } else if (mode == 0U) {
        terminal_clear_line(terminal, terminal->row, terminal->column, TERMINAL_COLUMNS - 1U);
        for (unsigned int row = terminal->row + 1U; row < TERMINAL_ROWS; ++row) terminal_clear_line(terminal, row, 0, TERMINAL_COLUMNS - 1U);
    } else if (mode == 1U) {
        for (unsigned int row = 0; row < terminal->row; ++row) terminal_clear_line(terminal, row, 0, TERMINAL_COLUMNS - 1U);
        terminal_clear_line(terminal, terminal->row, 0, terminal->column);
    }
}

static void terminal_csi(TerminalModel *terminal, unsigned char command) {
    const unsigned int n = parameter(terminal, 0, 1U);
    if (terminal->private_mode) {
        if (command == 'h' && terminal->parameters[0] == 25U) terminal->cursor_enabled = 1;
        else if (command == 'l' && terminal->parameters[0] == 25U) terminal->cursor_enabled = 0;
        else if (terminal->parameter_count == 1U && terminal->parameters[0] == 1049U &&
                 command == 'h' && terminal->cells != terminal->alternate) {
            terminal->saved_cells = terminal->cells;
            terminal->saved_row = terminal->row; terminal->saved_column = terminal->column;
            terminal->saved_foreground = terminal->foreground; terminal->saved_background = terminal->background;
            terminal->saved_bold = terminal->bold; terminal->saved_reverse_video = terminal->reverse_video;
            terminal->cells = terminal->alternate;
            for (unsigned int i = 0; i < TERMINAL_COLUMNS * TERMINAL_ROWS; ++i)
                terminal->alternate[i] = (TerminalCell){ ' ', 0, 7, 0 };
            terminal->row = terminal->column = 0; terminal->foreground = 7; terminal->background = 0;
            terminal->bold = terminal->reverse_video = 0;
            terminal->wrap_pending = 0;
        } else if (terminal->parameter_count == 1U && terminal->parameters[0] == 1049U &&
                   command == 'l' && terminal->cells == terminal->alternate) {
            terminal->cells = terminal->saved_cells;
            terminal->row = terminal->saved_row; terminal->column = terminal->saved_column;
            terminal->foreground = terminal->saved_foreground; terminal->background = terminal->saved_background;
            terminal->bold = terminal->saved_bold; terminal->reverse_video = terminal->saved_reverse_video;
            terminal->wrap_pending = 0;
        }
        return;
    }
    if (command == 'H' || command == 'f') {
        terminal->row = parameter(terminal, 0, 1U) - 1U;
        terminal->column = parameter(terminal, 1, 1U) - 1U;
        if (terminal->row >= TERMINAL_ROWS) terminal->row = TERMINAL_ROWS - 1U;
        if (terminal->column >= TERMINAL_COLUMNS) terminal->column = TERMINAL_COLUMNS - 1U;
        terminal->wrap_pending = 0;
    } else if (command == 'A') terminal->row = terminal->row > n ? terminal->row - n : 0;
    else if (command == 'B') { terminal->row += n; if (terminal->row >= TERMINAL_ROWS) terminal->row = TERMINAL_ROWS - 1U; }
    else if (command == 'C') { terminal->column += n; if (terminal->column >= TERMINAL_COLUMNS) terminal->column = TERMINAL_COLUMNS - 1U; }
    else if (command == 'D') terminal->column = terminal->column > n ? terminal->column - n : 0;
    else if (command == 'J') terminal_erase_display(terminal, terminal->parameters[0]);
    else if (command == 'K') {
        if (terminal->parameters[0] == 0U) terminal_clear_line(terminal, terminal->row, terminal->column, TERMINAL_COLUMNS - 1U);
        else if (terminal->parameters[0] == 1U) terminal_clear_line(terminal, terminal->row, 0, terminal->column);
        else if (terminal->parameters[0] == 2U) terminal_clear_line(terminal, terminal->row, 0, TERMINAL_COLUMNS - 1U);
    } else if (command == 'm') {
        if (terminal->parameter_count == 0U) terminal->parameters[terminal->parameter_count++] = 0U;
        for (unsigned int i = 0; i < terminal->parameter_count; ++i) {
            const unsigned int value = terminal->parameters[i];
            if (value == 0U) { terminal->foreground = 7U; terminal->background = 0U; terminal->bold = 0; terminal->reverse_video = 0; }
            else if (value == 1U) terminal->bold = 1;
            else if (value == 22U) terminal->bold = 0;
            else if (value == 7U) terminal->reverse_video = 1;
            else if (value == 27U) terminal->reverse_video = 0;
            else if (value == 39U) terminal->foreground = 7U;
            else if (value == 49U) terminal->background = 0U;
            else if (value >= 30U && value <= 37U) terminal->foreground = value - 30U;
            else if (value >= 40U && value <= 47U) terminal->background = value - 40U;
            else if (value >= 90U && value <= 97U) terminal->foreground = value - 90U + 8U;
            else if (value >= 100U && value <= 107U) terminal->background = value - 100U + 8U;
        }
    }
}

void terminal_model_feed(TerminalModel *terminal, unsigned char byte) {
    if (terminal->parser == 0U) {
        if (byte == 0x1bU) { terminal->parser = 1U; return; }
        if (byte == '\r') { terminal->column = 0; terminal->wrap_pending = 0; return; }
        if (byte == '\n') { terminal_line_feed(terminal); return; }
        if (byte == '\b' || byte == 127U) {
            if (terminal->wrap_pending) terminal->wrap_pending = 0;
            else if (terminal->column) --terminal->column;
            return;
        }
        if (byte == '\t') {
            const unsigned int target = (terminal->column + 8U) & ~7U;
            do { terminal_put_printable(terminal, ' '); } while (!terminal->wrap_pending && terminal->column < target);
            return;
        }
        if (byte >= 32U && byte <= 126U) terminal_put_printable(terminal, byte);
    } else if (terminal->parser == 1U) {
        if (byte == '[') {
            terminal->parser = 2U; terminal->parameter_count = 1;
            for (unsigned int i = 0; i < 8U; ++i) terminal->parameters[i] = 0;
            terminal->private_mode = 0;
        } else terminal->parser = 0;
    } else {
        if (byte == '?' && terminal->parameter_count == 1U && terminal->parameters[0] == 0U) terminal->private_mode = 1;
        else if (byte >= '0' && byte <= '9' && terminal->parameter_count > 0U) {
            const unsigned int index = terminal->parameter_count - 1U;
            if (index < 8U && terminal->parameters[index] < 999U)
                terminal->parameters[index] = terminal->parameters[index] * 10U + (byte - '0');
        } else if (byte == ';') {
            if (terminal->parameter_count < 8U) ++terminal->parameter_count;
        } else if (byte >= 0x40U && byte <= 0x7eU) {
            terminal_csi(terminal, byte);
            terminal->parser = 0;
        } else if (byte == 0x1bU) terminal->parser = 1U;
        else if (byte < 0x20U) { /* Ignore controls inside an incomplete CSI. */ }
        else terminal->parser = 0;
    }
}
