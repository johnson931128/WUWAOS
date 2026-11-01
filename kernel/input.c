#include "input.h"
#include "clock.h"
#include "console.h"
#include "gpu.h"
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
#define QUEUE_SIZE 8U
#define EV_SYN 0U
#define EV_ABS 3U
#define SYN_REPORT 0U
#define ABS_X 0U
#define ABS_Y 1U
#define CONFIG_ABS_INFO 0x12U

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
    VirtqDescriptor descriptors[QUEUE_SIZE] __attribute__((aligned(16)));
    VirtqAvailable available __attribute__((aligned(2)));
    VirtqUsed used __attribute__((aligned(4)));
    InputEvent events[QUEUE_SIZE] __attribute__((aligned(16)));
    AbsInfo axes[2];
    uint32_t x, y;
    int pending_changed;
    int initialized;
} InputDevice;
static InputDevice tablet;
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
        if (end - at == sizeof(target) - 1) {
            uint32_t i = 0;
            while (i < sizeof(target) - 1 && bytes[at + i] == (uint8_t)target[i]) ++i;
            if (i == sizeof(target) - 1) return 1;
        }
        at = end + 1;
    }
    return 0;
}

int input_probe(const void *blob) {
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
            nodes[depth] = (FdtNode){ .address_cells = depth == 0 ? 2 : nodes[depth - 1].address_cells,
                                     .size_cells = depth == 0 ? 1 : nodes[depth - 1].size_cells };
            cursor = (const uint8_t *)(((uintptr_t)(end + 1) + 3U) & ~(uintptr_t)3U);
        } else if (token == FDT_END_NODE) {
            if (depth < 0) return -1;
            FdtNode *node = &nodes[depth];
            if (node->compatible && node->has_reg) {
                volatile uint32_t *candidate = (volatile uint32_t *)(uintptr_t)node->address;
                if (candidate[0] == 0x74726976U && candidate[1] == 2U && candidate[2] == 18U) {
                    tablet.base = (uintptr_t)node->address; tablet.registers = candidate;
                    console_puts("[input] FDT VirtIO tablet at "); console_hex(tablet.base); console_putc('\n');
                    return 0;
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
                if (parent->address_cells == 2 && length >= 16) {
                    node->address = ((uint64_t)load_be32(value) << 32) | load_be32(value + 4); node->has_reg = 1;
                } else if (parent->address_cells == 1 && length >= 8) {
                    node->address = load_be32(value); node->has_reg = 1;
                }
            }
            cursor = (const uint8_t *)(((uintptr_t)(cursor + length) + 3U) & ~(uintptr_t)3U);
        } else if (token == FDT_NOP) continue;
        else if (token == FDT_END) break;
        else return -1;
    }
    console_puts("[input] No modern VirtIO tablet in FDT\n");
    return -1;
}

