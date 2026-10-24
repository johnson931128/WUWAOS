#include "block.h"
#include "console.h"
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
#define VIRTIO_QUEUE_DESC_HIGH 0x084U
#define VIRTIO_QUEUE_AVAIL_LOW 0x090U
#define VIRTIO_QUEUE_AVAIL_HIGH 0x094U
#define VIRTIO_QUEUE_USED_LOW 0x0a0U
#define VIRTIO_QUEUE_USED_HIGH 0x0a4U
#define VIRTIO_CONFIG 0x100U

#define DEVICE_ACKNOWLEDGE 1U
#define DEVICE_DRIVER 2U
#define DEVICE_DRIVER_OK 4U
#define DEVICE_FEATURES_OK 8U
#define FEATURE_VERSION_1 0U
#define QUEUE_SIZE 8U

typedef struct {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} VirtqDescriptor;

typedef struct {
    uint16_t flags;
    volatile uint16_t index;
    uint16_t ring[QUEUE_SIZE];
    uint16_t used_event;
} VirtqAvailable;

typedef struct {
    uint32_t id;
    uint32_t length;
} VirtqUsedElement;

typedef struct {
    uint16_t flags;
    volatile uint16_t index;
    VirtqUsedElement ring[QUEUE_SIZE];
    uint16_t available_event;
} VirtqUsed;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} BlockRequest;

static volatile uint32_t *device;
static volatile uint16_t consumed_used;
static uint16_t queue_size;
static uint64_t sectors;
static VirtqDescriptor descriptors[QUEUE_SIZE] __attribute__((aligned(16)));
static VirtqAvailable available __attribute__((aligned(2)));
static VirtqUsed used __attribute__((aligned(4)));
static BlockRequest request __attribute__((aligned(16)));
static uint8_t request_status;
static uint8_t *transfer_buffer;

static uint32_t load_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

typedef struct {
    uint32_t address_cells;
    uint32_t size_cells;
    uint64_t reg_address;
    uint32_t reg_size;
    int compatible;
    int has_reg;
} FdtNode;

static int compatible_virtio(const uint8_t *value, uint32_t length) {
    static const char target[] = "virtio,mmio";
    uint32_t at = 0;
    while (at < length) {
        uint32_t end = at;
        while (end < length && value[end] != 0) {
            ++end;
        }
        if (end - at == sizeof(target) - 1) {
            uint32_t i = 0;
            while (i < sizeof(target) - 1 && value[at + i] == (uint8_t)target[i]) {
                ++i;
            }
            if (i == sizeof(target) - 1) {
                return 1;
            }
        }
        at = end + 1;
    }
    return 0;
}

static int fdt_find_virtio(const void *blob, uintptr_t *base) {
    if (blob == 0) {
        return -1;
    }
    const uint8_t *bytes = blob;
    if (load_be32(bytes) != FDT_MAGIC) {
        return -1;
    }
    const uint32_t struct_offset = load_be32(bytes + 8);
    const uint32_t strings_offset = load_be32(bytes + 12);
    const uint32_t strings_size = load_be32(bytes + 32);
    const uint32_t struct_size = load_be32(bytes + 36);
    const uint8_t *cursor = bytes + struct_offset;
    const uint8_t *limit = cursor + struct_size;
    const uint8_t *strings = bytes + strings_offset;
    FdtNode nodes[16] = {{0}};
    int depth = -1;

    while (cursor + 4 <= limit) {
        const uint32_t token = load_be32(cursor);
        cursor += 4;
        if (token == FDT_BEGIN_NODE) {
            const uint8_t *name_end = cursor;
            while (name_end < limit && *name_end != 0) {
                ++name_end;
            }
            if (name_end == limit || depth + 1 >= 16) {
                return -1;
            }
            ++depth;
            nodes[depth] = (FdtNode){ .address_cells = depth == 0 ? 2 : nodes[depth - 1].address_cells,
                                      .size_cells = depth == 0 ? 1 : nodes[depth - 1].size_cells };
            cursor = (const uint8_t *)(((uintptr_t)(name_end + 1) + 3U) & ~(uintptr_t)3U);
        } else if (token == FDT_END_NODE) {
            if (depth < 0) {
                return -1;
            }
            FdtNode *node = &nodes[depth];
            if (node->compatible && node->has_reg && node->reg_size >= 0x100U) {
                volatile uint32_t *candidate = (volatile uint32_t *)(uintptr_t)node->reg_address;
                if (candidate[0] == 0x74726976U && candidate[1] == 2U && candidate[2] == 2U) {
                    *base = (uintptr_t)node->reg_address;
                    return 0;
                }
            }
            --depth;
        } else if (token == FDT_PROP) {
            if (depth < 0 || cursor + 8 > limit) {
                return -1;
            }
            const uint32_t length = load_be32(cursor);
            const uint32_t name_offset = load_be32(cursor + 4);
            cursor += 8;
            if (cursor + length > limit || name_offset >= strings_size) {
                return -1;
            }
            const char *name = (const char *)(strings + name_offset);
            const uint8_t *value = cursor;
            FdtNode *node = &nodes[depth];
            if (name[0] == '#' && name[1] == 'a' && length == 4) {
                node->address_cells = load_be32(value);
            } else if (name[0] == '#' && name[1] == 's' && length == 4) {
                node->size_cells = load_be32(value);
            } else if (name[0] == 'c' && compatible_virtio(value, length)) {
                node->compatible = 1;
            } else if (name[0] == 'r' && name[1] == 'e' && name[2] == 'g' && depth > 0) {
                const FdtNode *parent = &nodes[depth - 1];
                if (parent->address_cells == 2 && parent->size_cells == 2 && length >= 16) {
                    node->reg_address = ((uint64_t)load_be32(value) << 32) |
                                        load_be32(value + 4);
                    node->reg_size = load_be32(value + 12);
                    node->has_reg = 1;
                } else if (parent->address_cells == 1 && parent->size_cells == 1 && length >= 8) {
                    node->reg_address = load_be32(value);
                    node->reg_size = load_be32(value + 4);
                    node->has_reg = 1;
                }
            }
            cursor = (const uint8_t *)(((uintptr_t)(cursor + length) + 3U) & ~(uintptr_t)3U);
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            return -1;
        }
    }
    return -1;
}

