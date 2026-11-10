#include "block.h"
#include "console.h"
#include "clock.h"
#include "irq.h"
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
#define VIRTIO_INTERRUPT_STATUS 0x060U
#define VIRTIO_INTERRUPT_ACK 0x064U
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

typedef enum {
    REQUEST_IDLE, REQUEST_INFLIGHT, REQUEST_COMPLETE, REQUEST_FAILED
} RequestState;

typedef struct {
    volatile uint32_t *registers;
    volatile uint16_t consumed;
    uint16_t length;
    uint64_t capacity;
    VirtqDescriptor descriptor[QUEUE_SIZE] __attribute__((aligned(16)));
    VirtqAvailable avail_ring __attribute__((aligned(2)));
    VirtqUsed used_ring __attribute__((aligned(4)));
    BlockRequest header __attribute__((aligned(16)));
    volatile uint8_t status; /* Written by device DMA, read only after used publication. */
    uint32_t irq_events;
    RequestState state;
    uint64_t submitted, completed, io_errors, protocol_errors;
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

static void write_address(BlockDevice *source, uint32_t low_register, uintptr_t address) {
    source->registers[low_register / 4] = (uint32_t)address;
    source->registers[(low_register + 4) / 4] = (uint32_t)(address >> 32);
}

static int init_device(BlockDevice *source, uintptr_t base) {
    volatile uint32_t *device = (volatile uint32_t *)base;
    source->registers = device;
    if (device[VIRTIO_MAGIC / 4] != 0x74726976U ||
        device[VIRTIO_VERSION / 4] != 2U || device[VIRTIO_DEVICE_ID / 4] != 2U) {
        console_puts("[block] FDT device is not modern virtio-blk\n");
        source->registers = 0;
        return -1;
    }

    device[VIRTIO_STATUS / 4] = 0;
    device[VIRTIO_STATUS / 4] = DEVICE_ACKNOWLEDGE;
    device[VIRTIO_STATUS / 4] |= DEVICE_DRIVER;
    device[VIRTIO_DEVICE_FEATURES_SEL / 4] = 1;
    const uint32_t high_features = device[VIRTIO_DEVICE_FEATURES / 4];
    if ((high_features & 1U) == 0) {
        console_puts("[block] Device lacks VERSION_1 feature\n");
        source->registers = 0;
        return -1;
    }
    device[VIRTIO_DRIVER_FEATURES_SEL / 4] = 0;
    device[VIRTIO_DRIVER_FEATURES / 4] = 0;
    device[VIRTIO_DRIVER_FEATURES_SEL / 4] = 1;
    device[VIRTIO_DRIVER_FEATURES / 4] = 1U << FEATURE_VERSION_1;
    device[VIRTIO_STATUS / 4] |= DEVICE_FEATURES_OK;
    if ((device[VIRTIO_STATUS / 4] & DEVICE_FEATURES_OK) == 0) {
        console_puts("[block] Feature negotiation rejected\n");
        source->registers = 0;
        return -1;
    }

    device[VIRTIO_QUEUE_SEL / 4] = 0;
    const uint32_t maximum = device[VIRTIO_QUEUE_NUM_MAX / 4];
    if (maximum < 3) {
        console_puts("[block] Queue too small\n");
        source->registers = 0;
        return -1;
    }
    source->length = maximum < QUEUE_SIZE ? (uint16_t)maximum : QUEUE_SIZE;
    device[VIRTIO_QUEUE_NUM / 4] = source->length;
    write_address(source, VIRTIO_QUEUE_DESC_LOW, (uintptr_t)source->descriptor);
    write_address(source, VIRTIO_QUEUE_AVAIL_LOW, (uintptr_t)&source->avail_ring);
    write_address(source, VIRTIO_QUEUE_USED_LOW, (uintptr_t)&source->used_ring);
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
    source->capacity = ((uint64_t)high << 32) | low;
    console_puts("[block] VirtIO block ready, sectors=");
    console_number((long)source->capacity);
    console_putc('\n');
    return source->capacity == 0 ? -1 : 0;
}

static _Noreturn void transfer_failure(BlockDevice *source, const char *reason,
                                      uint32_t sector, int write, uint64_t elapsed) {
    source->state = REQUEST_FAILED;
    console_puts("[block] "); console_puts(reason); console_puts(" device=");
    console_hex((uintptr_t)source->registers);
    console_puts(" role=");
    console_puts(source == filesystem ? "WUFS" : source == swap_disk ? "swap" : "probe");
    console_puts(write ? " op=write" : " op=read");
    console_puts(" sector="); console_number((long)sector);
    console_puts(" elapsed_ms="); console_number((long)(elapsed / (CLOCK_TIMEBASE_HZ / 1000UL)));
    console_puts(" limit_ms="); console_number((long)BLOCK_IO_TIMEOUT_MS);
    console_puts(" consumed="); console_number(source->consumed);
    console_puts(" used="); console_number(source->used_ring.index);
    console_puts("\n[panic] Outstanding disk request retained; kernel halted. Restart QEMU.\n");
    /* The device may still DMA into the request/buffer. Never return or reuse either. */
    __asm__ volatile("csrci sstatus, 2\ncsrw sie, zero" : : : "memory");
    kernel_idle();
}

/*
 * Exactly one chain (head descriptor 0) may be outstanding per device. Both
 * polling and IRQ notification may reach this collector, but only with SIE=0
 * on our single hart. State is changed before another path can consume again.
 * A notification alone is never proof of completion: used.index publishes DMA.
 */
static void collect_completion(BlockDevice *source) {
    if (source->state != REQUEST_INFLIGHT) return;
    const uint16_t published = source->used_ring.index;
    const uint16_t count = (uint16_t)(published - source->consumed);
    if (!count) return;
    __asm__ volatile("fence r, rw" : : : "memory");
    const VirtqUsedElement entry = source->used_ring.ring[source->consumed % source->length];
    const uint32_t writable = source->header.type == 0 ? 513U : 1U;
    /*
     * Reject index jumps, a different descriptor head, impossible DMA lengths,
     * or an unwritten/unknown status. Never reuse a chain with corrupt ownership.
     * Error completions may have short data; exact read length is required only
     * on success. uint16_t subtraction deliberately handles ring index wrap.
     */
    if (count != 1 || entry.id != 0 || entry.length < 1 || entry.length > writable ||
        source->status > 2 || (source->status == 0 && entry.length != writable)) {
        ++source->protocol_errors;
        source->state = REQUEST_FAILED;
        return;
    }
    source->consumed = published;
    ++source->completed;
    if (source->status != 0) ++source->io_errors;
    source->state = REQUEST_COMPLETE;
}

static int transfer(BlockDevice *source, uint32_t sector, void *buffer, int write) {
    if (source == 0 || source->registers == 0 || buffer == 0 || sector >= source->capacity) {
        return -1;
    }
    uintptr_t cpu_status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(cpu_status));
    if ((cpu_status & 2U) || source->state != REQUEST_IDLE ||
        source->used_ring.index != source->consumed) {
        transfer_failure(source, "REQUEST OWNERSHIP ERROR", sector, write, 0);
    }
    source->header.type = write ? 1U : 0U;
    source->header.reserved = 0;
    source->header.sector = sector;
    source->status = 0xffU;
    source->descriptor[0] = (VirtqDescriptor){ (uintptr_t)&source->header, sizeof(source->header), 1U, 1U };
    source->descriptor[1] = (VirtqDescriptor){ (uintptr_t)buffer, 512U,
        (uint16_t)(1U | (write ? 0U : 2U)), 2U };
    source->descriptor[2] = (VirtqDescriptor){ (uintptr_t)&source->status, 1U, 2U, 0U };
    const uint16_t index = source->avail_ring.index;
    source->avail_ring.ring[index % source->length] = 0;
    const uint64_t start = clock_ticks();
    source->state = REQUEST_INFLIGHT;
    ++source->submitted;
    __asm__ volatile("fence rw, w" : : : "memory");
    source->avail_ring.index = (uint16_t)(index + 1U);
    __asm__ volatile("fence w, o" : : : "memory");
    source->registers[VIRTIO_QUEUE_NOTIFY / 4] = 0;
    while (source->state == REQUEST_INFLIGHT) {
        collect_completion(source);
        const uint64_t elapsed = clock_ticks() - start;
        if (elapsed >= clock_ms_ticks(BLOCK_IO_TIMEOUT_MS) &&
            source->state == REQUEST_INFLIGHT) {
            /* Final acquire closes completion-at-deadline without reusing DMA. */
            collect_completion(source);
            if (source->state == REQUEST_INFLIGHT)
                transfer_failure(source, "I/O TIMEOUT", sector, write, elapsed);
        }
    }
    if (source->state == REQUEST_FAILED)
        transfer_failure(source, "INVALID COMPLETION", sector, write, clock_ticks() - start);
    const int result = source->status == 0 ? 0 : -1;
    /* Only the synchronous caller releases the chain and its borrowed buffer. */
    source->state = REQUEST_IDLE;
    return result;
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
        BlockDevice *source = &devices[i];
        if (init_device(source, bases[i]) < 0) continue;
        uint8_t header[512];
        if (transfer(source, 0, header, 0) < 0) continue;
        if (has_magic(header, "WUSWAP01")) {
            if (swap_disk != 0) {
                console_puts("[block] Duplicate swap disks; refusing boot\n");
                kernel_idle();
            }
            swap_disk = source;
        } else if (has_magic(header, "WUFS0001")) {
            if (filesystem != 0) {
                console_puts("[block] Duplicate WUFS disks; refusing boot\n");
                kernel_idle();
            }
            filesystem = source;
        } else {
            unmarked = source;
            ++unknown_count;
        }
    }
    if (filesystem == 0 && unknown_count == 1) filesystem = unmarked;
    console_puts("[block] WUFS disk="); console_number(filesystem != 0);
    console_puts(" swap disk="); console_number(swap_disk != 0); console_putc('\n');
    return filesystem != 0 ? 0 : -1;
}

