#include "drivers/virtio/block.h"
#include "kernel/io/console.h"
#include "kernel/proc/gate.h"
#include "kernel/proc/process.h"
#include "kernel/io/diagnostic.h"
#include "syscall_numbers.h"
#include "arch/riscv/clock.h"
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
#define FEATURE_FLUSH 9U
#define QUEUE_SIZE 8U
#define BLOCK_RESET_TIMEOUT_MS 100UL

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

typedef enum {
    BLOCK_REQUEST_IDLE,
    BLOCK_REQUEST_IN_FLIGHT,
    BLOCK_REQUEST_RESETTING,
    BLOCK_REQUEST_DONE
} BlockRequestState;

typedef enum {
    BLOCK_OP_READ,
    BLOCK_OP_WRITE,
    BLOCK_OP_FLUSH
} BlockOperation;

typedef struct {
    volatile uint32_t *registers;
    volatile uint16_t consumed;
    uint16_t length;
    uint64_t capacity;
    int flush_supported;
    VirtqDescriptor descriptor[QUEUE_SIZE] __attribute__((aligned(16)));
    VirtqAvailable avail_ring __attribute__((aligned(2)));
    VirtqUsed used_ring __attribute__((aligned(4)));
    BlockRequest header __attribute__((aligned(16)));
    volatile uint8_t status;
    uint8_t bounce[512] __attribute__((aligned(16)));
    KernelGate gate;
    KernelEventSource completion;
    volatile BlockRequestState request_state;
    BlockOperation operation;
    uint32_t request_sector;
    uint64_t deadline;
    uint64_t reset_deadline;
    long request_result;
    long reset_result;
    unsigned offline : 1;
    unsigned diagnostic_pending : 1;
    unsigned diagnostic_uncertain : 1;
} BlockDevice;

static BlockDevice devices[2];
static BlockDevice *filesystem, *swap_disk;
static uintptr_t bases[2];
static unsigned int device_count;

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

static int fdt_find_virtio(const void *blob) {
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
                    if (device_count < 2) bases[device_count++] = (uintptr_t)node->reg_address;
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
    return device_count == 0 ? -1 : 0;
}

static void write_address(BlockDevice *target, uint32_t low_register, uintptr_t address) {
    target->registers[low_register / 4] = (uint32_t)address;
    target->registers[(low_register + 4) / 4] = (uint32_t)(address >> 32);
}

static int init_device(BlockDevice *target, uintptr_t base) {
    volatile uint32_t *device = (volatile uint32_t *)base;
    target->registers = device;
    if (kernel_gate_init(&target->gate) < 0 ||
        event_source_init(&target->completion, 1) < 0) {
        target->registers = 0;
        return -1;
    }
    if (device[VIRTIO_MAGIC / 4] != 0x74726976U ||
        device[VIRTIO_VERSION / 4] != 2U || device[VIRTIO_DEVICE_ID / 4] != 2U) {
        console_puts("[block] FDT device is not modern virtio-blk\n");
        target->registers = 0;
        return -1;
    }

    device[VIRTIO_STATUS / 4] = 0;
    device[VIRTIO_STATUS / 4] = DEVICE_ACKNOWLEDGE;
    device[VIRTIO_STATUS / 4] |= DEVICE_DRIVER;
    device[VIRTIO_DEVICE_FEATURES_SEL / 4] = 1;
    const uint32_t high_features = device[VIRTIO_DEVICE_FEATURES / 4];
    if ((high_features & 1U) == 0) {
        console_puts("[block] Device lacks VERSION_1 feature\n");
        target->registers = 0;
        return -1;
    }
    device[VIRTIO_DRIVER_FEATURES_SEL / 4] = 0;
    device[VIRTIO_DEVICE_FEATURES_SEL / 4] = 0;
    const uint32_t low_features = device[VIRTIO_DEVICE_FEATURES / 4];
    target->flush_supported = (low_features & (1U << FEATURE_FLUSH)) != 0;
    device[VIRTIO_DRIVER_FEATURES / 4] = target->flush_supported ? (1U << FEATURE_FLUSH) : 0;
    device[VIRTIO_DRIVER_FEATURES_SEL / 4] = 1;
    device[VIRTIO_DRIVER_FEATURES / 4] = 1U << FEATURE_VERSION_1;
    device[VIRTIO_STATUS / 4] |= DEVICE_FEATURES_OK;
    if ((device[VIRTIO_STATUS / 4] & DEVICE_FEATURES_OK) == 0) {
        console_puts("[block] Feature negotiation rejected\n");
        target->registers = 0;
        return -1;
    }

    device[VIRTIO_QUEUE_SEL / 4] = 0;
    const uint32_t maximum = device[VIRTIO_QUEUE_NUM_MAX / 4];
    if (maximum < 3) {
        console_puts("[block] Queue too small\n");
        target->registers = 0;
        return -1;
    }
    target->length = maximum < QUEUE_SIZE ? (uint16_t)maximum : QUEUE_SIZE;
    target->consumed = 0;
    target->avail_ring.index = 0;
    target->used_ring.index = 0;
    device[VIRTIO_QUEUE_NUM / 4] = target->length;
    write_address(target, VIRTIO_QUEUE_DESC_LOW, (uintptr_t)target->descriptor);
    write_address(target, VIRTIO_QUEUE_AVAIL_LOW, (uintptr_t)&target->avail_ring);
    write_address(target, VIRTIO_QUEUE_USED_LOW, (uintptr_t)&target->used_ring);
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
    target->capacity = ((uint64_t)high << 32) | low;
    console_puts("[block] VirtIO block ready, sectors=");
    console_number((long)target->capacity);
    console_putc('\n');
    return target->capacity == 0 ? -1 : 0;
}

