#include "drivers/virtio/input.h"
#include "arch/riscv/clock.h"
#include "kernel/io/console.h"
#include <stddef.h>

#define FDT_MAGIC 0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U
#define VIRTIO_MAGIC 0x000U
#define VIRTIO_VERSION 0x004U
#define VIRTIO_DEVICE_ID 0x008U
#define VIRTIO_DEVICE_FEATURES 0x010U
#define VIRTIO_DEVICE_FEATURES_SEL 0x014U
#define VIRTIO_DRIVER_FEATURES 0x020U
#define VIRTIO_DRIVER_FEATURES_SEL 0x024U
#define VIRTIO_QUEUE_SEL 0x030U
#define VIRTIO_QUEUE_NUM_MAX 0x034U
#define VIRTIO_QUEUE_NUM 0x038U
#define VIRTIO_QUEUE_READY 0x044U
#define VIRTIO_QUEUE_NOTIFY 0x050U
#define VIRTIO_STATUS 0x070U
#define VIRTIO_QUEUE_DESC_LOW 0x080U
#define VIRTIO_QUEUE_AVAIL_LOW 0x090U
#define VIRTIO_QUEUE_USED_LOW 0x0a0U
#define VIRTIO_CONFIG 0x100U
#define DEVICE_ACKNOWLEDGE 1U
#define DEVICE_DRIVER 2U
#define DEVICE_DRIVER_OK 4U
#define DEVICE_FEATURES_OK 8U
#define FEATURE_VERSION_1 0U
#define QUEUE_SIZE 32U
#define MESSAGE_CAPACITY 256U
#define EV_SYN 0U
#define EV_KEY 1U
#define EV_ABS 3U
#define SYN_REPORT 0U
#define ABS_X 0U
#define ABS_Y 1U
#define BTN_LEFT 272U
#define KEY_A 30U
#define CONFIG_ABS_INFO 0x12U
#define CONFIG_EV_BITS 0x11U
#define CONFIG_DATA_BYTES 128U
#define INPUT_DEVICE_LIMIT 8U
#define ROLE_TABLET 1U
#define ROLE_KEYBOARD 2U

typedef struct { uint64_t address; uint32_t length; uint16_t flags, next; } VirtqDescriptor;
typedef struct { uint16_t flags; volatile uint16_t index; uint16_t ring[QUEUE_SIZE]; uint16_t event; } VirtqAvailable;
typedef struct { uint32_t id, length; } VirtqUsedElement;
typedef struct { uint16_t flags; volatile uint16_t index; VirtqUsedElement ring[QUEUE_SIZE]; uint16_t event; } VirtqUsed;
typedef struct { uint16_t type, code; int32_t value; } InputEvent;
typedef struct { uint32_t minimum, maximum, fuzz, flat, resolution; } AbsInfo;
typedef struct {
    volatile uint32_t *registers;
    uintptr_t base;
    uint16_t consumed, queue_length;
    uint8_t role;
    VirtqDescriptor descriptors[QUEUE_SIZE] __attribute__((aligned(16)));
    VirtqAvailable available __attribute__((aligned(2)));
    VirtqUsed used __attribute__((aligned(4)));
    InputEvent events[QUEUE_SIZE] __attribute__((aligned(16)));
    AbsInfo axes[2];
    uint32_t x, y;
    int pointer_changed;
    InputMessage pending[QUEUE_SIZE];
    uint16_t pending_count;
    int initialized;
} InputDevice;

static InputDevice devices[2];
static unsigned int device_count;
static InputMessage messages[MESSAGE_CAPACITY];
static uint32_t message_read, message_write;
static unsigned int overflow_count;
static uint32_t pointer_max_x, pointer_max_y;
typedef struct { uint32_t address_cells, size_cells; uint64_t address; int compatible, has_reg; } FdtNode;

static uint32_t load_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static int compatible_virtio(const uint8_t *bytes, uint32_t length) {
    static const char target[] = "virtio,mmio";
    for (uint32_t at = 0; at < length;) {
        uint32_t end = at;
        while (end < length && bytes[end]) ++end;
        if (end - at == sizeof(target) - 1U) {
            uint32_t i = 0;
            while (i < sizeof(target) - 1U && bytes[at + i] == (uint8_t)target[i]) ++i;
            if (i == sizeof(target) - 1U) return 1;
        }
        at = end + 1U;
    }
    return 0;
}

