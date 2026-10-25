#include "vm.h"
#include "console.h"
#include "syscall_numbers.h"

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
#define PTE_LEAF (PTE_R | PTE_W | PTE_X)
#define KERNEL_BASE 0x80200000UL
#define UART_BASE 0x10000000UL
#define USER_LIMIT (1UL << 38)

typedef uint64_t PageTable[512];
_Static_assert(sizeof(PageTable) == PAGE_SIZE, "Sv39 table occupies one page");

static uintptr_t kernel_root;
static uintptr_t extra_mmio_base;
extern unsigned char __text_end[], __rodata_end[];
extern const unsigned char __user_image_start[], __user_image_end[];
extern const unsigned char __app_image_start[], __app_image_end[];
extern const unsigned char __fan_image_start[], __fan_image_end[];

static _Noreturn void vm_panic(const char *message) {
    console_puts("[vm] ");
    console_puts(message);
    console_putc('\n');
    kernel_idle();
}

static uint64_t table_entry(uintptr_t table) {
    return ((table >> 12) << 10) | PTE_V;
}

static uintptr_t entry_address(uint64_t entry) {
    return (entry >> 10) << 12;
}

/* Return the L0 entry; allocate missing L1/L0 tables only when requested. */
static uint64_t *vm_walk(uintptr_t root, uintptr_t address, int allocate) {
    if (root == 0 || address >= USER_LIMIT) return 0;
    uint64_t *table = (uint64_t *)root;
    for (int level = 2; level > 0; --level) {
        uint64_t *entry = &table[(address >> (12 + 9 * level)) & 511];
        if ((*entry & PTE_V) == 0) {
            if (!allocate) return 0;
            const uintptr_t child = page_alloc();
            if (child == 0) return 0;
            *entry = table_entry(child);
        } else if (*entry & PTE_LEAF) {
            return 0; /* This implementation supports 4 KiB leaves only. */
        }
        table = (uint64_t *)entry_address(*entry);
    }
    return &table[(address >> 12) & 511];
}

static int vm_map(uintptr_t root, uintptr_t virtual, uintptr_t physical,
                  uint64_t permissions) {
    if (((virtual | physical) & (PAGE_SIZE - 1)) != 0) return -1;
    uint64_t *entry = vm_walk(root, virtual, 1);
    if (entry == 0 || (*entry & PTE_V)) return -1;
    *entry = ((physical >> 12) << 10) | permissions | PTE_V | PTE_A | PTE_D;
    return 0;
}

void vm_map_mmio(uintptr_t base) {
    extra_mmio_base = base;
}

void vm_enable(uintptr_t root) {
    const uint64_t satp = (8UL << 60) | (root >> 12);
    __asm__ volatile("sfence.vma zero, zero\ncsrw satp, %0\nsfence.vma zero, zero"
                     : : "r"(satp) : "memory");
    __asm__ volatile("fence.i" : : : "memory");
}

void vm_use_kernel(void) {
    vm_enable(kernel_root);
}

void vm_init(const void *fdt) {
    if ((uintptr_t)__user_image_end - (uintptr_t)__user_image_start > USER_CODE_SIZE + PAGE_SIZE ||
        (uintptr_t)__app_image_end - (uintptr_t)__app_image_start > USER_CODE_SIZE + PAGE_SIZE ||
        (uintptr_t)__fan_image_end - (uintptr_t)__fan_image_start > USER_CODE_SIZE + PAGE_SIZE) {
        vm_panic("User image exceeds code/data limits");
    }
    page_init(fdt);
    kernel_root = page_alloc();
    if (kernel_root == 0) vm_panic("Cannot allocate kernel root");
    /* Identity-map RAM so the kernel can access any allocated physical page. */
    for (uintptr_t address = KERNEL_BASE; address < RAM_END; address += PAGE_SIZE) {
        uint64_t permissions = PTE_R | PTE_W;
        if (address < (uintptr_t)__text_end) permissions = PTE_R | PTE_X;
        else if (address < (uintptr_t)__rodata_end) permissions = PTE_R;
        if (vm_map(kernel_root, address, address, permissions) < 0) {
            vm_panic("Cannot map kernel RAM");
        }
    }
    if (vm_map(kernel_root, UART_BASE, UART_BASE, PTE_R | PTE_W) < 0 ||
        (extra_mmio_base != 0 && extra_mmio_base != UART_BASE &&
         vm_map(kernel_root, extra_mmio_base, extra_mmio_base, PTE_R | PTE_W) < 0)) {
        vm_panic("Cannot map console/block MMIO");
    }
    vm_use_kernel();
    console_puts("[vm] Sv39 L2 -> L1 -> L0: 512 entries/table, 4 KiB pages\n");
    console_puts("[vm] User code RX; data/stack RW; kernel/RAM/MMIO supervisor-only\n");
    page_print_stats();
}

