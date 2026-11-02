#include "desktop.h"
#include "gpu.h"
#include "input.h"
#include "process.h"
#include "font8x8_basic.h"
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
#define TERMINAL_COLUMNS 100U
#define TERMINAL_ROWS 30U
#define CELL_WIDTH 8U
#define CELL_HEIGHT 16U
#define WINDOW_X ((GPU_WIDTH - (TERMINAL_COLUMNS * CELL_WIDTH + 16U)) / 2U)
#define WINDOW_Y 88U
#define WINDOW_WIDTH (TERMINAL_COLUMNS * CELL_WIDTH + 16U)
#define WINDOW_DAMAGE_WIDTH (WINDOW_WIDTH + 6U)
#define WINDOW_DAMAGE_HEIGHT (WINDOW_HEIGHT + 8U)
#define TITLE_HEIGHT 28U
#define WINDOW_HEIGHT (TERMINAL_ROWS * CELL_HEIGHT + 16U + TITLE_HEIGHT)
#define BODY_X (WINDOW_X + 8U)
#define BODY_Y (WINDOW_Y + TITLE_HEIGHT + 8U)
#define ICON_X 20U
#define ICON_Y 18U
#define ICON_WIDTH 72U
#define ICON_HEIGHT 78U
#define INPUT_CAPACITY 1024U
#define OUTPUT_CAPACITY 8192U

typedef struct { uint8_t character, bold, color_fg, color_bg; } TerminalCell;
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
} Terminal;

static const uint8_t logo_glyphs[5][GLYPH_HEIGHT] = {
    { 17, 17, 17, 21, 21, 21, 10 }, { 17, 17, 17, 17, 17, 17, 14 },
    { 14, 17, 17, 31, 17, 17, 17 }, { 14, 17, 17, 17, 17, 17, 14 },
    { 15, 16, 16, 14,  1,  1, 30 }
};
static const uint8_t logo_letters[LETTERS] = { 0, 1, 0, 2, 3, 4 };
static const char pointer_shape[CURSOR_HEIGHT][CURSOR_WIDTH + 1U] = {
    "X...............", "XX..............", "XWX.............", "XWWX............",
    "XWWWX...........", "XWWWWX..........", "XWWWWWX.........", "XWWWWWWX........",
    "XWWWWWWWX.......", "XWWXWWWWX.......", "XWX.XWWX........", "XX..XWWX........",
    "....XWWX........", ".....XWWX.......", "......XX........", "................"
};
static const uint32_t palette[16] = {
    0x00131b25U, 0x00cc5555U, 0x004caf50U, 0x00c89b38U,
    0x005b8dd9U, 0x00b25cc7U, 0x003ca6a6U, 0x00d8e0e8U,
    0x00637482U, 0x00ff7777U, 0x0076d67aU, 0x00f2c66dU,
    0x007ba8f2U, 0x00d58be8U, 0x0061ceceU, 0x00ffffffU
};

static uint32_t cursor_saved[CURSOR_HEIGHT][CURSOR_WIDTH];
static uint32_t cursor_x, cursor_y, pointer_x = GPU_WIDTH / 2U, pointer_y = GPU_HEIGHT / 2U;
static int cursor_visible, terminal_visible, terminal_focused, terminal_session;
static unsigned int shift_keys, control_keys;
static int mouse_left_down;
static Terminal terminal;
static unsigned char input_bytes[INPUT_CAPACITY], output_bytes[OUTPUT_CAPACITY];
static uint32_t input_read, input_write, output_read, output_write;
static uint32_t dirty_left, dirty_top, dirty_right, dirty_bottom;
static int dirty;

static int glyph_pixel(unsigned int x, unsigned int y) {
    const unsigned int letter_column = x / (LETTER_STRIDE * LOGO_SCALE);
    const unsigned int within_letter = (x / LOGO_SCALE) % LETTER_STRIDE;
    if (letter_column >= LETTERS || within_letter >= GLYPH_WIDTH) return 0;
    const uint8_t row = logo_glyphs[logo_letters[letter_column]][y / LOGO_SCALE];
    return (row >> (GLYPH_WIDTH - 1U - within_letter)) & 1U;
}

