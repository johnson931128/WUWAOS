#include "ui/desktop/desktop.h"
#include "arch/riscv/clock.h"
#include "drivers/virtio/gpu.h"
#include "drivers/virtio/input.h"
#include "kernel/io/tty.h"
#include "kernel/proc/terminal_service.h"
#include "ui/graphics/primitives.h"
#include "ui/input/keymap.h"
#include "ui/assets/font_terminal.h"
#include "ui/terminal/renderer.h"
#include "ui/terminal/vt.h"

#define WINDOW_LIMIT 4U
#define CURSOR_WIDTH 16U
#define CURSOR_HEIGHT 16U
#define DEFAULT_WINDOW_WIDTH (TERMINAL_COLUMNS * TERMINAL_FONT_WIDTH + 16U)
#define DEFAULT_WINDOW_HEIGHT (TERMINAL_ROWS * TERMINAL_FONT_HEIGHT + 16U + TITLE_HEIGHT)
#define TITLE_HEIGHT 28U
#define WINDOW_MIN_WIDTH 240U
#define WINDOW_MIN_HEIGHT 150U
#define BODY_INSET 8U
#define ICON_X 20U
#define ICON_Y 18U
#define ICON_WIDTH 72U
#define ICON_HEIGHT 78U
#define FEEDBACK_MS 2600UL

typedef struct {
    TerminalModel terminal;
    TtyId tty;
    uint32_t x, y, width, height;
    int allocated;
} DesktopWindow;

static const char pointer_shape[CURSOR_HEIGHT][CURSOR_WIDTH + 1U] = {
    "X...............", "XX..............", "XWX.............", "XWWX............",
    "XWWWX...........", "XWWWWX..........", "XWWWWWX.........", "XWWWWWWX........",
    "XWWWWWWWX.......", "XWWXWWWWX.......", "XWX.XWWX........", "XX..XWWX........",
    "....XWWX........", ".....XWWX.......", "......XX........", "................"
};
static DesktopWindow windows[WINDOW_LIMIT];
static uint8_t z_order[WINDOW_LIMIT];
static unsigned int z_count;
static int focused_slot = -1, drag_slot = -1, resize_slot = -1;
static uint32_t drag_offset_x, drag_offset_y, resize_start_x, resize_start_y;
static uint32_t resize_start_width, resize_start_height;
static uint32_t cursor_saved[CURSOR_HEIGHT][CURSOR_WIDTH];
static uint32_t cursor_x, cursor_y, pointer_x = GPU_WIDTH / 2U, pointer_y = GPU_HEIGHT / 2U;
static int cursor_visible, mouse_left_down;
static uint32_t dirty_left, dirty_top, dirty_right, dirty_bottom;
static int dirty;
static const char *feedback_text;
static uint64_t feedback_deadline;

static void mark_dirty(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!width || !height || x >= GPU_WIDTH || y >= GPU_HEIGHT) return;
    uint32_t right = x + width, bottom = y + height;
    if (right > GPU_WIDTH) right = GPU_WIDTH;
    if (bottom > GPU_HEIGHT) bottom = GPU_HEIGHT;
    if (!dirty) {
        dirty_left = x; dirty_top = y; dirty_right = right; dirty_bottom = bottom; dirty = 1;
    } else {
        if (x < dirty_left) dirty_left = x;
        if (y < dirty_top) dirty_top = y;
        if (right > dirty_right) dirty_right = right;
        if (bottom > dirty_bottom) dirty_bottom = bottom;
    }
}

static void mark_window_dirty(const DesktopWindow *window) {
    if (window) mark_dirty(window->x, window->y, window->width, window->height);
}

static void set_feedback(const char *message) {
    feedback_text = message;
    feedback_deadline = clock_ticks() + clock_ms_ticks(FEEDBACK_MS);
    mark_dirty(GPU_WIDTH - 420U, 8U, 412U, 34U);
}

static int window_slot_valid(int slot) {
    return slot >= 0 && (unsigned int)slot < WINDOW_LIMIT && windows[slot].allocated;
}

static void make_window_top(unsigned int slot) {
    unsigned int at = 0;
    while (at < z_count && z_order[at] != slot) ++at;
    if (at == z_count) return;
    while (at + 1U < z_count) { z_order[at] = z_order[at + 1U]; ++at; }
    z_order[z_count - 1U] = (uint8_t)slot;
}

static void focus_window(int slot) {
    if (!window_slot_valid(slot)) slot = -1;
    if (focused_slot != slot) {
        if (window_slot_valid(focused_slot)) mark_window_dirty(&windows[focused_slot]);
        focused_slot = slot;
        if (window_slot_valid(focused_slot)) mark_window_dirty(&windows[focused_slot]);
    }
    if (slot >= 0) make_window_top((unsigned int)slot);
}

