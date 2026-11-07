#include "drivers/virtio/gpu.h"
#include "arch/riscv/clock.h"
#include "kernel/io/console.h"
#include "kernel/mm/page.h"
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
#define DEVICE_ACKNOWLEDGE 1U
#define DEVICE_DRIVER 2U
#define DEVICE_DRIVER_OK 4U
#define DEVICE_FEATURES_OK 8U
#define FEATURE_VERSION_1 0U
#define QUEUE_SIZE 8U
#define REQUEST_TIMEOUT_MS 5000UL

#define GPU_CMD_RESOURCE_CREATE_2D 0x0101U
#define GPU_CMD_SET_SCANOUT 0x0103U
#define GPU_CMD_RESOURCE_FLUSH 0x0104U
#define GPU_CMD_TRANSFER_TO_HOST_2D 0x0105U
#define GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106U
#define GPU_RESP_OK_NODATA 0x1100U
#define GPU_FORMAT_B8G8R8X8_UNORM 2U
#define GPU_RESOURCE_ID 1U

typedef struct {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} VirtqDescriptor;
typedef struct { uint16_t flags; volatile uint16_t index; uint16_t ring[QUEUE_SIZE]; uint16_t event; } VirtqAvailable;
typedef struct { uint32_t id, length; } VirtqUsedElement;
typedef struct { uint16_t flags; volatile uint16_t index; VirtqUsedElement ring[QUEUE_SIZE]; uint16_t event; } VirtqUsed;
typedef struct {
    uint32_t type, flags;
    uint64_t fence_id;
    uint32_t context_id;
    uint8_t ring_index, padding[3];
} GpuHeader;
typedef struct { uint32_t x, y, width, height; } GpuRectangle;
typedef struct { GpuHeader header; GpuRectangle rectangle; } GpuRectCommand;
typedef struct { GpuHeader header; uint32_t resource_id, format, width, height; } GpuCreateCommand;
typedef struct { uint64_t address; uint32_t length, padding; } GpuMemoryEntry;
typedef struct { GpuHeader header; uint32_t resource_id, entries; GpuMemoryEntry memory; } GpuAttachCommand;
typedef struct { GpuHeader header; GpuRectangle rectangle; uint32_t scanout_id, resource_id; } GpuSetScanoutCommand;
typedef struct { GpuHeader header; GpuRectangle rectangle; uint32_t resource_id, padding; } GpuFlushCommand;
typedef struct { GpuHeader header; GpuRectangle rectangle; uint64_t offset; uint32_t resource_id, padding; } GpuTransferCommand;
typedef union {
    GpuHeader header;
    GpuRectCommand rectangle;
    GpuCreateCommand create;
    GpuAttachCommand attach;
    GpuSetScanoutCommand scanout;
    GpuFlushCommand flush;
    GpuTransferCommand transfer;
    uint8_t bytes[128];
} GpuCommand;

typedef struct {
    volatile uint32_t *registers;
    uintptr_t base, framebuffer_physical;
    uint32_t *framebuffer;
    size_t framebuffer_pages;
    uint16_t consumed, queue_length;
    VirtqDescriptor descriptors[QUEUE_SIZE] __attribute__((aligned(16)));
    VirtqAvailable available __attribute__((aligned(2)));
    VirtqUsed used __attribute__((aligned(4)));
    GpuCommand command __attribute__((aligned(16)));
    GpuHeader response __attribute__((aligned(16)));
} GpuDevice;
static GpuDevice gpu;

typedef struct {
    uint32_t address_cells, size_cells;
    uint64_t address;
    int compatible, has_reg;
} FdtNode;

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