static void mark_dirty(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
    if (!width || !height || x >= GPU_WIDTH || y >= GPU_HEIGHT) return;
    uint32_t right = x + width, bottom = y + height;
    if (right > GPU_WIDTH) right = GPU_WIDTH;
    if (bottom > GPU_HEIGHT) bottom = GPU_HEIGHT;
    if (!dirty) { dirty_left = x; dirty_top = y; dirty_right = right; dirty_bottom = bottom; dirty = 1; }
    else {
        if (x < dirty_left) dirty_left = x;
        if (y < dirty_top) dirty_top = y;
        if (right > dirty_right) dirty_right = right;
        if (bottom > dirty_bottom) dirty_bottom = bottom;
    }
}

static uint32_t background_pixel(unsigned int x, unsigned int y) {
    if (x >= LOGO_X && x < LOGO_X + LOGO_WIDTH && y >= LOGO_Y && y < LOGO_Y + LOGO_HEIGHT &&
        glyph_pixel(x - LOGO_X, y - LOGO_Y)) return LOGO_PIXEL;
    if (x > LOGO_X && x <= LOGO_X + LOGO_WIDTH && y > LOGO_Y && y <= LOGO_Y + LOGO_HEIGHT &&
        glyph_pixel(x - LOGO_X - 1U, y - LOGO_Y - 1U)) return SHADOW_PIXEL;
    return DESKTOP_PIXEL;
}

static void fill_rect(uint32_t *framebuffer, unsigned int x, unsigned int y,
                      unsigned int width, unsigned int height, uint32_t color) {
    if (x >= GPU_WIDTH || y >= GPU_HEIGHT) return;
    if (x + width > GPU_WIDTH) width = GPU_WIDTH - x;
    if (y + height > GPU_HEIGHT) height = GPU_HEIGHT - y;
    for (unsigned int row = 0; row < height; ++row) {
        uint32_t *pixel = framebuffer + (y + row) * GPU_WIDTH + x;
        for (unsigned int column = 0; column < width; ++column) pixel[column] = color;
    }
}

static void paint_wallpaper_rect(uint32_t *framebuffer, unsigned int x, unsigned int y,
                                 unsigned int width, unsigned int height) {
    if (x >= GPU_WIDTH || y >= GPU_HEIGHT) return;
    if (x + width > GPU_WIDTH) width = GPU_WIDTH - x;
    if (y + height > GPU_HEIGHT) height = GPU_HEIGHT - y;
    for (unsigned int row = 0; row < height; ++row)
        for (unsigned int column = 0; column < width; ++column)
            framebuffer[(y + row) * GPU_WIDTH + x + column] = background_pixel(x + column, y + row);
}

static void draw_glyph(uint32_t *framebuffer, unsigned int x, unsigned int y, unsigned char character,
                       uint32_t foreground, uint32_t background) {
    if (character < 0x20U || character > 0x7eU) character = '?';
    const unsigned char *rows = (const unsigned char *)font8x8_basic[character];
    for (unsigned int row = 0; row < CELL_HEIGHT; ++row) {
        const unsigned int font_row = row / 2U;
        for (unsigned int column = 0; column < CELL_WIDTH; ++column) {
            const int set = (rows[font_row] & (1U << column)) != 0;
            framebuffer[(y + row) * GPU_WIDTH + x + column] = set ? foreground : background;
        }
    }
}

static void draw_small_text(uint32_t *framebuffer, unsigned int x, unsigned int y,
                            const char *text, uint32_t foreground, uint32_t background) {
    while (*text) { draw_glyph(framebuffer, x, y, (unsigned char)*text++, foreground, background); x += CELL_WIDTH; }
}

