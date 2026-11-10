#include "arch/riscv/irq.h"
#include "arch/riscv/fdt.h"
#include "kernel/io/console.h"

#define PAGE_BYTES 4096UL
#define SEIE (1UL << 9)
#define PLIC_ENABLE 0x2000UL
#define PLIC_ENABLE_STRIDE 0x80UL
#define PLIC_CONTEXT 0x200000UL
#define PLIC_CONTEXT_STRIDE 0x1000UL
#define PLIC_CONTEXT_LIMIT 15872U
#define IRQ_ROUTE_LIMIT 16U
#define IRQ_BATCH_LIMIT 32U

typedef struct { uintptr_t base; uint32_t source; } IrqRoute;
typedef struct {
    IrqService service;
    void *device;
    const char *name;
    uint64_t handled, spurious;
} IrqSlot;

static uintptr_t plic_base, enable_base, context_base;
static uint32_t source_count, supervisor_context;
static IrqRoute routes[IRQ_ROUTE_LIMIT];
static unsigned int route_count;
static IrqSlot slots[IRQ_SOURCE_LIMIT];
static int available, enabled;
static uint64_t entries, claims, unhandled, invalid, budget_exhausted;

static uint32_t mmio_read(uintptr_t address) {
    const uint32_t value = *(volatile uint32_t *)address;
    __asm__ volatile("fence iorw, iorw" : : : "memory");
    return value;
}

static void mmio_write(uintptr_t address, uint32_t value) {
    *(volatile uint32_t *)address = value;
    /* Order device acknowledgement, PLIC completion, and subsequent masking. */
    __asm__ volatile("fence iorw, iorw" : : : "memory");
}

static int unavailable(const char *reason) {
    console_puts("[irq] "); console_puts(reason);
    console_puts("; retaining polling\n");
    return -1;
}

static int plic_context(const Fdt *tree, unsigned int node, unsigned long hart_id, uint32_t *context) {
    uint32_t length;
    const uint8_t *targets = fdt_property(tree, node, "interrupts-extended", &length);
    if (!targets || !length || length % 8U) return -1;
    int found = 0;
    for (uint32_t at = 0; at < length; at += 8U) {
        const int controller = fdt_phandle(tree, fdt_be32(targets + at));
        uint32_t cells;
        if (controller < 0 || !fdt_compatible(tree, (unsigned int)controller, "riscv,cpu-intc") ||
            fdt_u32(tree, (unsigned int)controller, "#interrupt-cells", &cells) < 0 || cells != 1U) return -1;
        const int cpu = tree->nodes[controller].parent;
        uint64_t address, size;
        if (cpu < 0 || fdt_reg(tree, (unsigned int)cpu, &address, &size) < 0) return -1;
        if (address == hart_id && fdt_be32(targets + at + 4U) == 9U) {
            if (found) return -1;
            *context = at / 8U;
            found = 1;
        }
    }
    return found ? 0 : -1;
}

static int interrupt_parent(const Fdt *tree, unsigned int node, uint32_t *phandle) {
    for (int current = (int)node; current >= 0; current = tree->nodes[current].parent) {
        uint32_t length;
        if (fdt_property(tree, (unsigned int)current, "interrupt-parent", &length)) {
            return fdt_u32(tree, (unsigned int)current, "interrupt-parent", phandle);
        }
    }
    return -1;
}

int irq_probe(const void *blob, unsigned long hart_id) {
    /* Optional, boot-only facility: unsupported trees leave external IRQs disabled. */
    __asm__ volatile("csrc sie, %0" : : "r"(SEIE) : "memory");
    available = enabled = 0;
    route_count = 0;
    Fdt tree;
    if (fdt_open(&tree, blob) < 0) return unavailable("Unsupported or malformed DTB");
    int selected = -1;
    uint64_t base = 0, size = 0;
    for (unsigned int i = 0; i < tree.count; ++i) {
        if (!fdt_compatible(&tree, i, "riscv,plic0") &&
            !fdt_compatible(&tree, i, "sifive,plic-1.0.0")) continue;
        uint32_t candidate_context;
        if (plic_context(&tree, i, hart_id, &candidate_context) < 0) continue;
        if (selected >= 0) return unavailable("Ambiguous boot-hart PLIC contexts");
        if (fdt_reg(&tree, i, &base, &size) < 0) return unavailable("Unsupported PLIC register range");
        supervisor_context = candidate_context;
        selected = (int)i;
    }
    if (selected < 0) return unavailable("No supported boot-hart supervisor PLIC");
    uint32_t cells;
    if (fdt_u32(&tree, (unsigned int)selected, "#interrupt-cells", &cells) < 0 || cells != 1U ||
        fdt_u32(&tree, (unsigned int)selected, "riscv,ndev", &source_count) < 0 ||
        !source_count || source_count >= IRQ_SOURCE_LIMIT || supervisor_context >= PLIC_CONTEXT_LIMIT) {
        return unavailable("Unsupported PLIC source/context geometry");
    }
    const uint64_t context_offset = PLIC_CONTEXT + (uint64_t)supervisor_context * PLIC_CONTEXT_STRIDE;
    if ((base & (PAGE_BYTES - 1U)) || size < context_offset + PAGE_BYTES ||
        base >= (1UL << 38) || size > (1UL << 38) - base) return unavailable("Invalid PLIC MMIO bounds");
    plic_base = (uintptr_t)base;
    enable_base = plic_base + PLIC_ENABLE + (uintptr_t)supervisor_context * PLIC_ENABLE_STRIDE;
    context_base = plic_base + (uintptr_t)context_offset;

    /* Bind routes by their interrupt-parent phandle, never by VirtIO slot order. */
    for (unsigned int i = 0; i < tree.count; ++i) {
        if (!fdt_compatible(&tree, i, "virtio,mmio")) continue;
        uint32_t parent, length;
        if (interrupt_parent(&tree, i, &parent) < 0 || fdt_phandle(&tree, parent) != selected) continue;
        const uint8_t *interrupt = fdt_property(&tree, i, "interrupts", &length);
        uint64_t address, span;
        if (!interrupt || length != 4U || fdt_reg(&tree, i, &address, &span) < 0 ||
            (address & (PAGE_BYTES - 1U)) || address >= (1UL << 38) || span < 0x100U) {
            return unavailable("Unsupported VirtIO interrupt route");
        }
        const uint32_t source = fdt_be32(interrupt);
        if (!source || source > source_count || route_count == IRQ_ROUTE_LIMIT) return unavailable("Invalid VirtIO IRQ ID");
        for (unsigned int j = 0; j < route_count; ++j) {
            if (routes[j].base == address || routes[j].source == source) return unavailable("Shared/duplicate VirtIO route unsupported");
        }
        routes[route_count++] = (IrqRoute){ (uintptr_t)address, source };
    }
    available = 1;
    return 0;
}