static _Noreturn void reset_fail_stop(BlockDevice *target) {
    kernel_diagnostic("block", "reset-unconfirmed", ERR_IO, 1,
                      (uintptr_t)target->request_sector);
    /* Reset was not acknowledged, so the device may still own every queue buffer. */
    __asm__ volatile("csrci sstatus, 2\ncsrw sie, zero" : : : "memory");
    kernel_idle();
}

static void begin_reset(BlockDevice *target, long result) {
    if (target->request_state != BLOCK_REQUEST_IN_FLIGHT) return;
    target->reset_result = result;
    target->reset_deadline = clock_ticks() + clock_ms_ticks(BLOCK_RESET_TIMEOUT_MS);
    target->request_state = BLOCK_REQUEST_RESETTING;
    /* Do not clear queue memory or return while reset ownership is uncertain. */
    __asm__ volatile("fence rw, rw" : : : "memory");
    target->registers[VIRTIO_STATUS / 4] = 0;
    __asm__ volatile("fence o, rw" : : : "memory");
}

static void poll_reset(BlockDevice *target) {
    if (target->registers[VIRTIO_STATUS / 4] == 0U) {
        __asm__ volatile("fence iorw, iorw" : : : "memory");
        target->offline = 1;
        target->request_result = target->reset_result;
        target->diagnostic_pending = 1;
        target->diagnostic_uncertain = target->operation != BLOCK_OP_READ;
        target->request_state = BLOCK_REQUEST_DONE;
        event_source_notify(&target->completion, target->request_result);
        return;
    }
    if ((int64_t)(clock_ticks() - target->reset_deadline) >= 0)
        reset_fail_stop(target);
}

static void poll_device(BlockDevice *target) {
    if (target == 0) return;
    if (target->request_state == BLOCK_REQUEST_RESETTING) {
        poll_reset(target);
        return;
    }
    if (target->request_state != BLOCK_REQUEST_IN_FLIGHT) return;
    const uint16_t used_index = target->used_ring.index;
    if (used_index != target->consumed) {
        __asm__ volatile("fence r, rw" : : : "memory");
        const uint16_t delta = (uint16_t)(used_index - target->consumed);
        if (delta != 1U) {
            begin_reset(target, ERR_IO);
            return;
        }
        const VirtqUsedElement *element = &target->used_ring.ring[target->consumed % target->length];
        if (element->id != 0U) {
            begin_reset(target, ERR_IO);
            return;
        }
        const uint8_t status = target->status;
        if (status > 2U || element->length == 0U || element->length > 513U ||
            (target->operation != BLOCK_OP_READ && element->length != 1U) ||
            (target->operation == BLOCK_OP_READ && status == 0U && element->length != 513U)) {
            begin_reset(target, ERR_IO);
            return;
        }
        ++target->consumed;
        target->request_result = status == 0U ? 0 : ERR_IO;
        if (target->request_result < 0) {
            target->diagnostic_pending = 1;
            target->diagnostic_uncertain = target->operation != BLOCK_OP_READ;
        }
        target->request_state = BLOCK_REQUEST_DONE;
        event_source_notify(&target->completion, target->request_result);
        return;
    }
    if ((int64_t)(clock_ticks() - target->deadline) >= 0 &&
        target->used_ring.index == target->consumed) {
        begin_reset(target, ERR_TIMED_OUT);
    }
}