static void draw_icon(uint32_t *framebuffer) {
    const unsigned int x = ICON_X, y = ICON_Y;
    fill_rect(framebuffer, x + 4U, y + 5U, 60U, 45U, 0x007d9ec0U);
    fill_rect(framebuffer, x + 8U, y + 2U, 24U, 10U, 0x007d9ec0U);
    fill_rect(framebuffer, x + 1U, y + 1U, 60U, 45U, 0x002f6fa3U);
    fill_rect(framebuffer, x + 5U, y + 6U, 52U, 33U, 0x00101b28U);
    draw_glyph(framebuffer, x + 16U, y + 14U, '>', 0x0078dbffU, 0x00101b28U);
    fill_rect(framebuffer, x + 34U, y + 27U, 8U, 2U, 0x00d4e7f4U);
    fill_rect(framebuffer, x + 1U, y + 47U, 60U, 3U, 0x00d7e2eaU);
    fill_rect(framebuffer, x + 17U, y + 50U, 28U, 3U, 0x00b0bec7U);
    draw_small_text(framebuffer, x - 1U, y + 58U, "Terminal", 0x001b3446U, DESKTOP_PIXEL);
}

static void terminal_initialize(void) {
    terminal.row = terminal.column = terminal.saved_row = terminal.saved_column = 0;
    terminal.parser = terminal.parameter_count = 0;
    terminal.private_mode = terminal.wrap_pending = terminal.bold = terminal.reverse_video = 0;
    terminal.saved_bold = terminal.saved_reverse_video = 0;
    terminal.foreground = terminal.saved_foreground = 7U;
    terminal.background = terminal.saved_background = 0U;
    terminal.cells = terminal.normal;
    terminal.saved_cells = terminal.normal;
    terminal.cursor_enabled = 1;
    for (unsigned int i = 0; i < TERMINAL_COLUMNS * TERMINAL_ROWS; ++i) {
        terminal.normal[i] = (TerminalCell){ ' ', 0, 7, 0 };
        terminal.alternate[i] = (TerminalCell){ ' ', 0, 7, 0 };
    }
}

static TerminalCell *cell_at(unsigned int row, unsigned int column) {
    return &terminal.cells[row * TERMINAL_COLUMNS + column];
}

static TerminalCell current_blank(unsigned char character) {
    const unsigned int foreground = terminal.reverse_video ? terminal.background : terminal.foreground;
    const unsigned int background = terminal.reverse_video ? terminal.foreground : terminal.background;
    return (TerminalCell){ character, (uint8_t)terminal.bold, (uint8_t)foreground, (uint8_t)background };
}

static void terminal_clear_line(unsigned int row, unsigned int from, unsigned int to) {
    if (row >= TERMINAL_ROWS || from >= TERMINAL_COLUMNS) return;
    if (to >= TERMINAL_COLUMNS) to = TERMINAL_COLUMNS - 1U;
    for (unsigned int column = from; column <= to; ++column)
        *cell_at(row, column) = current_blank(' ');
}

static void terminal_scroll(void) {
    const unsigned int cells = TERMINAL_COLUMNS * (TERMINAL_ROWS - 1U);
    for (unsigned int i = 0; i < cells; ++i) terminal.cells[i] = terminal.cells[i + TERMINAL_COLUMNS];
    terminal_clear_line(TERMINAL_ROWS - 1U, 0, TERMINAL_COLUMNS - 1U);
}

static void terminal_line_feed(void) {
    terminal.wrap_pending = 0;
    terminal.column = 0;
    if (terminal.row + 1U < TERMINAL_ROWS) ++terminal.row;
    else terminal_scroll();
}

static void terminal_put_printable(unsigned char character) {
    if (terminal.wrap_pending) terminal_line_feed();
    *cell_at(terminal.row, terminal.column) = current_blank(character);
    if (terminal.column + 1U == TERMINAL_COLUMNS) terminal.wrap_pending = 1;
    else ++terminal.column;
}

static unsigned int parameter(unsigned int index, unsigned int fallback) {
    if (index >= terminal.parameter_count || terminal.parameters[index] == 0) return fallback;
    return terminal.parameters[index];
}

