#include "api.h"
#include "print.h"
#include <stddef.h>

#define LIMIT 2048U
#define COLS 78U
#define ROWS 20U
#define NONE (-1)
enum { UP = 256, DOWN, LEFT, RIGHT, HOME, END, DELETE, PAGE_UP, PAGE_DOWN };

static char text[LIMIT + 1]; /* Extra byte detects oversized existing files. */
static size_t length, cursor, goal, top, left;
static int dirty, confirming;
static const char *filename;
static const char *message = "Ready";
static long save_error;
/* Cache visible lines, leaving room for text in the single 4 KiB data page. */
static char screen[22][COLS + 1];
static unsigned int screen_valid;

static size_t line_start(size_t at) {
    while (at > 0 && text[at - 1] != '\n') --at;
    return at;
}

static size_t line_end(size_t at) {
    while (at < length && text[at] != '\n') ++at;
    return at;
}

static void set_goal(void) {
    goal = cursor - line_start(cursor);
}

/* Insert/delete move the tail of a bounded contiguous buffer, O(length). */
static void insert(char character, size_t count) {
    if (count > LIMIT - length) {
        message = "Buffer full: 2048 bytes maximum";
        return;
    }
    for (size_t i = length; i > cursor; --i) text[i + count - 1] = text[i - 1];
    for (size_t i = 0; i < count; ++i) text[cursor + i] = character;
    length += count;
    cursor += count;
    dirty = 1;
    set_goal();
}

static void erase(size_t at) {
    if (at == length) return;
    for (size_t i = at; i + 1 < length; ++i) text[i] = text[i + 1];
    --length;
    dirty = 1;
}

static void vertical(int direction) {
    size_t start, end;
    if (direction < 0) {
        start = line_start(cursor);
        if (start == 0) return;
        end = start - 1;
        start = line_start(end);
    } else {
        end = line_end(cursor);
        if (end == length) return;
        start = end + 1;
        end = line_end(start);
    }
    cursor = start + (goal < end - start ? goal : end - start);
}

static void append(char *row, size_t *used, const char *value) {
    while (*value && *used < COLS) row[(*used)++] = *value++;
    row[*used] = 0;
}

static void number(char *row, size_t *used, size_t value) {
    char digits[21];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (count && *used < COLS) row[(*used)++] = digits[--count];
    row[*used] = 0;
}

static void position(size_t row, size_t column) {
    char sequence[79];
    size_t used = 0;
    append(sequence, &used, "\033[");
    number(sequence, &used, row);
    append(sequence, &used, ";");
    number(sequence, &used, column);
    append(sequence, &used, "H");
    user_puts(sequence);
}

static void draw_line(size_t row, const char *contents) {
    const unsigned int mask = 1U << (row - 1);
    if (screen_valid & mask) {
        if (row > 22) return; /* The two help lines are constant. */
        size_t at = 0;
        while (screen[row - 1][at] && screen[row - 1][at] == contents[at]) ++at;
        if (screen[row - 1][at] == contents[at]) return;
    }
    if (row <= 22) {
        size_t at = 0;
        do { screen[row - 1][at] = contents[at]; } while (contents[at++]);
    }
    screen_valid |= mask;
    position(row, 1);
    if (row == 1) user_puts("\033[7m");
    user_puts("\033[K");
    user_puts(contents);
    if (row == 1) user_puts("\033[0m");
}

static void render(void) {
    size_t current_line = 0, column = 0;
    for (size_t i = 0; i < cursor; ++i) {
        if (text[i] == '\n') { ++current_line; column = 0; }
        else ++column;
    }
    if (current_line < top) top = current_line;
    if (current_line >= top + ROWS) top = current_line - ROWS + 1;
    if (column < left) left = column;
    if (column >= left + COLS) left = column - COLS + 1;

    user_puts("\033[?25l");
    char row[COLS + 1];
    size_t used = 0;
    append(row, &used, dirty ? " fan * | " : " fan | ");
    append(row, &used, filename);
    draw_line(1, row);

    size_t start = 0;
    for (size_t i = 0; i < top; ++i) start = line_end(start) + 1;
    int valid = 1;
    for (size_t r = 0; r < ROWS; ++r) {
        used = 0;
        if (valid) {
            const size_t end = line_end(start);
            for (size_t i = start + left; i < end && used < COLS; ++i) {
                row[used++] = text[i] == '\t' ? '>' : text[i];
            }
            valid = end < length;
            start = end + 1;
        } else row[used++] = '~';
        row[used] = 0;
        draw_line(r + 2, row);
    }
    used = 0;
    if (confirming) append(row, &used, "Save changes? y=save and exit  n=discard  c/Esc=cancel");
    else {
        append(row, &used, "Ln "); number(row, &used, current_line + 1);
        append(row, &used, " Col "); number(row, &used, column + 1);
        append(row, &used, " | "); number(row, &used, length);
        append(row, &used, "/2048 bytes | "); append(row, &used, message);
        if (save_error < 0) {
            append(row, &used, " -"); number(row, &used, (size_t)-save_error);
        }
    }
    draw_line(22, row);
    draw_line(23, "^O Save   ^X Exit   ^L Redraw   Arrows Move   Home/End   PgUp/PgDn");
    draw_line(24, "Backspace/Delete: erase   Enter: new line   Tab: spaces   ASCII/LF text");
    position(current_line - top + 2, column - left + 1);
    user_puts("\033[?25h");
}