static void draw_window(uint32_t *framebuffer, unsigned int slot) {
    const DesktopWindow *window = &windows[slot];
    const int focused = focused_slot == (int)slot;
    const uint32_t title_color = focused ? 0x002b4256U : 0x00384753U;
    graphics_fill_rect(framebuffer, window->x, window->y, window->width, window->height, 0x00e0e9efU);
    graphics_fill_rect(framebuffer, window->x + 2U, window->y + 2U,
        window->width - 4U, window->height - 4U, 0x0014212dU);
    graphics_fill_rect(framebuffer, window->x + 2U, window->y + 2U,
        window->width - 4U, TITLE_HEIGHT, title_color);
    graphics_draw_text(framebuffer, window->x + 12U, window->y + 7U,
        "Terminal - WUWAOS", 0x00f1f6faU, title_color);
    graphics_fill_rect(framebuffer, window->x + window->width - 30U, window->y + 5U,
        20U, 18U, 0x00b84d55U);
    graphics_draw_glyph(framebuffer, window->x + window->width - 26U,
        window->y + 6U, 'x', 0x00ffffffU, 0x00b84d55U);
    const uint32_t body_x = window->x + BODY_INSET;
    const uint32_t body_y = window->y + TITLE_HEIGHT + BODY_INSET;
    const uint32_t body_width = window->width - BODY_INSET * 2U;
    const uint32_t body_height = window->height - TITLE_HEIGHT - BODY_INSET * 2U;
    graphics_fill_rect(framebuffer, body_x, body_y, body_width, body_height, 0x00131b25U);
    terminal_renderer_draw_viewport(framebuffer, &window->terminal, body_x, body_y,
        body_width, body_height, focused);
    for (unsigned int line = 0; line < 3U; ++line) {
        const uint32_t inset = 5U + line * 4U;
        graphics_fill_rect(framebuffer, window->x + window->width - inset - 1U,
            window->y + window->height - 3U, 2U, 2U, 0x00687884U);
        graphics_fill_rect(framebuffer, window->x + window->width - 3U,
            window->y + window->height - inset - 1U, 2U, 2U, 0x00687884U);
    }
}

static void draw_feedback(uint32_t *framebuffer) {
    if (!feedback_text) return;
    const uint32_t x = GPU_WIDTH - 420U, y = 8U;
    graphics_fill_rect(framebuffer, x, y, 412U, 34U, 0x00324758U);
    graphics_fill_rect(framebuffer, x + 2U, y + 2U, 408U, 30U, 0x0014212dU);
    graphics_draw_text(framebuffer, x + 10U, y + 9U, feedback_text, 0x00ffffffU, 0x0014212dU);
}