static void terminal_erase_display(unsigned int mode) {
    if (mode == 2U) {
        for (unsigned int row = 0; row < TERMINAL_ROWS; ++row) terminal_clear_line(row, 0, TERMINAL_COLUMNS - 1U);
    } else if (mode == 0U) {
        terminal_clear_line(terminal.row, terminal.column, TERMINAL_COLUMNS - 1U);
        for (unsigned int row = terminal.row + 1U; row < TERMINAL_ROWS; ++row) terminal_clear_line(row, 0, TERMINAL_COLUMNS - 1U);
    } else if (mode == 1U) {
        for (unsigned int row = 0; row < terminal.row; ++row) terminal_clear_line(row, 0, TERMINAL_COLUMNS - 1U);
        terminal_clear_line(terminal.row, 0, terminal.column);
    }
}

static void terminal_csi(unsigned char command) {
    const unsigned int n = parameter(0, 1U);
    if (terminal.private_mode) {
        if (command == 'h' && terminal.parameters[0] == 25U) terminal.cursor_enabled = 1;
        else if (command == 'l' && terminal.parameters[0] == 25U) terminal.cursor_enabled = 0;
        else if (terminal.parameter_count == 1U && terminal.parameters[0] == 1049U &&
                 command == 'h' && terminal.cells != terminal.alternate) {
            terminal.saved_cells = terminal.cells;
            terminal.saved_row = terminal.row; terminal.saved_column = terminal.column;
            terminal.saved_foreground = terminal.foreground; terminal.saved_background = terminal.background;
            terminal.saved_bold = terminal.bold; terminal.saved_reverse_video = terminal.reverse_video;
            terminal.cells = terminal.alternate;
            for (unsigned int i = 0; i < TERMINAL_COLUMNS * TERMINAL_ROWS; ++i)
                terminal.alternate[i] = (TerminalCell){ ' ', 0, 7, 0 };
            terminal.row = terminal.column = 0; terminal.foreground = 7; terminal.background = 0;
            terminal.bold = terminal.reverse_video = 0;
            terminal.wrap_pending = 0;
        } else if (terminal.parameter_count == 1U && terminal.parameters[0] == 1049U &&
                   command == 'l' && terminal.cells == terminal.alternate) {
            terminal.cells = terminal.saved_cells;
            terminal.row = terminal.saved_row; terminal.column = terminal.saved_column;
            terminal.foreground = terminal.saved_foreground; terminal.background = terminal.saved_background;
            terminal.bold = terminal.saved_bold; terminal.reverse_video = terminal.saved_reverse_video;
            terminal.wrap_pending = 0;
        }
        return;
    }
    if (command == 'H' || command == 'f') {
        terminal.row = parameter(0, 1U) - 1U;
        terminal.column = parameter(1, 1U) - 1U;
        if (terminal.row >= TERMINAL_ROWS) terminal.row = TERMINAL_ROWS - 1U;
        if (terminal.column >= TERMINAL_COLUMNS) terminal.column = TERMINAL_COLUMNS - 1U;
        terminal.wrap_pending = 0;
    } else if (command == 'A') terminal.row = terminal.row > n ? terminal.row - n : 0;
    else if (command == 'B') { terminal.row += n; if (terminal.row >= TERMINAL_ROWS) terminal.row = TERMINAL_ROWS - 1U; }
    else if (command == 'C') { terminal.column += n; if (terminal.column >= TERMINAL_COLUMNS) terminal.column = TERMINAL_COLUMNS - 1U; }
    else if (command == 'D') terminal.column = terminal.column > n ? terminal.column - n : 0;
    else if (command == 'J') terminal_erase_display(terminal.parameters[0]);
    else if (command == 'K') {
        if (terminal.parameters[0] == 0U) terminal_clear_line(terminal.row, terminal.column, TERMINAL_COLUMNS - 1U);
        else if (terminal.parameters[0] == 1U) terminal_clear_line(terminal.row, 0, terminal.column);
        else if (terminal.parameters[0] == 2U) terminal_clear_line(terminal.row, 0, TERMINAL_COLUMNS - 1U);
    } else if (command == 'm') {
        if (terminal.parameter_count == 0U) terminal.parameters[terminal.parameter_count++] = 0U;
        for (unsigned int i = 0; i < terminal.parameter_count; ++i) {
            const unsigned int value = terminal.parameters[i];
            if (value == 0U) { terminal.foreground = 7U; terminal.background = 0U; terminal.bold = 0; terminal.reverse_video = 0; }
            else if (value == 1U) terminal.bold = 1;
            else if (value == 22U) terminal.bold = 0;
            else if (value == 7U) terminal.reverse_video = 1;
            else if (value == 27U) terminal.reverse_video = 0;
            else if (value == 39U) terminal.foreground = 7U;
            else if (value == 49U) terminal.background = 0U;
            else if (value >= 30U && value <= 37U) terminal.foreground = value - 30U;
            else if (value >= 40U && value <= 47U) terminal.background = value - 40U;
            else if (value >= 90U && value <= 97U) terminal.foreground = value - 90U + 8U;
            else if (value >= 100U && value <= 107U) terminal.background = value - 100U + 8U;
        }
    }
}