uintptr_t input_mmio_base(void) { return tablet.base; }
static void write_address(uint32_t offset, uintptr_t address) {
    tablet.registers[offset / 4] = (uint32_t)address;
    tablet.registers[(offset + 4) / 4] = (uint32_t)(address >> 32);
}
static uint32_t load_le32(const volatile uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}
static void query_abs_info(uint8_t axis, AbsInfo *result) {
    volatile uint8_t *config = (volatile uint8_t *)tablet.registers + VIRTIO_CONFIG;
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

int input_init(void) {
    if (!tablet.registers || tablet.registers[VIRTIO_MAGIC / 4] != 0x74726976U ||
        tablet.registers[VIRTIO_VERSION / 4] != 2U || tablet.registers[VIRTIO_DEVICE_ID / 4] != 18U) return -1;
    tablet.registers[VIRTIO_STATUS / 4] = 0;
    tablet.registers[VIRTIO_STATUS / 4] = DEVICE_ACKNOWLEDGE;
    tablet.registers[VIRTIO_STATUS / 4] |= DEVICE_DRIVER;
    tablet.registers[VIRTIO_DEVICE_FEATURES_SEL / 4] = 1;
    const uint32_t high_features = tablet.registers[VIRTIO_DEVICE_FEATURES / 4];
    tablet.registers[VIRTIO_DRIVER_FEATURES_SEL / 4] = 0;
    tablet.registers[VIRTIO_DRIVER_FEATURES / 4] = 0;
    tablet.registers[VIRTIO_DRIVER_FEATURES_SEL / 4] = 1;
    if ((high_features & 1U) == 0) { console_puts("[input] Device lacks VERSION_1 feature\n"); return -1; }
    tablet.registers[VIRTIO_DRIVER_FEATURES / 4] = 1U << FEATURE_VERSION_1;
    tablet.registers[VIRTIO_STATUS / 4] |= DEVICE_FEATURES_OK;
    if ((tablet.registers[VIRTIO_STATUS / 4] & DEVICE_FEATURES_OK) == 0) {
        console_puts("[input] Feature negotiation rejected\n"); return -1;
    }
    tablet.registers[VIRTIO_QUEUE_SEL / 4] = 0;
    const uint32_t maximum = tablet.registers[VIRTIO_QUEUE_NUM_MAX / 4];
    if (maximum < 2) { console_puts("[input] Event queue must hold at least two descriptors\n"); return -1; }
    tablet.queue_length = maximum < QUEUE_SIZE ? (uint16_t)maximum : QUEUE_SIZE;
    tablet.registers[VIRTIO_QUEUE_NUM / 4] = tablet.queue_length;
    for (uint16_t i = 0; i < tablet.queue_length; ++i) {
        tablet.descriptors[i] = (VirtqDescriptor){ (uintptr_t)&tablet.events[i], sizeof(InputEvent), 2U, 0U };
        tablet.available.ring[i] = i;
    }
    __asm__ volatile("fence rw, w" : : : "memory");
    tablet.available.index = tablet.queue_length;
    write_address(VIRTIO_QUEUE_DESC_LOW, (uintptr_t)tablet.descriptors);
    write_address(VIRTIO_QUEUE_AVAIL_LOW, (uintptr_t)&tablet.available);
    write_address(VIRTIO_QUEUE_USED_LOW, (uintptr_t)&tablet.used);
    tablet.registers[VIRTIO_QUEUE_READY / 4] = 1;
    tablet.registers[VIRTIO_STATUS / 4] |= DEVICE_DRIVER_OK;
    query_abs_info(ABS_X, &tablet.axes[ABS_X]);
    query_abs_info(ABS_Y, &tablet.axes[ABS_Y]);
    if (tablet.axes[ABS_X].maximum <= tablet.axes[ABS_X].minimum ||
        tablet.axes[ABS_Y].maximum <= tablet.axes[ABS_Y].minimum) {
        tablet.axes[ABS_X] = (AbsInfo){ .maximum = 32767 };
        tablet.axes[ABS_Y] = (AbsInfo){ .maximum = 32767 };
    }
    tablet.x = GPU_WIDTH / 2U; tablet.y = GPU_HEIGHT / 2U;
    tablet.registers[VIRTIO_QUEUE_NOTIFY / 4] = 0;
    tablet.initialized = 1;
    console_puts("[input] VirtIO absolute tablet ready, X="); console_number((long)tablet.axes[ABS_X].maximum);
    console_puts(" Y="); console_number((long)tablet.axes[ABS_Y].maximum); console_putc('\n');
    return 0;
}

int input_poll(uint32_t *x, uint32_t *y) {
    if (!tablet.initialized) return 0;
    int consumed_event = 0;
    while (tablet.used.index != tablet.consumed) {
        __asm__ volatile("fence r, rw" : : : "memory");
        const uint16_t used_index = tablet.consumed % tablet.queue_length;
        const uint32_t id = tablet.used.ring[used_index].id;
        if (id >= tablet.queue_length) {
            console_puts("[input] Invalid event descriptor ID\n");
            tablet.registers[VIRTIO_STATUS / 4] |= 0x80U;
            return -1;
        }
        const InputEvent event = tablet.events[id];
        ++tablet.consumed;
        consumed_event = 1;
        tablet.available.ring[tablet.available.index % tablet.queue_length] = (uint16_t)id;
        __asm__ volatile("fence rw, w" : : : "memory");
        ++tablet.available.index;
        if (event.type == EV_ABS && event.code == ABS_X) {
            tablet.x = normalize_axis(event.value, &tablet.axes[ABS_X], GPU_WIDTH - 1U); tablet.pending_changed = 1;
        } else if (event.type == EV_ABS && event.code == ABS_Y) {
            tablet.y = normalize_axis(event.value, &tablet.axes[ABS_Y], GPU_HEIGHT - 1U); tablet.pending_changed = 1;
        } else if (event.type == EV_SYN && event.code == SYN_REPORT && tablet.pending_changed) {
            if (consumed_event) tablet.registers[VIRTIO_QUEUE_NOTIFY / 4] = 0;
            tablet.pending_changed = 0;
            *x = tablet.x; *y = tablet.y; return 1;
        }
    }
    if (consumed_event) tablet.registers[VIRTIO_QUEUE_NOTIFY / 4] = 0;
    return 0;
}