int gpu_probe(const void *blob) {
    if (blob == 0) return -1;
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
                if (candidate[0] == 0x74726976U && candidate[1] == 2U && candidate[2] == 16U) {
                    gpu.base = (uintptr_t)node->address;
                    gpu.registers = candidate;
                    console_puts("[gpu] FDT VirtIO GPU at "); console_hex(gpu.base); console_putc('\n');
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
    console_puts("[gpu] No modern VirtIO GPU in FDT\n");
    return -1;
}

uintptr_t gpu_mmio_base(void) { return gpu.base; }
static void write_address(uint32_t offset, uintptr_t address) {
    gpu.registers[offset / 4] = (uint32_t)address;
    gpu.registers[(offset + 4) / 4] = (uint32_t)(address >> 32);
}
static int command(GpuCommand *request, size_t request_size) {
    gpu.response = (GpuHeader){0};
    gpu.descriptors[0] = (VirtqDescriptor){ (uintptr_t)request, (uint32_t)request_size, 1U, 1U };
    gpu.descriptors[1] = (VirtqDescriptor){ (uintptr_t)&gpu.response, sizeof(gpu.response), 2U, 0U };
    const uint16_t index = gpu.available.index;
    gpu.available.ring[index % gpu.queue_length] = 0;
    const uint64_t start = clock_ticks();
    __asm__ volatile("fence rw, w" : : : "memory");
    gpu.available.index = (uint16_t)(index + 1U);
    __asm__ volatile("fence w, o" : : : "memory");
    gpu.registers[VIRTIO_QUEUE_NOTIFY / 4] = 0;
    while (gpu.used.index == gpu.consumed) {
        if (clock_ticks() - start >= clock_ms_ticks(REQUEST_TIMEOUT_MS)) {
            console_puts("[gpu] Command timed out after 5000 ms; halting to protect DMA buffers\n");
            kernel_idle();
        }
    }
    __asm__ volatile("fence r, rw" : : : "memory");
    ++gpu.consumed;
    if (gpu.response.type != GPU_RESP_OK_NODATA) {
        console_puts("[gpu] Command failed, type="); console_hex(request->header.type);
        console_puts(" response="); console_hex(gpu.response.type); console_putc('\n');
        return -1;
    }
    return 0;
}

static GpuHeader header(uint32_t type) { return (GpuHeader){ .type = type }; }
static int create_resource(void) {
    gpu.command.create = (GpuCreateCommand){ .header = header(GPU_CMD_RESOURCE_CREATE_2D),
        .resource_id = GPU_RESOURCE_ID, .width = GPU_WIDTH, .height = GPU_HEIGHT,
        .format = GPU_FORMAT_B8G8R8X8_UNORM };
    if (command(&gpu.command, sizeof(gpu.command.create)) < 0) return -1;
    gpu.command.attach = (GpuAttachCommand){ .header = header(GPU_CMD_RESOURCE_ATTACH_BACKING),
        .resource_id = GPU_RESOURCE_ID, .entries = 1,
        .memory = { gpu.framebuffer_physical, GPU_WIDTH * GPU_HEIGHT * 4U, 0 } };
    if (command(&gpu.command, sizeof(gpu.command.attach)) < 0) return -1;
    gpu.command.scanout = (GpuSetScanoutCommand){ .header = header(GPU_CMD_SET_SCANOUT),
        .resource_id = GPU_RESOURCE_ID, .scanout_id = 0,
        .rectangle = { .x = 0, .y = 0, .width = GPU_WIDTH, .height = GPU_HEIGHT } };
    if (command(&gpu.command, sizeof(gpu.command.scanout)) < 0) return -1;
    return 0;
}

int gpu_present(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!gpu.framebuffer || width == 0 || height == 0 || x >= GPU_WIDTH || y >= GPU_HEIGHT ||
        width > GPU_WIDTH - x || height > GPU_HEIGHT - y) return -1;
    gpu.command.transfer = (GpuTransferCommand){ .header = header(GPU_CMD_TRANSFER_TO_HOST_2D),
        .offset = ((uint64_t)y * GPU_WIDTH + x) * 4U, .resource_id = GPU_RESOURCE_ID,
        .rectangle = { .x = x, .y = y, .width = width, .height = height } };
    if (command(&gpu.command, sizeof(gpu.command.transfer)) < 0) return -1;
    gpu.command.flush = (GpuFlushCommand){ .header = header(GPU_CMD_RESOURCE_FLUSH),
        .resource_id = GPU_RESOURCE_ID,
        .rectangle = { .x = x, .y = y, .width = width, .height = height } };
    return command(&gpu.command, sizeof(gpu.command.flush));
}