unsigned int irq_mmio_count(void) { return available ? 3U : 0U; }

uintptr_t irq_mmio_base(unsigned int index) {
    if (!available) return 0;
    if (index == 0U) return plic_base;
    if (index == 1U) return enable_base & ~(PAGE_BYTES - 1U);
    if (index == 2U) return context_base;
    return 0;
}

int irq_register_mmio(uintptr_t base, IrqService service, void *device, const char *name) {
    if (!available || enabled || !service || !device || !name) return -1;
    for (unsigned int i = 0; i < route_count; ++i) {
        if (routes[i].base != base) continue;
        IrqSlot *slot = &slots[routes[i].source];
        if (slot->service) return -1;
        *slot = (IrqSlot){ .service = service, .device = device, .name = name };
        return 0;
    }
    return -1;
}

void irq_start(void) {
    if (!available || enabled) return;
    const uint32_t words = (source_count + 32U) / 32U;
    for (uint32_t i = 0; i < words; ++i) mmio_write(enable_base + 4U * i, 0);
    mmio_write(context_base, 0); /* Priority 1 sources exceed threshold 0. */
    unsigned int registered = 0;
    for (uint32_t id = 1; id <= source_count; ++id) {
        if (!slots[id].service) continue;
        mmio_write(plic_base + 4U * id, 1U);
        const uintptr_t word = enable_base + 4U * (id / 32U);
        mmio_write(word, mmio_read(word) | (1U << (id % 32U)));
        ++registered;
    }
    console_puts("[irq] PLIC context="); console_number(supervisor_context);
    console_puts(" sources="); console_number(source_count);
    console_puts(" registered="); console_number(registered); console_putc('\n');
    if (registered) {
        enabled = 1;
        /* Set only SEIE. Keep the non-preemptible kernel's global SIE policy. */
        __asm__ volatile("csrs sie, %0" : : "r"(SEIE) : "memory");
    }
}

static void dispatch_sources(void) {
    ++entries;
    if (!available || !enabled) {
        ++invalid;
        __asm__ volatile("csrc sie, %0" : : "r"(SEIE) : "memory");
        return;
    }
    for (unsigned int batch = 0; batch < IRQ_BATCH_LIMIT; ++batch) {
        const uint32_t id = mmio_read(context_base + 4U);
        if (!id) return;
        if (id > source_count) {
            /* Do not index a callback table or touch an invalid priority register. */
            ++invalid;
            enabled = 0;
            __asm__ volatile("csrc sie, %0" : : "r"(SEIE) : "memory");
            return;
        }
        ++claims;
        IrqSlot *slot = &slots[id];
        if (slot->service) {
            /* Callback acknowledges the device; it must not block or reschedule. */
            if (slot->service(slot->device)) ++slot->handled;
            else ++slot->spurious;
            mmio_write(context_base + 4U, id);
        } else {
            ++unhandled;
            /* PLIC ignores completion for disabled sources: complete BEFORE masking. */
            mmio_write(context_base + 4U, id);
            const uintptr_t word = enable_base + 4U * (id / 32U);
            mmio_write(word, mmio_read(word) & ~(1U << (id % 32U)));
        }
    }
    ++budget_exhausted;
    /*
     * With only disk sources and non-preemptible dispatch, a full batch indicates a stuck or
     * flooding line. A per-entry limit alone would immediately retrap forever.
     * Keep the fallback polling path usable and expose the fault through ps.
     */
    enabled = 0;
    __asm__ volatile("csrc sie, %0" : : "r"(SEIE) : "memory");
}

static int in_handler;

int irq_in_handler(void) { return in_handler; }

void irq_dispatch(void) {
    /* IRQ callbacks may acknowledge DMA, but must never acquire sleeping locks. */
    in_handler = 1;
    dispatch_sources();
    in_handler = 0;
}

void irq_print_stats(void) {
    console_puts("IRQ: PLIC="); console_puts(!available ? "unavailable" : enabled ? "enabled" : "disabled");
    console_puts(" external="); console_number((long)entries);
    console_puts(" claims="); console_number((long)claims);
    console_puts(" unhandled="); console_number((long)unhandled);
    console_puts(" invalid="); console_number((long)invalid);
    console_puts(" budget="); console_number((long)budget_exhausted); console_putc('\n');
    for (uint32_t id = 1; available && id <= source_count; ++id) {
        const IrqSlot *slot = &slots[id];
        if (!slot->service) continue;
        console_puts("  IRQ "); console_number(id); console_putc(' '); console_puts(slot->name);
        console_puts(" handled="); console_number((long)slot->handled);
        console_puts(" spurious="); console_number((long)slot->spurious); console_putc('\n');
    }
}