void block_poll(void) {
    for (unsigned int i = 0; i < device_count; ++i) poll_device(&devices[i]);
}

int block_poll_pending(void) {
    for (unsigned int i = 0; i < device_count; ++i)
        if (devices[i].request_state == BLOCK_REQUEST_IN_FLIGHT ||
            devices[i].request_state == BLOCK_REQUEST_RESETTING) return 1;
    return 0;
}

static int submit_request(BlockDevice *target, uint32_t sector, const void *buffer,
                          BlockOperation operation) {
    if (target == 0 || target->registers == 0 || target->offline ||
        (operation != BLOCK_OP_FLUSH && (buffer == 0 || sector >= target->capacity)) ||
        (operation == BLOCK_OP_FLUSH && !target->flush_supported)) return ERR_IO;
    if (process_runtime_started() && !process_wait_allowed()) return ERR_BUSY;

    const int gate_result = kernel_gate_enter(&target->gate);
    if (gate_result < 0) return gate_result;
    const int defer_result = process_cancel_defer_enter();
    if (defer_result < 0) {
        (void)kernel_gate_leave(&target->gate);
        return defer_result;
    }

    /* Request storage and the data DMA target remain owned until collect. */
    if (target->request_state != BLOCK_REQUEST_IDLE || target->offline) {
        (void)kernel_gate_leave(&target->gate);
        (void)process_cancel_defer_leave();
        return target->offline ? ERR_IO : ERR_BUSY;
    }
    if (operation == BLOCK_OP_WRITE) {
        const uint8_t *source = buffer;
        for (unsigned int i = 0; i < sizeof(target->bounce); ++i) target->bounce[i] = source[i];
    }
    target->operation = operation;
    target->request_sector = sector;
    target->request_result = ERR_IO;
    target->header.type = operation == BLOCK_OP_READ ? 0U :
                          operation == BLOCK_OP_WRITE ? 1U : 4U;
    target->header.reserved = 0;
    target->header.sector = sector;
    target->status = 0xffU;
    target->descriptor[0] = (VirtqDescriptor){ (uintptr_t)&target->header, sizeof(target->header), 1U, 1U };
    const int has_data = operation != BLOCK_OP_FLUSH;
    if (has_data) {
        target->descriptor[1] = (VirtqDescriptor){ (uintptr_t)target->bounce, 512U,
            (uint16_t)(1U | (operation == BLOCK_OP_READ ? 2U : 0U)), 2U };
        target->descriptor[2] = (VirtqDescriptor){ (uintptr_t)&target->status, 1U, 2U, 0U };
    } else {
        target->descriptor[1] = (VirtqDescriptor){ (uintptr_t)&target->status, 1U, 2U, 0U };
    }
    uint32_t generation;
    uint64_t sequence;
    if (event_source_snapshot(&target->completion, &generation, &sequence) < 0) {
        (void)kernel_gate_leave(&target->gate);
        (void)process_cancel_defer_leave();
        return ERR_IO;
    }
    const uint16_t index = target->avail_ring.index;
    target->avail_ring.ring[index % target->length] = 0;
    target->deadline = clock_ticks() + clock_ms_ticks(BLOCK_IO_TIMEOUT_MS);
    target->request_state = BLOCK_REQUEST_IN_FLIGHT;
    __asm__ volatile("fence rw, w" : : : "memory");
    target->avail_ring.index = (uint16_t)(index + 1U);
    __asm__ volatile("fence w, o" : : : "memory");
    target->registers[VIRTIO_QUEUE_NOTIFY / 4] = 0;
    while (target->request_state != BLOCK_REQUEST_DONE) {
        if (!process_runtime_started()) {
            poll_device(target);
            continue;
        }
        (void)process_wait_event(&target->completion, generation, sequence, 0, ERR_TIMED_OUT);
        (void)event_source_snapshot(&target->completion, &generation, &sequence);
    }

    const int result = (int)target->request_result;
    if (target->diagnostic_pending) {
        const char *operation_name = target->operation == BLOCK_OP_READ ? "read" :
                                     target->operation == BLOCK_OP_WRITE ? "write" : "flush";
        kernel_diagnostic("block", operation_name, result,
                          target->diagnostic_uncertain,
                          (uintptr_t)target->request_sector);
        target->diagnostic_pending = 0;
        target->diagnostic_uncertain = 0;
    }
    if (result == 0 && operation == BLOCK_OP_READ) {
        uint8_t *destination = (uint8_t *)buffer;
        for (unsigned int i = 0; i < sizeof(target->bounce); ++i) destination[i] = target->bounce[i];
    }
    target->request_state = BLOCK_REQUEST_IDLE;
    (void)kernel_gate_leave(&target->gate);
    (void)process_cancel_defer_leave();
    return result;
}