unsigned int block_mmio_count(void) { return device_count; }

static int service_block_irq(void *context) {
    BlockDevice *source = context;
    const uint32_t events = source->registers[VIRTIO_INTERRUPT_STATUS / 4] & 3U;
    if (!events) return 0;
    /*
     * Preserve observed notification bits, then deassert the device IRQ before
     * the dispatcher completes the PLIC claim. The shared collector may retire
     * one used entry, but cannot release/reuse buffers, print, or reschedule:
     * transfer() retains the chain until it observes REQUEST_COMPLETE.
     * Config-change notifications are recorded; live disk resizing is unsupported.
     */
    source->irq_events |= events;
    __asm__ volatile("fence iorw, iorw" : : : "memory");
    source->registers[VIRTIO_INTERRUPT_ACK / 4] = events;
    __asm__ volatile("fence iorw, iorw" : : : "memory");
    collect_completion(source);
    return 1;
}

void block_register_irqs(void) {
    for (unsigned int i = 0; i < device_count; ++i) {
        BlockDevice *source = &devices[i];
        if (source != filesystem && source != swap_disk) continue;
        const char *name = source == filesystem ? "WUFS" : "swap";
        if (irq_register_mmio((uintptr_t)source->registers, service_block_irq, source, name) < 0) {
            console_puts("[irq] No supported route for "); console_puts(name);
            console_puts(" disk; retaining polling\n");
        }
    }
}

uintptr_t block_mmio_base(unsigned int index) { return index < device_count ? bases[index] : 0; }
int block_read(uint32_t sector, void *buffer) {
    return transfer(filesystem, sector, buffer, 0);
}
int block_write(uint32_t sector, const void *buffer) {
    return transfer(filesystem, sector, (void *)buffer, 1);
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

void block_print_stats(void) {
    static const char *states[] = { "idle", "inflight", "complete", "failed" };
    for (unsigned int i = 0; i < device_count; ++i) {
        const BlockDevice *source = &devices[i];
        if (source != filesystem && source != swap_disk) continue;
        console_puts("Block "); console_puts(source == filesystem ? "WUFS" : "swap");
        console_puts(" state="); console_puts(states[source->state]);
        console_puts(" submitted="); console_number((long)source->submitted);
        console_puts(" completed="); console_number((long)source->completed);
        console_puts(" io_errors="); console_number((long)source->io_errors);
        console_puts(" protocol_errors="); console_number((long)source->protocol_errors);
        console_putc('\n');
    }
}