static void terminal_feed(unsigned char byte) {
    if (terminal.parser == 0U) {
        if (byte == 0x1bU) { terminal.parser = 1U; return; }
        if (byte == '\r') { terminal.column = 0; terminal.wrap_pending = 0; return; }
        if (byte == '\n') { terminal_line_feed(); return; }
        if (byte == '\b' || byte == 127U) {
            if (terminal.wrap_pending) terminal.wrap_pending = 0;
            else if (terminal.column) --terminal.column;
            return;
        }
        if (byte == '\t') {
            const unsigned int target = (terminal.column + 8U) & ~7U;
            do { terminal_put_printable(' '); } while (!terminal.wrap_pending && terminal.column < target);
            return;
        }
        if (byte >= 32U && byte <= 126U) terminal_put_printable(byte);
    } else if (terminal.parser == 1U) {
        if (byte == '[') {
            terminal.parser = 2U; terminal.parameter_count = 1;
            for (unsigned int i = 0; i < 8U; ++i) terminal.parameters[i] = 0;
            terminal.private_mode = 0;
        } else terminal.parser = 0;
    } else {
        if (byte == '?' && terminal.parameter_count == 1U && terminal.parameters[0] == 0U) terminal.private_mode = 1;
        else if (byte >= '0' && byte <= '9' && terminal.parameter_count > 0U) {
            const unsigned int index = terminal.parameter_count - 1U;
            if (index < 8U && terminal.parameters[index] < 999U)
                terminal.parameters[index] = terminal.parameters[index] * 10U + (byte - '0');
        } else if (byte == ';') {
            if (terminal.parameter_count < 8U) ++terminal.parameter_count;
        } else if (byte >= 0x40U && byte <= 0x7eU) {
            terminal_csi(byte);
            terminal.parser = 0;
        } else if (byte == 0x1bU) terminal.parser = 1U;
        else if (byte < 0x20U) { /* Ignore controls inside an incomplete CSI. */ }
        else terminal.parser = 0;
    }
}