/* Compare with the kernel template at the same tree position to skip shared branches. */
static void free_table(uintptr_t address, uintptr_t shared, int level) {
    uint64_t *table = (uint64_t *)address;
    const uint64_t *template = (const uint64_t *)shared;
    for (size_t i = 0; i < 512; ++i) {
        const uint64_t entry = table[i];
        const uint64_t common = template != 0 ? template[i] : 0;
        if ((entry & PTE_V) == 0 || entry == common) continue;
        if (entry & PTE_LEAF) {
            if (entry & PTE_U) page_free(entry_address(entry));
        } else {
            if (level == 0) vm_panic("Non-leaf at L0 during destroy");
            const uintptr_t shared_child = (common & PTE_V) && !(common & PTE_LEAF)
                ? entry_address(common) : 0;
            free_table(entry_address(entry), shared_child, level - 1);
        }
    }
    page_free(address);
}

void vm_destroy(uintptr_t root) {
    if (root == 0) return;
    uintptr_t satp;
    __asm__ volatile("csrr %0, satp" : "=r"(satp));
    if (root == kernel_root || (((satp & ((1UL << 44) - 1)) << 12) == root)) {
        vm_panic("Attempt to destroy active or shared kernel root");
    }
    free_table(root, kernel_root, 2);
}

static uintptr_t user_physical(uintptr_t root, uintptr_t address, uint64_t permission) {
    const uint64_t *entry = vm_walk(root, address, 0);
    if (entry == 0 || (*entry & (PTE_V | PTE_U | PTE_R | permission)) !=
        (PTE_V | PTE_U | PTE_R | permission)) return 0;
    return entry_address(*entry) | (address & (PAGE_SIZE - 1));
}

uintptr_t vm_create(long program) {
    const uintptr_t root = page_alloc();
    if (root == 0) return 0;
    uint64_t *table = (uint64_t *)root;
    const uint64_t *common = (const uint64_t *)kernel_root;
    for (size_t i = 0; i < 512; ++i) table[i] = common[i];
    /* User addresses and MMIO share L2[0]; keep its L1 private. */
    table[0] = 0;
    const uintptr_t low = page_alloc();
    if (low == 0) {
        vm_destroy(root);
        return 0;
    }
    const uint64_t *shared_low = (const uint64_t *)entry_address(common[0]);
    for (size_t i = 0; i < 512; ++i) ((uint64_t *)low)[i] = shared_low[i];
    table[0] = table_entry(low);
    for (size_t i = 0; i < USER_CODE_SIZE / PAGE_SIZE + 2; ++i) {
        const uintptr_t virtual = i <= USER_CODE_SIZE / PAGE_SIZE
            ? USER_ENTRY + i * PAGE_SIZE : USER_STACK;
        const uint64_t permissions = PTE_U | PTE_R |
            (i < USER_CODE_SIZE / PAGE_SIZE ? PTE_X : PTE_W);
        const uintptr_t physical = page_alloc();
        if (physical == 0 || vm_map(root, virtual, physical, permissions) < 0) {
            if (physical != 0) page_free(physical);
            vm_destroy(root);
            return 0;
        }
    }
    const unsigned char *image = program == 0 ? __user_image_start :
        program == PROGRAM_FAN ? __fan_image_start : __app_image_start;
    const unsigned char *end = program == 0 ? __user_image_end :
        program == PROGRAM_FAN ? __fan_image_end : __app_image_end;
    const size_t length = (uintptr_t)end - (uintptr_t)image;
    /* Initialization uses supervisor identity mappings, including RX user code pages. */
    for (size_t copied = 0; copied < length;) {
        const uintptr_t physical = user_physical(root, USER_ENTRY + copied, PTE_R);
        size_t amount = PAGE_SIZE - (copied & (PAGE_SIZE - 1));
        if (amount > length - copied) amount = length - copied;
        for (size_t i = 0; i < amount; ++i) ((uint8_t *)physical)[i] = image[copied + i];
        copied += amount;
    }
    return root;
}

int vm_clone(uintptr_t destination, uintptr_t source) {
    for (size_t i = 0; i < USER_CODE_SIZE / PAGE_SIZE + 2; ++i) {
        const uintptr_t virtual = i <= USER_CODE_SIZE / PAGE_SIZE
            ? USER_ENTRY + i * PAGE_SIZE : USER_STACK;
        const uintptr_t from = user_physical(source, virtual, PTE_R);
        const uintptr_t to = user_physical(destination, virtual, PTE_R);
        if (from == 0 || to == 0) return -1;
        for (size_t byte = 0; byte < PAGE_SIZE; ++byte) {
            ((uint8_t *)to)[byte] = ((const uint8_t *)from)[byte];
        }
    }
    return 0;
}

static int user_accessible(uintptr_t root, uintptr_t address, size_t length,
                           uint64_t permission) {
    if (length == 0) return 1;
    if (length - 1 > UINTPTR_MAX - address) return 0;
    const uintptr_t last = address + length - 1;
    for (;;) {
        if (user_physical(root, address, permission) == 0) return 0;
        if ((address >> 12) == (last >> 12)) return 1;
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
    for (size_t i = 0; i < sizeof(value); ++i) user_write_byte(root, address + i, bytes[i]);
}

void user_write_byte(uintptr_t root, uintptr_t address, unsigned char value) {
    *(unsigned char *)user_physical(root, address, PTE_R | PTE_W) = value;
}