static void write_address(uint32_t low_register, uintptr_t address) {
    device[low_register / 4] = (uint32_t)address;
    device[(low_register + 4) / 4] = (uint32_t)(address >> 32);
}

int block_init_from_fdt(const void *fdt, uintptr_t *mmio_base) {
    uintptr_t base;
    if (fdt_find_virtio(fdt, &base) < 0) {
        console_puts("[block] No virtio,mmio device in FDT\n");
        return -1;
    }
    device = (volatile uint32_t *)base;
    *mmio_base = base;
    if (device[VIRTIO_MAGIC / 4] != 0x74726976U ||
        device[VIRTIO_VERSION / 4] != 2U || device[VIRTIO_DEVICE_ID / 4] != 2U) {
        console_puts("[block] FDT device is not modern virtio-blk\n");
        device = 0;
        return -1;
    }

    device[VIRTIO_STATUS / 4] = 0;
    device[VIRTIO_STATUS / 4] = DEVICE_ACKNOWLEDGE;
    device[VIRTIO_STATUS / 4] |= DEVICE_DRIVER;
    device[VIRTIO_DEVICE_FEATURES_SEL / 4] = 1;
    const uint32_t high_features = device[VIRTIO_DEVICE_FEATURES / 4];
    if ((high_features & 1U) == 0) {
        console_puts("[block] Device lacks VERSION_1 feature\n");
        device = 0;
        return -1;
    }
    device[VIRTIO_DRIVER_FEATURES_SEL / 4] = 0;
    device[VIRTIO_DRIVER_FEATURES / 4] = 0;
    device[VIRTIO_DRIVER_FEATURES_SEL / 4] = 1;
    device[VIRTIO_DRIVER_FEATURES / 4] = 1U << FEATURE_VERSION_1;
    device[VIRTIO_STATUS / 4] |= DEVICE_FEATURES_OK;
    if ((device[VIRTIO_STATUS / 4] & DEVICE_FEATURES_OK) == 0) {
        console_puts("[block] Feature negotiation rejected\n");
        device = 0;
        return -1;
    }

    device[VIRTIO_QUEUE_SEL / 4] = 0;
    const uint32_t maximum = device[VIRTIO_QUEUE_NUM_MAX / 4];
    if (maximum < 3) {
        console_puts("[block] Queue too small\n");
        device = 0;
        return -1;
    }
    queue_size = maximum < QUEUE_SIZE ? (uint16_t)maximum : QUEUE_SIZE;
    device[VIRTIO_QUEUE_NUM / 4] = queue_size;
    write_address(VIRTIO_QUEUE_DESC_LOW, (uintptr_t)descriptors);
    write_address(VIRTIO_QUEUE_AVAIL_LOW, (uintptr_t)&available);
    write_address(VIRTIO_QUEUE_USED_LOW, (uintptr_t)&used);
    device[VIRTIO_QUEUE_READY / 4] = 1;
    device[VIRTIO_STATUS / 4] |= DEVICE_DRIVER_OK;

    /* Read capacity from the little-endian configuration space. */
    volatile uint32_t *config = (volatile uint32_t *)((uintptr_t)device + VIRTIO_CONFIG);
    uint32_t generation_before, generation_after, low, high;
    do {
        generation_before = device[0xfc / 4];
        low = config[0];
        high = config[1];
        generation_after = device[0xfc / 4];
    } while (generation_before != generation_after);
    sectors = ((uint64_t)high << 32) | low;
    console_puts("[block] VirtIO block ready, sectors=");
    console_number((long)sectors);
    console_putc('\n');
    return sectors == 0 ? -1 : 0;
}

static int transfer(uint32_t sector, void *buffer, int write) {
    if (device == 0 || buffer == 0 || sector >= sectors) {
        return -1;
    }
    request.type = write ? 1U : 0U;
    request.reserved = 0;
    request.sector = sector;
    request_status = 0xffU;
    transfer_buffer = buffer;
    descriptors[0] = (VirtqDescriptor){ (uintptr_t)&request, sizeof(request), 1U, 1U };
    descriptors[1] = (VirtqDescriptor){ (uintptr_t)transfer_buffer, 512U,
        (uint16_t)(1U | (write ? 0U : 2U)), 2U };
    descriptors[2] = (VirtqDescriptor){ (uintptr_t)&request_status, 1U, 2U, 0U };
    const uint16_t index = available.index;
    available.ring[index % queue_size] = 0;
    __asm__ volatile("fence rw, w" : : : "memory");
    available.index = (uint16_t)(index + 1U);
    __asm__ volatile("fence w, o" : : : "memory");
    device[VIRTIO_QUEUE_NOTIFY / 4] = 0;
    while (used.index == consumed_used) {
        __asm__ volatile("nop");
    }
    __asm__ volatile("fence r, rw" : : : "memory");
    const int result = request_status == 0 ? 0 : -1;
    ++consumed_used;
    return result;
}

int block_read(uint32_t sector, void *buffer) { return transfer(sector, buffer, 0); }
int block_write(uint32_t sector, const void *buffer) {
    return transfer(sector, (void *)buffer, 1);
}
uint32_t block_sector_count(void) { return sectors > UINT32_MAX ? UINT32_MAX : (uint32_t)sectors; }