static int save(void) {
    const long result = fs_writefile(filename, text, length);
    if (result < 0 || (size_t)result != length) {
        message = "Save failed; buffer kept";
        save_error = result < 0 ? result : -5;
        return 0;
    }
    dirty = 0;
    save_error = 0;
    message = "Saved";
    return 1;
}

static int escape_key(int final, unsigned int parameter) {
    switch (final) {
    case 'A': return UP;
    case 'B': return DOWN;
    case 'C': return RIGHT;
    case 'D': return LEFT;
    case 'H': return HOME;
    case 'F': return END;
    case '~':
        switch (parameter) {
        case 1: case 7: return HOME;
        case 4: case 8: return END;
        case 3: return DELETE;
        case 5: return PAGE_UP;
        case 6: return PAGE_DOWN;
        }
    }
    return NONE;
}

/* Incremental CSI/SS3 decoder: a lone Escape never blocks waiting for a suffix. */
static int decode(int byte) {
    static int state, unsupported;
    static unsigned int parameter, count;
    if (byte == 27) { state = 1; return NONE; }
    if (state == 0) return byte;
    if (state == 1) {
        state = byte == '[' ? 2 : byte == 'O' ? 3 : 0;
        parameter = count = 0;
        unsupported = 0;
        return state ? NONE : byte;
    }
    if (byte < 32 || byte > 126 || ++count > 16) {
        state = 0;
        return byte < 32 ? byte : NONE;
    }
    if (state == 2 && byte >= '0' && byte <= '9') {
        parameter = parameter < 1000 ? parameter * 10 + (unsigned int)(byte - '0') : 1000;
        return NONE;
    }
    if (byte >= 0x20 && byte <= 0x3f && state == 2) {
        unsupported = 1;
        return NONE;
    }
    state = 0;
    return unsupported ? NONE : escape_key(byte, parameter);
}

static int handle(int key) {
    if (confirming) {
        if (key == 'n' || key == 'N') return 0;
        if (key == 'y' || key == 'Y') {
            confirming = 0;
            return save() ? 0 : 1;
        }
        if (key == 'c' || key == 'C' || key == 24 || key == 27) {
            confirming = 0;
            message = "Exit cancelled";
        }
        return 1;
    }
    message = "Ready";
    save_error = 0;
    switch (key) {
    case 15: save(); break; /* Ctrl+O */
    case 24: /* Ctrl+X */
        if (!dirty) return 0;
        confirming = 1;
        break;
    case 12: screen_valid = 0; break; /* Ctrl+L: repaint every line. */
    case LEFT: if (cursor > 0) --cursor; set_goal(); break;
    case RIGHT: if (cursor < length) ++cursor; set_goal(); break;
    case UP: vertical(-1); break;
    case DOWN: vertical(1); break;
    case HOME: cursor = line_start(cursor); set_goal(); break;
    case END: cursor = line_end(cursor); set_goal(); break;
    case PAGE_UP: for (size_t i = 0; i < ROWS; ++i) vertical(-1); break;
    case PAGE_DOWN: for (size_t i = 0; i < ROWS; ++i) vertical(1); break;
    case 8: case 127:
        if (cursor > 0) erase(--cursor);
        set_goal();
        break;
    case DELETE: erase(cursor); set_goal(); break;
    case '\n': insert('\n', 1); break;
    case '\t': insert(' ', 4 - (cursor - line_start(cursor)) % 4); break;
    default:
        if (key >= 32 && key <= 126) insert((char)key, 1);
        else message = "Unsupported key (ASCII text only)";
    }
    return 1;
}

long user_main(long program, const char *path) {
    (void)program;
    filename = path;
    if (path == 0 || path[0] != '/') {
        user_puts("fan: an absolute file path is required.\n");
        return 1;
    }
    const long loaded = fs_readfile(path, text, sizeof(text));
    if (loaded == -2) message = "New file (created on save)";
    else if (loaded < 0) {
        user_puts("fan: cannot open file, error "); user_number(loaded); user_puts("\n");
        return 1;
    } else if (loaded > LIMIT) {
        user_puts("fan: file exceeds 2048 bytes; left unchanged.\n");
        return 1;
    } else length = (size_t)loaded;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char byte = (unsigned char)text[i];
        if (byte != '\n' && byte != '\t' && (byte < 32 || byte > 126)) {
            user_puts("fan: unsupported file encoding/control bytes; left unchanged.\n");
            return 1;
        }
    }
    const long mode = console_mode(1);
    if (mode < 0) {
        user_puts("fan: cannot acquire raw console, error "); user_number(mode); user_puts("\n");
        return 1;
    }
    user_puts("\033[?1049h\033[2J");
    render();
    int result = 0, previous_cr = 0;
    for (;;) {
        char byte;
        if (read(0, &byte, 1) != 1) { result = 1; break; }
        int key = (unsigned char)byte;
        if (key == '\n' && previous_cr) { previous_cr = 0; continue; }
        previous_cr = key == '\r';
        if (previous_cr) key = '\n';
        if (!(confirming && key == 27)) key = decode(key);
        if (key == NONE) continue;
        if (!handle(key)) break;
        render();
    }
    user_puts("\033[0m\033[?25h\033[?1049l");
    console_mode(0);
    return result;
}
