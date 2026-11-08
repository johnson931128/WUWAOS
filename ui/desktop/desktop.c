#include "ui/desktop/desktop.h"
#include "drivers/virtio/gpu.h"
#include "drivers/virtio/input.h"
#include "kernel/io/tty.h"
#include "kernel/proc/terminal_service.h"
#include "ui/graphics/primitives.h"
#include "ui/input/keymap.h"
#include "ui/assets/font_terminal.h"
#include "ui/terminal/renderer.h"
#include "ui/terminal/vt.h"

#define DESKTOP_PIXEL 0x00b8dcf5U
#define CURSOR_WIDTH 16U
#define CURSOR_HEIGHT 16U
#define WINDOW_X ((GPU_WIDTH - (TERMINAL_COLUMNS * TERMINAL_FONT_WIDTH + 16U)) / 2U)
#define WINDOW_Y 88U
#define WINDOW_WIDTH (TERMINAL_COLUMNS * TERMINAL_FONT_WIDTH + 16U)
#define TITLE_HEIGHT 28U
#define WINDOW_HEIGHT (TERMINAL_ROWS * TERMINAL_FONT_HEIGHT + 16U + TITLE_HEIGHT)
#define BODY_X (WINDOW_X + 8U)
#define BODY_Y (WINDOW_Y + TITLE_HEIGHT + 8U)
#define ICON_X 20U
#define ICON_Y 18U
#define ICON_WIDTH 72U
#define ICON_HEIGHT 78U

static const char pointer_shape[CURSOR_HEIGHT][CURSOR_WIDTH + 1U] = {
    "X...............", "XX..............", "XWX.............", "XWWX............",
    "XWWWX...........", "XWWWWX..........", "XWWWWWX.........", "XWWWWWWX........",
    "XWWWWWWWX.......", "XWWXWWWWX.......", "XWX.XWWX........", "XX..XWWX........",
    "....XWWX........", ".....XWWX.......", "......XX........", "................"
};
static uint32_t cursor_saved[CURSOR_HEIGHT][CURSOR_WIDTH];
static uint32_t cursor_x, cursor_y, pointer_x = GPU_WIDTH / 2U, pointer_y = GPU_HEIGHT / 2U;
static int cursor_visible, terminal_visible, terminal_focused;
static int mouse_left_down;
static TerminalModel terminal;
static TtyId terminal_tty;
static uint32_t dirty_left, dirty_top, dirty_right, dirty_bottom;
static int dirty;

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

static void draw_terminal_window(uint32_t *framebuffer) {
    graphics_fill_rect(framebuffer, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, 0x00e0e9efU);
    graphics_fill_rect(framebuffer, WINDOW_X + 2U, WINDOW_Y + 2U, WINDOW_WIDTH - 4U, WINDOW_HEIGHT - 4U, 0x0014212dU);
    graphics_fill_rect(framebuffer, WINDOW_X + 2U, WINDOW_Y + 2U, WINDOW_WIDTH - 4U, TITLE_HEIGHT, 0x002b4256U);
    graphics_draw_text(framebuffer, WINDOW_X + 12U, WINDOW_Y + 7U, "Terminal - WUWAOS", 0x00f1f6faU, 0x002b4256U);
    graphics_fill_rect(framebuffer, WINDOW_X + WINDOW_WIDTH - 30U, WINDOW_Y + 5U, 20U, 18U, 0x00b84d55U);
    graphics_draw_glyph(framebuffer, WINDOW_X + WINDOW_WIDTH - 26U, WINDOW_Y + 6U, 'x', 0x00ffffffU, 0x00b84d55U);
    graphics_fill_rect(framebuffer, WINDOW_X + 4U, WINDOW_Y + TITLE_HEIGHT + 4U,
        WINDOW_WIDTH - 8U, WINDOW_HEIGHT - TITLE_HEIGHT - 8U, 0x00131b25U);
    terminal_renderer_draw(framebuffer, &terminal, BODY_X, BODY_Y, terminal_visible && terminal_focused);
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
    graphics_draw_logo(framebuffer);
    if (input_keyboard_ready()) graphics_draw_terminal_icon(framebuffer);
    (void)gpu_present(0, 0, GPU_WIDTH, GPU_HEIGHT);
    terminal_model_init(&terminal);
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
    if (!terminal_tty || !tty_session_active(terminal_tty)) {
        if (terminal_tty) tty_release(terminal_tty);
        terminal_tty = TTY_ID_INVALID;
        ui_keymap_reset();
        terminal_model_init(&terminal);
        static const unsigned char welcome[] = "WUWAOS Terminal\n";
        for (size_t i = 0; i < sizeof(welcome) - 1U; ++i) terminal_model_feed(&terminal, welcome[i]);
        TtyId id = TTY_ID_INVALID;
        const long pid = process_start_gui_terminal(desktop_terminal_output, &terminal, &id);
        if (pid < 0) {
            static const unsigned char error[] = "Unable to start shell: process table or memory is full.\n";
            for (size_t i = 0; i < sizeof(error) - 1U; ++i) terminal_model_feed(&terminal, error[i]);
        } else {
            terminal_tty = id;
        }
    }
    terminal_visible = 1;
    terminal_focused = 1;
    mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
}

static void handle_key(const InputMessage *message) {
    unsigned char bytes[3];
    const size_t length = ui_keymap_translate(message->code, message->value != 0, bytes);
    if (length && terminal_visible && terminal_focused && terminal_tty)
        (void)tty_input_push(terminal_tty, bytes, length);
}

static void handle_message(const InputMessage *message) {
    if (message->type == INPUT_MESSAGE_RESET) {
        ui_keymap_reset(); mouse_left_down = 0;
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
                mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
            } else if (hit_icon(pointer_x, pointer_y)) show_terminal();
            else if (hit_window(pointer_x, pointer_y)) {
                if (!terminal_focused) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
                terminal_focused = 1;
            } else if (terminal_visible) {
                if (terminal_focused) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
                terminal_focused = 0;
            }
        } else mouse_left_down = 0;
    }
}

void desktop_poll(void) {
    gpu_poll();
    if (!gpu_is_enabled()) return;
    input_poll();
    InputMessage message;
    while (input_next_message(&message)) handle_message(&message);

    if (terminal_tty && !tty_session_active(terminal_tty)) {
        tty_release(terminal_tty);
        terminal_tty = TTY_ID_INVALID;
    }

    if (cursor_visible && (pointer_x != cursor_x || pointer_y != cursor_y)) {
        mark_dirty(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
        mark_dirty(pointer_x, pointer_y, CURSOR_WIDTH, CURSOR_HEIGHT);
    }
    if (!dirty) return;
    restore_pointer();
    if (dirty_left < WINDOW_X + WINDOW_WIDTH && dirty_right > WINDOW_X &&
        dirty_top < WINDOW_Y + WINDOW_HEIGHT && dirty_bottom > WINDOW_Y) {
        graphics_paint_wallpaper(gpu_framebuffer(), WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
        if (terminal_visible) draw_terminal_window(gpu_framebuffer());
        mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
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
int desktop_terminal_visible(void) { return terminal_visible; }

void desktop_terminal_output(TtyId id, void *context, const unsigned char *bytes, size_t length) {
    if (id != terminal_tty || context != &terminal || !bytes) return;
    for (size_t i = 0; i < length; ++i) terminal_model_feed(&terminal, bytes[i]);
    if (terminal_visible) mark_dirty(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT);
}
