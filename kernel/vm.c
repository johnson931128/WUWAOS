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
static PageTable root __attribute__((aligned(PAGE_SIZE)));
static PageTable low_l1 __attribute__((aligned(PAGE_SIZE)));
static PageTable kernel_l1 __attribute__((aligned(PAGE_SIZE)));
static PageTable kernel_l0 __attribute__((aligned(PAGE_SIZE)));
static PageTable uart_l0 __attribute__((aligned(PAGE_SIZE)));
static PageTable user_l0 __attribute__((aligned(PAGE_SIZE)));
static PageTable stack_l0 __attribute__((aligned(PAGE_SIZE)));
static unsigned char user_code[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static unsigned char user_data[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static unsigned char user_stack[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

extern unsigned char __text_end[], __rodata_end[], __kernel_end[];
extern const unsigned char __user_image_start[], __user_image_end[];

static uint64_t table_entry(const void *table) {
    return (((uintptr_t)table >> 12) << 10) | PTE_V;
}

static uint64_t leaf_entry(uintptr_t physical, uint64_t permissions) {
    return ((physical >> 12) << 10) | permissions | PTE_V | PTE_A | PTE_D;
}

uintptr_t vm_init(void) {
    const size_t image_size = (uintptr_t)__user_image_end -
                              (uintptr_t)__user_image_start;
    if (image_size > 2 * PAGE_SIZE) {
        console_puts("[vm] User image exceeds two pages\n");
        kernel_idle();
    }
    for (size_t i = 0; i < image_size; ++i) {
        if (i < PAGE_SIZE) {
            user_code[i] = __user_image_start[i];
        } else {
            user_data[i - PAGE_SIZE] = __user_image_start[i];
        }
    }

    root[2] = table_entry(kernel_l1);
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

    root[0] = table_entry(low_l1);
    low_l1[128] = table_entry(uart_l0);
    uart_l0[0] = leaf_entry(UART_BASE, PTE_R | PTE_W);
    low_l1[2] = table_entry(user_l0);
    user_l0[0] = leaf_entry((uintptr_t)user_code, PTE_U | PTE_R | PTE_X);
    user_l0[1] = leaf_entry((uintptr_t)user_data, PTE_U | PTE_R | PTE_W);
    low_l1[4] = table_entry(stack_l0);
    stack_l0[0] = leaf_entry((uintptr_t)user_stack, PTE_U | PTE_R | PTE_W);

    console_puts("[vm] Sv39: user code RX; data/stack RW; kernel/UART supervisor-only\n");
    return (uintptr_t)root;
}

void vm_enable(uintptr_t table) {
    const uint64_t satp = (8UL << 60) | (table >> 12);
    __asm__ volatile("sfence.vma zero, zero\ncsrw satp, %0\nsfence.vma zero, zero"
                     : : "r"(satp) : "memory");
    __asm__ volatile("fence.i" : : : "memory");
}

/* This fixed address space has only 4 KiB leaves; no dynamic mappings. */
static uintptr_t user_physical(uintptr_t address) {
    if (address >= (1UL << 38)) {
        return 0;
    }
    uint64_t *table = root;
    for (int level = 2; level >= 0; --level) {
        const uint64_t entry = table[(address >> (12 + 9 * level)) & 511];
        if ((entry & PTE_V) == 0) {
            return 0;
        }
        if (entry & (PTE_R | PTE_W | PTE_X)) {
            if (level != 0 || (entry & (PTE_U | PTE_R)) != (PTE_U | PTE_R)) {
                return 0;
            }
            return ((entry >> 10) << 12) | (address & (PAGE_SIZE - 1));
        }
        table = (uint64_t *)((entry >> 10) << 12);
    }
    return 0;
}

int user_readable(uintptr_t address, size_t length) {
    if (length == 0) {
        return 1;
    }
    if (length - 1 > UINTPTR_MAX - address) {
        return 0;
    }
    const uintptr_t last = address + length - 1;
    for (;;) {
        if (user_physical(address) == 0) {
            return 0;
        }
        if ((address >> 12) == (last >> 12)) {
            return 1;
        }
        address = (address & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }
}

unsigned char user_read_byte(uintptr_t address) {
    /* Caller validated the complete range; mappings cannot change here. */
    return *(const unsigned char *)user_physical(address);
}