static int config_has_bit(volatile uint8_t *config, uint8_t event_type, uint8_t code) {
    config[0] = CONFIG_EV_BITS;
    config[1] = event_type;
    for (volatile unsigned int delay = 0; delay < 32; ++delay) __asm__ volatile("nop");
    const uint8_t size = config[2];
    const unsigned int byte = code / 8U;
    return byte < size && byte < CONFIG_DATA_BYTES &&
        (config[8U + byte] & (uint8_t)(1U << (code % 8U))) != 0;
}

static int identify_device(volatile uint32_t *registers) {
    volatile uint8_t *config = (volatile uint8_t *)registers + VIRTIO_CONFIG;
    const int abs_x = config_has_bit(config, EV_ABS, ABS_X);
    const int abs_y = config_has_bit(config, EV_ABS, ABS_Y);
    if (abs_x && abs_y) return ROLE_TABLET;
    return config_has_bit(config, EV_KEY, KEY_A) ? ROLE_KEYBOARD : 0;
}

int input_probe(const void *blob) {
    device_count = 0;
    if (!blob) return -1;
    const uint8_t *bytes = blob;
    if (load_be32(bytes) != FDT_MAGIC) return -1;
    const uint32_t struct_offset = load_be32(bytes + 8), strings_offset = load_be32(bytes + 12);
    const uint32_t strings_size = load_be32(bytes + 32), struct_size = load_be32(bytes + 36);
    const uint8_t *cursor = bytes + struct_offset, *limit = cursor + struct_size, *strings = bytes + strings_offset;
    FdtNode nodes[16] = {{0}};
    int depth = -1;
    while (cursor + 4 <= limit) {
        const uint32_t token = load_be32(cursor); cursor += 4;
        if (token == FDT_BEGIN_NODE) {
            const uint8_t *end = cursor;
            while (end < limit && *end) ++end;
            if (end == limit || depth + 1 >= 16) return -1;
            ++depth;
            nodes[depth] = (FdtNode){ .address_cells = depth == 0 ? 2U : nodes[depth - 1].address_cells,
                .size_cells = depth == 0 ? 1U : nodes[depth - 1].size_cells };
            cursor = (const uint8_t *)(((uintptr_t)(end + 1) + 3U) & ~(uintptr_t)3U);
        } else if (token == FDT_END_NODE) {
            if (depth < 0) return -1;
            FdtNode *node = &nodes[depth];
            if (node->compatible && node->has_reg) {
                volatile uint32_t *candidate = (volatile uint32_t *)(uintptr_t)node->address;
                if (candidate[0] == 0x74726976U && candidate[1] == 2U && candidate[2] == 18U) {
                    const int role = identify_device(candidate);
                    if (role && (role == ROLE_TABLET ? !devices[0].registers : !devices[1].registers)) {
                        const unsigned int index = role == ROLE_TABLET ? 0U : 1U;
                        devices[index].base = (uintptr_t)node->address;
                        devices[index].registers = candidate;
                        devices[index].role = (uint8_t)role;
                        if (device_count <= index) device_count = index + 1U;
                        console_puts(role == ROLE_TABLET ? "[input] FDT VirtIO tablet at " : "[input] FDT VirtIO keyboard at ");
                        console_hex(node->address); console_putc('\n');
                    }
                }
            }
            --depth;
        } else if (token == FDT_PROP) {
            if (depth < 0 || cursor + 8 > limit) return -1;
            const uint32_t length = load_be32(cursor), name_offset = load_be32(cursor + 4); cursor += 8;
            if (cursor + length > limit || name_offset >= strings_size) return -1;
            const char *name = (const char *)(strings + name_offset);
            const uint8_t *value = cursor;
            FdtNode *node = &nodes[depth];
            if (name[0] == '#' && name[1] == 'a' && length == 4) node->address_cells = load_be32(value);
            else if (name[0] == '#' && name[1] == 's' && length == 4) node->size_cells = load_be32(value);
            else if (name[0] == 'c' && compatible_virtio(value, length)) node->compatible = 1;
            else if (name[0] == 'r' && name[1] == 'e' && name[2] == 'g' && depth > 0) {
                const FdtNode *parent = &nodes[depth - 1];
                if (parent->address_cells == 2U && length >= 16U) {
                    node->address = ((uint64_t)load_be32(value) << 32) | load_be32(value + 4); node->has_reg = 1;
                } else if (parent->address_cells == 1U && length >= 8U) {
                    node->address = load_be32(value); node->has_reg = 1;
                }
            }
            cursor = (const uint8_t *)(((uintptr_t)(cursor + length) + 3U) & ~(uintptr_t)3U);
        } else if (token == FDT_NOP) continue;
        else if (token == FDT_END) break;
        else return -1;
    }
    return devices[0].registers ? 0 : -1;
}