uint32_t *gpu_framebuffer(void) { return gpu.framebuffer; }

int gpu_init(void) {
    if (!gpu.registers) return -1;
    if (gpu.registers[VIRTIO_MAGIC / 4] != 0x74726976U || gpu.registers[VIRTIO_VERSION / 4] != 2U ||
        gpu.registers[VIRTIO_DEVICE_ID / 4] != 16U) return -1;
    gpu.registers[VIRTIO_STATUS / 4] = 0;
    gpu.registers[VIRTIO_STATUS / 4] = DEVICE_ACKNOWLEDGE;
    gpu.registers[VIRTIO_STATUS / 4] |= DEVICE_DRIVER;
    gpu.registers[VIRTIO_DEVICE_FEATURES_SEL / 4] = 1;
    const uint32_t high_features = gpu.registers[VIRTIO_DEVICE_FEATURES / 4];
    gpu.registers[VIRTIO_DRIVER_FEATURES_SEL / 4] = 0;
    gpu.registers[VIRTIO_DRIVER_FEATURES / 4] = 0;
    gpu.registers[VIRTIO_DRIVER_FEATURES_SEL / 4] = 1;
    if ((high_features & 1U) == 0) {
        console_puts("[gpu] Device lacks VERSION_1 feature\n"); return -1;
    }
    gpu.registers[VIRTIO_DRIVER_FEATURES / 4] = 1U << FEATURE_VERSION_1;
    gpu.registers[VIRTIO_STATUS / 4] |= DEVICE_FEATURES_OK;
    if ((gpu.registers[VIRTIO_STATUS / 4] & DEVICE_FEATURES_OK) == 0) {
        console_puts("[gpu] Feature negotiation rejected\n"); return -1;
    }
    gpu.registers[VIRTIO_QUEUE_SEL / 4] = 0;
    const uint32_t maximum = gpu.registers[VIRTIO_QUEUE_NUM_MAX / 4];
    if (maximum < 2) { console_puts("[gpu] Control queue must hold two descriptors\n"); return -1; }
    gpu.queue_length = maximum < QUEUE_SIZE ? (uint16_t)maximum : QUEUE_SIZE;
    gpu.registers[VIRTIO_QUEUE_NUM / 4] = gpu.queue_length;
    write_address(VIRTIO_QUEUE_DESC_LOW, (uintptr_t)gpu.descriptors);
    write_address(VIRTIO_QUEUE_AVAIL_LOW, (uintptr_t)&gpu.available);
    write_address(VIRTIO_QUEUE_USED_LOW, (uintptr_t)&gpu.used);
    gpu.registers[VIRTIO_QUEUE_READY / 4] = 1;
    gpu.registers[VIRTIO_STATUS / 4] |= DEVICE_DRIVER_OK;
    gpu.framebuffer_pages = (GPU_WIDTH * GPU_HEIGHT * 4U + PAGE_SIZE - 1) / PAGE_SIZE;
    gpu.framebuffer_physical = page_alloc_contiguous(gpu.framebuffer_pages);
    if (!gpu.framebuffer_physical) { console_puts("[gpu] Cannot allocate contiguous framebuffer RAM\n"); return -1; }
    gpu.framebuffer = (uint32_t *)gpu.framebuffer_physical;
    const uint32_t pixel = 0x00d0b070U;
    for (size_t i = 0; i < GPU_WIDTH * GPU_HEIGHT; ++i) gpu.framebuffer[i] = pixel;
    if (create_resource() < 0 || gpu_present(0, 0, GPU_WIDTH, GPU_HEIGHT) < 0) return -1;
    console_puts("[gpu] 1280x720 VirtIO 2D scanout ready\n");
    return 0;
}