static int transfer(BlockDevice *target, uint32_t sector, void *buffer, int write) {
    return submit_request(target, sector, buffer, write ? BLOCK_OP_WRITE : BLOCK_OP_READ);
}

static int has_magic(const uint8_t *header, const char *magic) {
    for (unsigned int i = 0; i < 8; ++i) if (header[i] != (uint8_t)magic[i]) return 0;
    return 1;
}

int block_init_from_fdt(const void *fdt) {
    if (fdt_find_virtio(fdt) < 0) {
        console_puts("[block] No modern VirtIO block device in FDT\n");
        return -1;
    }
    BlockDevice *unmarked = 0;
    unsigned int unknown_count = 0;
    for (unsigned int i = 0; i < device_count; ++i) {
        BlockDevice *target = &devices[i];
        if (init_device(target, bases[i]) < 0) continue;
        uint8_t header[512];
        if (transfer(target, 0, header, 0) < 0) continue;
        if (has_magic(header, "WUSWAP01")) {
            if (swap_disk != 0) {
                console_puts("[block] Duplicate swap disks; refusing boot\n");
                kernel_idle();
            }
            swap_disk = target;
        } else if (has_magic(header, "WUFS0001") || has_magic(header, "WUFS0002")) {
            if (filesystem != 0) {
                console_puts("[block] Duplicate WUFS disks; refusing boot\n");
                kernel_idle();
            }
            filesystem = target;
        } else {
            unmarked = target;
            ++unknown_count;
        }
    }
    if (filesystem == 0 && unknown_count == 1) filesystem = unmarked;
    console_puts("[block] WUFS disk="); console_number(filesystem != 0);
    console_puts(" swap disk="); console_number(swap_disk != 0); console_putc('\n');
    return filesystem != 0 ? 0 : -1;
}

unsigned int block_mmio_count(void) { return device_count; }
uintptr_t block_mmio_base(unsigned int index) { return index < device_count ? bases[index] : 0; }
int block_read(uint32_t sector, void *buffer) {
    return transfer(filesystem, sector, buffer, 0);
}
int block_write(uint32_t sector, const void *buffer) {
    return transfer(filesystem, sector, (void *)buffer, 1);
}
static int flush_device(BlockDevice *target) {
    return submit_request(target, 0, 0, BLOCK_OP_FLUSH);
}
int block_flush(void) { return flush_device(filesystem); }
int block_flush_supported(void) {
    return filesystem != 0 && filesystem->flush_supported;
}
uint32_t block_sector_count(void) {
    return filesystem == 0 ? 0 : filesystem->capacity > UINT32_MAX ? UINT32_MAX : (uint32_t)filesystem->capacity;
}
int block_swap_read(uint32_t sector, void *buffer) {
    return transfer(swap_disk, sector, buffer, 0);
}
int block_swap_write(uint32_t sector, const void *buffer) {
    return transfer(swap_disk, sector, (void *)buffer, 1);
}
uint32_t block_swap_sector_count(void) {
    return swap_disk == 0 ? 0 : swap_disk->capacity > UINT32_MAX ? UINT32_MAX : (uint32_t)swap_disk->capacity;
}