static void draw_terminal_window(uint32_t *framebuffer) {
    const unsigned int x = WINDOW_X, y = WINDOW_Y;
    fill_rect(framebuffer, x + 6U, y + 8U, WINDOW_WIDTH, WINDOW_HEIGHT, 0x00435b70U);
    fill_rect(framebuffer, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 0x00e0e9efU);
    fill_rect(framebuffer, x + 2U, y + 2U, WINDOW_WIDTH - 4U, WINDOW_HEIGHT - 4U, 0x0014212dU);
    fill_rect(framebuffer, x + 2U, y + 2U, WINDOW_WIDTH - 4U, TITLE_HEIGHT, 0x002b4256U);
    draw_small_text(framebuffer, x + 12U, y + 7U, "Terminal - WUWAOS", 0x00f1f6faU, 0x002b4256U);
    fill_rect(framebuffer, x + WINDOW_WIDTH - 30U, y + 5U, 20U, 18U, 0x00b84d55U);
    draw_glyph(framebuffer, x + WINDOW_WIDTH - 26U, y + 6U, 'x', 0x00ffffffU, 0x00b84d55U);
    fill_rect(framebuffer, x + 4U, y + TITLE_HEIGHT + 4U, WINDOW_WIDTH - 8U, WINDOW_HEIGHT - TITLE_HEIGHT - 8U, palette[0]);

    for (unsigned int row = 0; row < TERMINAL_ROWS; ++row) {
        for (unsigned int column = 0; column < TERMINAL_COLUMNS; ++column) {
            const TerminalCell *cell = &terminal.cells[row * TERMINAL_COLUMNS + column];
            uint32_t foreground = palette[cell->color_fg & 15U];
            const uint32_t background = palette[cell->color_bg & 15U];
            if (cell->bold && cell->color_fg < 8U) foreground = palette[cell->color_fg + 8U];
            draw_glyph(framebuffer, BODY_X + column * CELL_WIDTH, BODY_Y + row * CELL_HEIGHT,
                cell->character, foreground, background);
        }
    }
    if (terminal.cursor_enabled && terminal_visible && terminal_focused) {
        unsigned int xcur = terminal.column;
        if (terminal.wrap_pending) xcur = TERMINAL_COLUMNS - 1U;
        const unsigned int py = BODY_Y + terminal.row * CELL_HEIGHT + CELL_HEIGHT - 2U;
        fill_rect(framebuffer, BODY_X + xcur * CELL_WIDTH, py, CELL_WIDTH, 2U, 0x00f4f7faU);
    }
}

static void draw_pointer(void) {
    uint32_t *framebuffer = gpu_framebuffer();
    if (!framebuffer) return;
    for (unsigned int row = 0; row < CURSOR_HEIGHT; ++row) {
        for (unsigned int column = 0; column < CURSOR_WIDTH; ++column) {
            const size_t index = (cursor_y + row) * GPU_WIDTH + cursor_x + column;
            cursor_saved[row][column] = framebuffer[index];
            if (pointer_shape[row][column] == 'X') framebuffer[index] = 0x00000000U;
            else if (pointer_shape[row][column] == 'W') framebuffer[index] = 0x00ffffffU;
        }
    }
}

static void restore_pointer(void) {
    if (!cursor_visible) return;
    uint32_t *framebuffer = gpu_framebuffer();
    if (!framebuffer) return;
    for (unsigned int row = 0; row < CURSOR_HEIGHT; ++row)
        for (unsigned int column = 0; column < CURSOR_WIDTH; ++column)
            framebuffer[(cursor_y + row) * GPU_WIDTH + cursor_x + column] = cursor_saved[row][column];
}

void desktop_draw(void) {
    uint32_t *framebuffer = gpu_framebuffer();
    if (!framebuffer) return;
    for (size_t index = 0; index < GPU_WIDTH * GPU_HEIGHT; ++index) framebuffer[index] = DESKTOP_PIXEL;
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
    if (input_keyboard_ready()) draw_icon(framebuffer);
    (void)gpu_present(0, 0, GPU_WIDTH, GPU_HEIGHT);
    terminal_initialize();
}