static void draw_scene(uint32_t *framebuffer) {
    graphics_paint_wallpaper(framebuffer, 0, 0, GPU_WIDTH, GPU_HEIGHT);
    if (input_keyboard_ready()) graphics_draw_terminal_icon(framebuffer);
    for (unsigned int at = 0; at < z_count; ++at) {
        const unsigned int slot = z_order[at];
        if (windows[slot].allocated) draw_window(framebuffer, slot);
    }
    draw_feedback(framebuffer);
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
    draw_scene(framebuffer);
    (void)gpu_present(0, 0, GPU_WIDTH, GPU_HEIGHT);
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

static int hit_window(uint32_t x, uint32_t y, const DesktopWindow *window) {
    return x >= window->x && x < window->x + window->width &&
        y >= window->y && y < window->y + window->height;
}

static int hit_close(uint32_t x, uint32_t y, const DesktopWindow *window) {
    return x >= window->x + window->width - 32U && x < window->x + window->width - 6U &&
        y >= window->y + 4U && y < window->y + 25U;
}

static int hit_resize(uint32_t x, uint32_t y, const DesktopWindow *window) {
    return x >= window->x + window->width - 20U && x < window->x + window->width &&
        y >= window->y + window->height - 20U && y < window->y + window->height;
}

static int hit_title(uint32_t x, uint32_t y, const DesktopWindow *window) {
    return x >= window->x && x < window->x + window->width &&
        y >= window->y && y < window->y + TITLE_HEIGHT;
}

static int top_window_at(uint32_t x, uint32_t y) {
    for (unsigned int at = z_count; at > 0; --at) {
        const unsigned int slot = z_order[at - 1U];
        if (windows[slot].allocated && hit_window(x, y, &windows[slot])) return (int)slot;
    }
    return -1;
}

static void remove_window_order(unsigned int slot) {
    unsigned int at = 0;
    while (at < z_count && z_order[at] != slot) ++at;
    if (at == z_count) return;
    while (at + 1U < z_count) { z_order[at] = z_order[at + 1U]; ++at; }
    --z_count;
}

static void close_window(unsigned int slot) {
    DesktopWindow *window = &windows[slot];
    mark_window_dirty(window);
    if (window->tty) {
        (void)tty_session_close(window->tty);
        tty_release(window->tty);
    }
    remove_window_order(slot);
    *window = (DesktopWindow){0};
    if (focused_slot == (int)slot) {
        focused_slot = z_count ? (int)z_order[z_count - 1U] : -1;
        if (window_slot_valid(focused_slot)) mark_window_dirty(&windows[focused_slot]);
    }
    if (drag_slot == (int)slot) drag_slot = -1;
    if (resize_slot == (int)slot) resize_slot = -1;
}

static void feed_text(TerminalModel *terminal, const char *text) {
    while (*text) terminal_model_feed(terminal, (unsigned char)*text++);
}

static void create_window(void) {
    unsigned int slot = 0;
    while (slot < WINDOW_LIMIT && windows[slot].allocated) ++slot;
    if (slot == WINDOW_LIMIT) { set_feedback("Terminal window capacity is full"); return; }

    DesktopWindow *window = &windows[slot];
    *window = (DesktopWindow){ .x = (GPU_WIDTH - DEFAULT_WINDOW_WIDTH) / 2U + slot * 32U,
        .y = 88U + slot * 32U, .width = DEFAULT_WINDOW_WIDTH,
        .height = DEFAULT_WINDOW_HEIGHT, .allocated = 1 };
    if (window->x + window->width > GPU_WIDTH) window->x = GPU_WIDTH - window->width;
    if (window->y + window->height > GPU_HEIGHT) window->y = GPU_HEIGHT - window->height;
    terminal_model_init(&window->terminal);
    feed_text(&window->terminal, "WUWAOS Terminal\n");
    z_order[z_count++] = (uint8_t)slot;
    focus_window((int)slot);

    TtyId id = TTY_ID_INVALID;
    const long pid = process_start_gui_terminal(desktop_terminal_output, window, &id);
    if (pid < 0) {
        feed_text(&window->terminal, "Unable to start shell: process table or memory is full.\n");
        set_feedback("Unable to start shell for new window");
    } else {
        window->tty = id;
        set_feedback("Terminal window opened");
    }
    mark_window_dirty(window);
}

static void handle_key(const InputMessage *message) {
    unsigned char bytes[3];
    const size_t length = ui_keymap_translate(message->code, message->value != 0, bytes);
    if (length && window_slot_valid(focused_slot) && windows[focused_slot].tty)
        (void)tty_input_push(windows[focused_slot].tty, bytes, length);
}

static void move_or_resize_window(void) {
    if (window_slot_valid(drag_slot)) {
        DesktopWindow *window = &windows[drag_slot];
        const uint32_t old_x = window->x, old_y = window->y;
        uint32_t next_x = pointer_x > drag_offset_x ? pointer_x - drag_offset_x : 0;
        uint32_t next_y = pointer_y > drag_offset_y ? pointer_y - drag_offset_y : 0;
        if (next_x + window->width > GPU_WIDTH) next_x = GPU_WIDTH - window->width;
        if (next_y + window->height > GPU_HEIGHT) next_y = GPU_HEIGHT - window->height;
        if (next_x != old_x || next_y != old_y) {
            mark_window_dirty(window);
            window->x = next_x; window->y = next_y;
            mark_window_dirty(window);
        }
    } else if (window_slot_valid(resize_slot)) {
        DesktopWindow *window = &windows[resize_slot];
        const uint32_t old_width = window->width, old_height = window->height;
        uint32_t width = resize_start_width + (pointer_x >= resize_start_x ?
            pointer_x - resize_start_x : 0U);
        uint32_t height = resize_start_height + (pointer_y >= resize_start_y ?
            pointer_y - resize_start_y : 0U);
        if (pointer_x < resize_start_x) {
            const uint32_t shrink = resize_start_x - pointer_x;
            width = shrink < resize_start_width - WINDOW_MIN_WIDTH ? resize_start_width - shrink : WINDOW_MIN_WIDTH;
        }
        if (pointer_y < resize_start_y) {
            const uint32_t shrink = resize_start_y - pointer_y;
            height = shrink < resize_start_height - WINDOW_MIN_HEIGHT ? resize_start_height - shrink : WINDOW_MIN_HEIGHT;
        }
        if (width < WINDOW_MIN_WIDTH) width = WINDOW_MIN_WIDTH;
        if (height < WINDOW_MIN_HEIGHT) height = WINDOW_MIN_HEIGHT;
        if (width > GPU_WIDTH - window->x) width = GPU_WIDTH - window->x;
        if (height > GPU_HEIGHT - window->y) height = GPU_HEIGHT - window->y;
        if (width != old_width || height != old_height) {
            mark_window_dirty(window);
            window->width = width; window->height = height;
            mark_window_dirty(window);
        }
    }
}

static void handle_message(const InputMessage *message) {
    if (message->type == INPUT_MESSAGE_RESET) {
        ui_keymap_reset(); mouse_left_down = 0; drag_slot = resize_slot = -1;
    } else if (message->type == INPUT_MESSAGE_POINTER) {
        pointer_x = message->x; pointer_y = message->y;
        if (pointer_x > GPU_WIDTH - CURSOR_WIDTH) pointer_x = GPU_WIDTH - CURSOR_WIDTH;
        if (pointer_y > GPU_HEIGHT - CURSOR_HEIGHT) pointer_y = GPU_HEIGHT - CURSOR_HEIGHT;
        if (mouse_left_down) move_or_resize_window();
    } else if (message->type == INPUT_MESSAGE_KEY) {
        handle_key(message);
    } else if (message->type == INPUT_MESSAGE_BUTTON && message->code == 272U) {
        if (message->value == 1) {
            mouse_left_down = 1;
            const int slot = top_window_at(pointer_x, pointer_y);
            if (slot >= 0) {
                DesktopWindow *window = &windows[slot];
                focus_window(slot);
                if (hit_close(pointer_x, pointer_y, window)) close_window((unsigned int)slot);
                else if (hit_resize(pointer_x, pointer_y, window)) {
                    resize_slot = slot; drag_slot = -1;
                    resize_start_x = pointer_x; resize_start_y = pointer_y;
                    resize_start_width = window->width; resize_start_height = window->height;
                } else if (hit_title(pointer_x, pointer_y, window)) {
                    drag_slot = slot; resize_slot = -1;
                    drag_offset_x = pointer_x - window->x; drag_offset_y = pointer_y - window->y;
                } else { drag_slot = resize_slot = -1; }
            } else if (hit_icon(pointer_x, pointer_y) && input_keyboard_ready()) {
                drag_slot = resize_slot = -1;
                create_window();
            } else {
                drag_slot = resize_slot = -1;
                focus_window(-1);
            }
        } else {
            mouse_left_down = 0; drag_slot = resize_slot = -1;
        }
    }
}

void desktop_poll(void) {
    gpu_poll();
    if (!gpu_is_enabled()) return;
    input_poll();
    InputMessage message;
    while (input_next_message(&message)) handle_message(&message);

    for (unsigned int slot = 0; slot < WINDOW_LIMIT; ++slot) {
        DesktopWindow *window = &windows[slot];
        if (window->allocated && window->tty && !tty_session_active(window->tty)) {
            tty_release(window->tty);
            window->tty = TTY_ID_INVALID;
            set_feedback("Terminal session ended");
        }
    }

    if (feedback_text && clock_ticks() >= feedback_deadline) {
        feedback_text = 0;
        mark_dirty(GPU_WIDTH - 420U, 8U, 412U, 34U);
    }
    if (cursor_visible && (pointer_x != cursor_x || pointer_y != cursor_y)) {
        mark_dirty(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
        mark_dirty(pointer_x, pointer_y, CURSOR_WIDTH, CURSOR_HEIGHT);
    }
    if (!dirty) return;
    restore_pointer();
    draw_scene(gpu_framebuffer());
    cursor_x = pointer_x; cursor_y = pointer_y;
    if (cursor_visible) {
        draw_pointer();
        mark_dirty(cursor_x, cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
    }
    const uint32_t x = dirty_left, y = dirty_top;
    const uint32_t width = dirty_right - dirty_left, height = dirty_bottom - dirty_top;
    dirty = 0;
    (void)gpu_present(x, y, width, height);
}

int desktop_event_source_active(void) { return cursor_visible && gpu_is_enabled(); }

int desktop_terminal_visible(void) {
    for (unsigned int slot = 0; slot < WINDOW_LIMIT; ++slot)
        if (windows[slot].allocated) return 1;
    return 0;
}

void desktop_terminal_output(TtyId id, void *context, const unsigned char *bytes, size_t length) {
    DesktopWindow *window = context;
    if (!window || !window->allocated || id != window->tty || !bytes) return;
    for (size_t i = 0; i < length; ++i) terminal_model_feed(&window->terminal, bytes[i]);
    mark_window_dirty(window);
}
