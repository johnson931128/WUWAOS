#include "vm.h"
#include "console.h"

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
#define KERNEL_BASE 0x80200000UL
#define UART_BASE 0x10000000UL

typedef uint64_t PageTable[512];
static PageTable kernel_l1 __attribute__((aligned(PAGE_SIZE)));
static PageTable kernel_l0 __attribute__((aligned(PAGE_SIZE)));
static PageTable uart_l0 __attribute__((aligned(PAGE_SIZE)));
static uintptr_t extra_mmio_base;

typedef struct {
    PageTable root;
    PageTable low_l1;
    PageTable user_l0;
    PageTable stack_l0;
    unsigned char code[USER_CODE_SIZE];
    unsigned char data[PAGE_SIZE];
    unsigned char stack[PAGE_SIZE];
} AddressSpace;

static AddressSpace storage[PROCESS_LIMIT + 1] __attribute__((aligned(PAGE_SIZE)));
static AddressSpace *spaces[PROCESS_LIMIT];
static AddressSpace *spare;

extern unsigned char __text_end[], __rodata_end[], __kernel_end[];
extern const unsigned char __user_image_start[], __user_image_end[];
extern const unsigned char __app_image_start[], __app_image_end[];

void vm_map_mmio(uintptr_t base) {
    extra_mmio_base = base;
}

static uint64_t table_entry(const void *table) {
    return (((uintptr_t)table >> 12) << 10) | PTE_V;
}

static uint64_t leaf_entry(uintptr_t physical, uint64_t permissions) {
    return ((physical >> 12) << 10) | permissions | PTE_V | PTE_A | PTE_D;
}

void vm_init(void) {
    const size_t image_size = (uintptr_t)__user_image_end -
                              (uintptr_t)__user_image_start;
    const size_t app_size = (uintptr_t)__app_image_end - (uintptr_t)__app_image_start;
    if (image_size > USER_CODE_SIZE + PAGE_SIZE || app_size > USER_CODE_SIZE + PAGE_SIZE) {
        console_puts("[vm] User image exceeds code/data limits\n");
        kernel_idle();
    }
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        spaces[i] = &storage[i];
    }
    spare = &storage[PROCESS_LIMIT];
    kernel_l1[1] = table_entry(kernel_l0);
    for (uintptr_t address = KERNEL_BASE;
         address < (uintptr_t)__kernel_end; address += PAGE_SIZE) {
        uint64_t permissions = PTE_R | PTE_W;
        if (address < (uintptr_t)__text_end) {
            permissions = PTE_R | PTE_X;
        } else if (address < (uintptr_t)__rodata_end) {
            permissions = PTE_R;
        }
        kernel_l0[(address >> 12) & 511] = leaf_entry(address, permissions);
    }

    uart_l0[0] = leaf_entry(UART_BASE, PTE_R | PTE_W);
    if (extra_mmio_base != 0 && (extra_mmio_base >> 21) == (UART_BASE >> 21)) {
        uart_l0[(extra_mmio_base >> 12) & 511] = leaf_entry(extra_mmio_base, PTE_R | PTE_W);
    }
    console_puts("[vm] Sv39: user code RX; data/stack RW; kernel/UART supervisor-only\n");
}

static void prepare_space(AddressSpace *space, long program) {
    /* Clear reused mappings and private memory before publishing the process. */
    volatile unsigned char *bytes = (volatile unsigned char *)space;
    for (size_t i = 0; i < sizeof(*space); ++i) {
        bytes[i] = 0;
    }
    const unsigned char *image = program == 0 ? __user_image_start : __app_image_start;
    const unsigned char *end = program == 0 ? __user_image_end : __app_image_end;
    const size_t image_size = (uintptr_t)end - (uintptr_t)image;
    for (size_t i = 0; i < image_size; ++i) {
        if (i < USER_CODE_SIZE) {
            space->code[i] = image[i];
        } else {
            space->data[i - USER_CODE_SIZE] = image[i];
        }
    }
    space->root[2] = table_entry(kernel_l1);
    space->root[0] = table_entry(space->low_l1);
    space->low_l1[128] = table_entry(uart_l0);
    space->low_l1[2] = table_entry(space->user_l0);
    for (size_t i = 0; i < USER_CODE_SIZE / PAGE_SIZE; ++i) {
        space->user_l0[i] = leaf_entry((uintptr_t)space->code + i * PAGE_SIZE,
                                      PTE_U | PTE_R | PTE_X);
    }
    space->user_l0[USER_CODE_SIZE / PAGE_SIZE] =
        leaf_entry((uintptr_t)space->data, PTE_U | PTE_R | PTE_W);
    space->low_l1[4] = table_entry(space->stack_l0);
    space->stack_l0[0] = leaf_entry((uintptr_t)space->stack, PTE_U | PTE_R | PTE_W);
}