unsigned int input_device_count(void) { return device_count; }
uintptr_t input_mmio_base(unsigned int index) { return index < 2U ? devices[index].base : 0; }

void input_set_pointer_bounds(uint32_t max_x, uint32_t max_y) {
    pointer_max_x = max_x;
    pointer_max_y = max_y;
}

static void write_address(InputDevice *device, uint32_t offset, uintptr_t address) {
    device->registers[offset / 4U] = (uint32_t)address;
    device->registers[(offset + 4U) / 4U] = (uint32_t)(address >> 32);
}

static uint32_t load_le32(const volatile uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void query_abs_info(InputDevice *device, uint8_t axis, AbsInfo *result) {
    volatile uint8_t *config = (volatile uint8_t *)device->registers + VIRTIO_CONFIG;
    config[0] = CONFIG_ABS_INFO;
    config[1] = axis;
    for (volatile unsigned int delay = 0; delay < 32; ++delay) __asm__ volatile("nop");
    result->minimum = load_le32(config + 8);
    result->maximum = load_le32(config + 12);
    result->fuzz = load_le32(config + 16);
    result->flat = load_le32(config + 20);
    result->resolution = load_le32(config + 24);
}

static uint32_t normalize_axis(int32_t value, const AbsInfo *axis, uint32_t screen_max) {
    if (value <= (int32_t)axis->minimum) return 0;
    if (value >= (int32_t)axis->maximum) return screen_max;
    const uint64_t position = (uint32_t)value - axis->minimum;
    const uint64_t range = axis->maximum - axis->minimum;
    return (uint32_t)((position * screen_max) / range);
}

static int init_device(InputDevice *device) {
    if (!device->registers || device->registers[VIRTIO_MAGIC / 4U] != 0x74726976U ||
        device->registers[VIRTIO_VERSION / 4U] != 2U || device->registers[VIRTIO_DEVICE_ID / 4U] != 18U) return -1;
    device->registers[VIRTIO_STATUS / 4U] = 0;
    device->registers[VIRTIO_STATUS / 4U] = DEVICE_ACKNOWLEDGE;
    device->registers[VIRTIO_STATUS / 4U] |= DEVICE_DRIVER;
    device->registers[VIRTIO_DEVICE_FEATURES_SEL / 4U] = 1;
    const uint32_t high_features = device->registers[VIRTIO_DEVICE_FEATURES / 4U];
    device->registers[VIRTIO_DRIVER_FEATURES_SEL / 4U] = 0;
    device->registers[VIRTIO_DRIVER_FEATURES / 4U] = 0;
    device->registers[VIRTIO_DRIVER_FEATURES_SEL / 4U] = 1;
    if ((high_features & 1U) == 0) { console_puts("[input] Device lacks VERSION_1 feature\n"); return -1; }
    device->registers[VIRTIO_DRIVER_FEATURES / 4U] = 1U << FEATURE_VERSION_1;
    device->registers[VIRTIO_STATUS / 4U] |= DEVICE_FEATURES_OK;
    if ((device->registers[VIRTIO_STATUS / 4U] & DEVICE_FEATURES_OK) == 0) {
        console_puts("[input] Feature negotiation rejected\n"); return -1;
    }
    device->registers[VIRTIO_QUEUE_SEL / 4U] = 0;
    const uint32_t maximum = device->registers[VIRTIO_QUEUE_NUM_MAX / 4U];
    if (maximum < 2U) { console_puts("[input] Event queue must hold at least two descriptors\n"); return -1; }
    device->queue_length = maximum < QUEUE_SIZE ? (uint16_t)maximum : QUEUE_SIZE;
    device->registers[VIRTIO_QUEUE_NUM / 4U] = device->queue_length;
    for (uint16_t i = 0; i < device->queue_length; ++i) {
        device->descriptors[i] = (VirtqDescriptor){ (uintptr_t)&device->events[i], sizeof(InputEvent), 2U, 0U };
        device->available.ring[i] = i;
    }
    __asm__ volatile("fence rw, w" : : : "memory");
    device->available.index = device->queue_length;
    write_address(device, VIRTIO_QUEUE_DESC_LOW, (uintptr_t)device->descriptors);
    write_address(device, VIRTIO_QUEUE_AVAIL_LOW, (uintptr_t)&device->available);
    write_address(device, VIRTIO_QUEUE_USED_LOW, (uintptr_t)&device->used);
    device->registers[VIRTIO_QUEUE_READY / 4U] = 1;
    device->registers[VIRTIO_STATUS / 4U] |= DEVICE_DRIVER_OK;
    if (device->role == ROLE_TABLET) {
        query_abs_info(device, ABS_X, &device->axes[ABS_X]);
        query_abs_info(device, ABS_Y, &device->axes[ABS_Y]);
        if (device->axes[ABS_X].maximum <= device->axes[ABS_X].minimum ||
            device->axes[ABS_Y].maximum <= device->axes[ABS_Y].minimum) {
            device->axes[ABS_X] = (AbsInfo){ .maximum = 32767 };
            device->axes[ABS_Y] = (AbsInfo){ .maximum = 32767 };
        }
        device->x = (pointer_max_x + 1U) / 2U; device->y = (pointer_max_y + 1U) / 2U;
        console_puts("[input] VirtIO absolute tablet ready, X="); console_number((long)device->axes[ABS_X].maximum);
        console_puts(" Y="); console_number((long)device->axes[ABS_Y].maximum); console_putc('\n');
        } else console_puts("[input] VirtIO keyboard ready (US key mapping in UI)\n");
    device->registers[VIRTIO_QUEUE_NOTIFY / 4U] = 0;
    device->initialized = 1;
    return 0;
}

int input_init(void) {
    int ready = 0;
    for (unsigned int i = 0; i < 2U; ++i) {
        if (devices[i].registers && init_device(&devices[i]) == 0) ++ready;
    }
    return devices[0].initialized && ready ? 0 : -1;
}

int input_keyboard_ready(void) { return devices[1].initialized; }

static void push_message(InputMessage message) {
    if (message_write - message_read >= MESSAGE_CAPACITY) {
        message_read = message_write;
        ++overflow_count;
        messages[message_write++ % MESSAGE_CAPACITY] = (InputMessage){ .type = INPUT_MESSAGE_RESET };
    }
    messages[message_write++ % MESSAGE_CAPACITY] = message;
}

int input_next_message(InputMessage *message) {
    if (!message || message_read == message_write) return 0;
    *message = messages[message_read++ % MESSAGE_CAPACITY];
    return 1;
}

unsigned int input_overflow_count(void) { return overflow_count; }

static void commit_device_batch(InputDevice *device) {
    if (device->role == ROLE_TABLET && device->pointer_changed) {
        push_message((InputMessage){ .type = INPUT_MESSAGE_POINTER, .x = device->x, .y = device->y });
        device->pointer_changed = 0;
    }
    for (uint16_t i = 0; i < device->pending_count; ++i) push_message(device->pending[i]);
    device->pending_count = 0;
}

static void poll_device(InputDevice *device) {
    if (!device->initialized) return;
    int consumed = 0;
    while (device->used.index != device->consumed) {
        __asm__ volatile("fence r, rw" : : : "memory");
        const uint16_t used_index = device->consumed % device->queue_length;
        const uint32_t id = device->used.ring[used_index].id;
        if (id >= device->queue_length) {
            console_puts("[input] Invalid event descriptor ID\n");
            device->registers[VIRTIO_STATUS / 4U] |= 0x80U;
            return;
        }
        const InputEvent event = device->events[id];
        ++device->consumed;
        consumed = 1;
        device->available.ring[device->available.index % device->queue_length] = (uint16_t)id;
        __asm__ volatile("fence rw, w" : : : "memory");
        ++device->available.index;
        if (device->role == ROLE_TABLET && event.type == EV_ABS && event.code == ABS_X) {
            device->x = normalize_axis(event.value, &device->axes[ABS_X], pointer_max_x); device->pointer_changed = 1;
        } else if (device->role == ROLE_TABLET && event.type == EV_ABS && event.code == ABS_Y) {
            device->y = normalize_axis(event.value, &device->axes[ABS_Y], pointer_max_y); device->pointer_changed = 1;
        } else if (event.type == EV_KEY && event.value != 2 && device->pending_count < QUEUE_SIZE) {
            const uint8_t type = device->role == ROLE_TABLET && event.code == BTN_LEFT ? INPUT_MESSAGE_BUTTON : INPUT_MESSAGE_KEY;
            device->pending[device->pending_count++] = (InputMessage){ .type = type, .code = event.code, .value = event.value };
        } else if (event.type == EV_SYN && event.code == SYN_REPORT) commit_device_batch(device);
    }
    if (consumed) device->registers[VIRTIO_QUEUE_NOTIFY / 4U] = 0;
}

void input_poll(void) {
    poll_device(&devices[0]);
    poll_device(&devices[1]);
}