void desktop_start_cursor(void) {
    uint32_t *framebuffer = gpu_framebuffer();
    if (!framebuffer) return;
    cursor_x = GPU_WIDTH / 2U; cursor_y = GPU_HEIGHT / 2U;
    pointer_x = cursor_x; pointer_y = cursor_y;
    draw_pointer(); cursor_visible = 1;
    (void)gpu_present(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
}

static int hit_icon(uint32_t x, uint32_t y) {
    return x >= ICON_X && x < ICON_X + ICON_WIDTH && y >= ICON_Y && y < ICON_Y + ICON_HEIGHT;
}

static int hit_close(uint32_t x, uint32_t y) {
    return x >= WINDOW_X + WINDOW_WIDTH - 32U && x < WINDOW_X + WINDOW_WIDTH - 6U &&
        y >= WINDOW_Y + 4U && y < WINDOW_Y + 25U;
}

static int hit_window(uint32_t x, uint32_t y) {
    return x >= WINDOW_X && x < WINDOW_X + WINDOW_WIDTH && y >= WINDOW_Y && y < WINDOW_Y + WINDOW_HEIGHT;
}

static void show_terminal(void) {
    if (!terminal_session) {
        input_read = input_write = output_read = output_write = 0;
        shift_keys = control_keys = 0;
        terminal_initialize();
        terminal_feed('W'); terminal_feed('U'); terminal_feed('W'); terminal_feed('A'); terminal_feed('O'); terminal_feed('S');
        terminal_feed(' '); terminal_feed('T'); terminal_feed('e'); terminal_feed('r'); terminal_feed('m'); terminal_feed('i');
        terminal_feed('n'); terminal_feed('a'); terminal_feed('l'); terminal_feed('\n');
        const long pid = process_start_gui_terminal();
        if (pid < 0) {
            static const unsigned char error[] = "Unable to start shell: process table or memory is full.\n";
            (void)desktop_terminal_write(error, sizeof(error) - 1U);
        } else terminal_session = 1;
    }
    terminal_visible = 1;
    terminal_focused = 1;
    mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
}

static unsigned char shifted_ascii(unsigned char character) {
    if (character >= 'a' && character <= 'z') return (unsigned char)(character - 'a' + 'A');
    switch (character) {
    case '1': return '!'; case '2': return '@'; case '3': return '#'; case '4': return '$';
    case '5': return '%'; case '6': return '^'; case '7': return '&'; case '8': return '*';
    case '9': return '('; case '0': return ')'; case '-': return '_'; case '=': return '+';
    case '[': return '{'; case ']': return '}'; case '\\': return '|'; case ';': return ':';
    case '\'': return '"'; case ',': return '<'; case '.': return '>'; case '/': return '?'; case '`': return '~';
    default: return character;
    }
}

static unsigned char key_ascii(uint16_t code) {
    if (code >= 2U && code <= 11U) { static const char row[] = "1234567890"; return row[code - 2U]; }
    if (code >= 16U && code <= 25U) { static const char row[] = "qwertyuiop"; return row[code - 16U]; }
    if (code >= 30U && code <= 38U) { static const char row[] = "asdfghjkl"; return row[code - 30U]; }
    if (code >= 44U && code <= 50U) { static const char row[] = "zxcvbnm"; return row[code - 44U]; }
    switch (code) {
    case 12: return '-'; case 13: return '='; case 26: return '['; case 27: return ']';
    case 39: return ';'; case 40: return '\''; case 41: return '`'; case 43: return '\\';
    case 51: return ','; case 52: return '.'; case 53: return '/'; case 57: return ' ';
    case 14: return '\b'; case 15: return '\t'; case 28: return '\r'; case 1: return 0x1b;
    default: return 0;
    }
}

static void enqueue_input(unsigned char byte) {
    if (input_write - input_read >= INPUT_CAPACITY) return;
    input_bytes[input_write++ % INPUT_CAPACITY] = byte;
}

static void handle_key(const InputMessage *message) {
    const uint16_t code = message->code;
    const int pressed = message->value != 0;
    if (code == 42U || code == 54U) {
        const unsigned int mask = code == 42U ? 1U : 2U;
        if (pressed) shift_keys |= mask; else shift_keys &= ~mask;
        return;
    }
    if (code == 29U || code == 97U) {
        const unsigned int mask = code == 29U ? 1U : 2U;
        if (pressed) control_keys |= mask; else control_keys &= ~mask;
        return;
    }
    if (!pressed || !terminal_visible || !terminal_focused) return;
    if (code == 103U) { enqueue_input(0x1b); enqueue_input('['); enqueue_input('A'); return; }
    if (code == 108U) { enqueue_input(0x1b); enqueue_input('['); enqueue_input('B'); return; }
    if (code == 105U) { enqueue_input(0x1b); enqueue_input('['); enqueue_input('D'); return; }
    if (code == 106U) { enqueue_input(0x1b); enqueue_input('['); enqueue_input('C'); return; }
    unsigned char character = key_ascii(code);
    if (!character) return;
    if (control_keys) {
        if (character >= 'a' && character <= 'z') character = (unsigned char)(character - 'a' + 1U);
        else if (character >= 'A' && character <= 'Z') character = (unsigned char)(character - 'A' + 1U);
        else if (character == ' ') character = 0;
    } else if (shift_keys) character = shifted_ascii(character);
    enqueue_input(character);
}

static void handle_message(const InputMessage *message) {
    if (message->type == INPUT_MESSAGE_RESET) {
        shift_keys = control_keys = mouse_left_down = 0;
    } else if (message->type == INPUT_MESSAGE_POINTER) {
        pointer_x = message->x; pointer_y = message->y;
        if (pointer_x > GPU_WIDTH - CURSOR_WIDTH) pointer_x = GPU_WIDTH - CURSOR_WIDTH;
        if (pointer_y > GPU_HEIGHT - CURSOR_HEIGHT) pointer_y = GPU_HEIGHT - CURSOR_HEIGHT;
    } else if (message->type == INPUT_MESSAGE_KEY) handle_key(message);
    else if (message->type == INPUT_MESSAGE_BUTTON && message->code == 272U) {
        if (message->value == 1) {
            mouse_left_down = 1;
            if (terminal_visible && hit_close(pointer_x, pointer_y)) {
                terminal_visible = 0; terminal_focused = 0;
                mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
            } else if (hit_icon(pointer_x, pointer_y)) show_terminal();
            else if (hit_window(pointer_x, pointer_y)) {
                if (!terminal_focused) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
                terminal_focused = 1;
            } else if (terminal_visible) {
                if (terminal_focused) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
                terminal_focused = 0;
            }
        } else mouse_left_down = 0;
    }
}

void desktop_poll(void) {
    input_poll();
    InputMessage message;
    while (input_next_message(&message)) handle_message(&message);

    if (cursor_visible && (pointer_x != cursor_x || pointer_y != cursor_y)) {
        mark_dirty(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
        mark_dirty(pointer_x, pointer_y, CURSOR_WIDTH, CURSOR_HEIGHT);
    }
    if (!dirty) return;
    restore_pointer();
    if (dirty_left < WINDOW_X + WINDOW_DAMAGE_WIDTH && dirty_right > WINDOW_X &&
        dirty_top < WINDOW_Y + WINDOW_DAMAGE_HEIGHT && dirty_bottom > WINDOW_Y) {
        paint_wallpaper_rect(gpu_framebuffer(), WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
        if (terminal_visible) draw_terminal_window(gpu_framebuffer());
        mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
    }
    cursor_x = pointer_x; cursor_y = pointer_y;
    if (cursor_visible) {
        draw_pointer();
        mark_dirty(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
    }
    dirty = 0;
    (void)gpu_present(dirty_left, dirty_top, dirty_right - dirty_left, dirty_bottom - dirty_top);
}

int desktop_event_source_active(void) { return cursor_visible; }
void desktop_terminal_set_session(int active) { terminal_session = !!active; }
int desktop_terminal_session_active(void) { return terminal_session; }
int desktop_terminal_visible(void) { return terminal_visible; }

int desktop_terminal_read_byte(void) {
    if (input_read == input_write) return -1;
    return input_bytes[input_read++ % INPUT_CAPACITY];
}

static void drain_output(void) {
    while (output_read != output_write) terminal_feed(output_bytes[output_read++ % OUTPUT_CAPACITY]);
    if (terminal_visible) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT);
}

long desktop_terminal_write(const unsigned char *bytes, size_t length) {
    if (!bytes) return -1;
    size_t copied = 0;
    while (copied < length) {
        if (output_write - output_read >= OUTPUT_CAPACITY) drain_output();
        output_bytes[output_write++ % OUTPUT_CAPACITY] = bytes[copied++];
    }
    drain_output();
    return (long)copied;
}

void desktop_terminal_echo_byte(unsigned char byte) { terminal_feed(byte); if (terminal_visible) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_DAMAGE_WIDTH, WINDOW_DAMAGE_HEIGHT); }