uintptr_t vm_create(unsigned int slot, long program) {
    if (slot >= PROCESS_LIMIT) {
        console_puts("[vm] Invalid address-space slot\n");
        kernel_idle();
    }
    prepare_space(spaces[slot], program);
    return (uintptr_t)spaces[slot]->root;
}

static void copy_bytes(void *destination, const void *source, size_t length) {
    volatile unsigned char *to = destination;
    const unsigned char *from = source;
    for (size_t i = 0; i < length; ++i) {
        to[i] = from[i];
    }
}

void vm_clone(unsigned int slot, uintptr_t source_root) {
    /* root is the first member; copy user pages, never the parent's PTEs. */
    const AddressSpace *source = (const AddressSpace *)source_root;
    copy_bytes(spaces[slot]->code, source->code, USER_CODE_SIZE);
    copy_bytes(spaces[slot]->data, source->data, PAGE_SIZE);
    copy_bytes(spaces[slot]->stack, source->stack, PAGE_SIZE);
}

uintptr_t vm_exec(unsigned int slot, long program) {
    /* Caller validated the image ID. All remaining resources are preallocated. */
    prepare_space(spare, program);
    AddressSpace *old = spaces[slot];
    spaces[slot] = spare;
    spare = old;
    return (uintptr_t)spaces[slot]->root;
}

void vm_enable(uintptr_t table) {
    const uint64_t satp = (8UL << 60) | (table >> 12);
    __asm__ volatile("sfence.vma zero, zero\ncsrw satp, %0\nsfence.vma zero, zero"
                     : : "r"(satp) : "memory");
    __asm__ volatile("fence.i" : : : "memory");
}

/* This fixed address space has only 4 KiB leaves; no dynamic mappings. */
static uintptr_t user_physical(uintptr_t root, uintptr_t address, uint64_t permission) {
    if (address >= (1UL << 38)) {
        return 0;
    }
    uint64_t *table = (uint64_t *)root;
    for (int level = 2; level >= 0; --level) {
        const uint64_t entry = table[(address >> (12 + 9 * level)) & 511];
        if ((entry & PTE_V) == 0) {
            return 0;
        }
        if (entry & (PTE_R | PTE_W | PTE_X)) {
            if (level != 0 || (entry & (PTE_U | permission)) != (PTE_U | permission)) {
                return 0;
            }
            return ((entry >> 10) << 12) | (address & (PAGE_SIZE - 1));
        }
        table = (uint64_t *)((entry >> 10) << 12);
    }
    return 0;
}

static int user_accessible(uintptr_t root, uintptr_t address, size_t length,
                           uint64_t permission) {
    if (length == 0) {
        return 1;
    }
    if (length - 1 > UINTPTR_MAX - address) {
        return 0;
    }
    const uintptr_t last = address + length - 1;
    for (;;) {
        if (user_physical(root, address, permission) == 0) {
            return 0;
        }
        if ((address >> 12) == (last >> 12)) {
            return 1;
        }
        address = (address & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }
}

int user_readable(uintptr_t root, uintptr_t address, size_t length) {
    return user_accessible(root, address, length, PTE_R);
}

int user_writable(uintptr_t root, uintptr_t address, size_t length) {
    return user_accessible(root, address, length, PTE_R | PTE_W);
}

unsigned char user_read_byte(uintptr_t root, uintptr_t address) {
    /* Caller validated the complete range; mappings cannot change here. */
    return *(const unsigned char *)user_physical(root, address, PTE_R);
}

void user_write_long(uintptr_t root, uintptr_t address, long value) {
    const unsigned char *bytes = (const unsigned char *)&value;
    for (size_t i = 0; i < sizeof(value); ++i) {
        user_write_byte(root, address + i, bytes[i]);
    }
}

void user_write_byte(uintptr_t root, uintptr_t address, unsigned char value) {
    *(unsigned char *)user_physical(root, address, PTE_R | PTE_W) = value;
}
