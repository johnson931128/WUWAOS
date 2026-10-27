#include "vm.h"
#include "console.h"
#include "syscall_numbers.h"
#include "process_limits.h"
#include "swap.h"

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
#define FIXED_PAGES (USER_CODE_SIZE / PAGE_SIZE + 2)
#define SPACE_PAGES (FIXED_PAGES + USER_HEAP_PAGES)

typedef uint64_t PageTable[512];
_Static_assert(sizeof(PageTable) == PAGE_SIZE, "Sv39 table occupies one page");
typedef struct {
    uintptr_t physical;
    uint64_t arrival;
    unsigned int slot, pins;
    int dirty;
} VmPage;
typedef struct {
    uintptr_t root, heap_end;
    long program;
    size_t faults;
    VmPage pages[SPACE_PAGES];
} VmSpace;

/* Metadata only; physical user memory is never preallocated here. */
static VmSpace spaces[PROCESS_LIMIT + 1];
static uintptr_t kernel_root, mmio[2];
static size_t mmio_count, resident, frame_limit = VM_DEFAULT_FRAMES;
static uint64_t next_arrival;
static struct {
    size_t faults, soft_loads, images, zeroes, ins, outs, drops, invalid, io_errors, copies;
} stats;
static unsigned char clone_buffer[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
extern unsigned char __text_end[], __rodata_end[];
extern const unsigned char __user_image_start[], __user_image_end[];
extern const unsigned char __app_image_start[], __app_image_end[];
extern const unsigned char __fan_image_start[], __fan_image_end[];

static _Noreturn void vm_panic(const char *message) {
    console_puts("[vm] "); console_puts(message); console_putc('\n'); kernel_idle();
}
static void flush(void) { __asm__ volatile("sfence.vma zero, zero" : : : "memory"); }
static uint64_t table_entry(uintptr_t table) { return ((table >> 12) << 10) | PTE_V; }
static uintptr_t entry_address(uint64_t entry) { return (entry >> 10) << 12; }
static int evict_one(void);

static uintptr_t table_alloc(void) {
    uintptr_t page = page_alloc();
    if (page == 0 && evict_one() == 0) page = page_alloc();
    return page;
}

static uint64_t *vm_walk(uintptr_t root, uintptr_t address, int allocate) {
    if (root == 0 || address >= USER_LIMIT) return 0;
    uint64_t *table = (uint64_t *)root;
    for (int level = 2; level > 0; --level) {
        uint64_t *entry = &table[(address >> (12 + 9 * level)) & 511];
        if ((*entry & PTE_V) == 0) {
            if (!allocate) return 0;
            const uintptr_t child = table_alloc();
            if (child == 0) return 0;
            *entry = table_entry(child);
        } else if (*entry & PTE_LEAF) return 0;
        table = (uint64_t *)entry_address(*entry);
    }
    return &table[(address >> 12) & 511];
}

static int vm_map(uintptr_t root, uintptr_t virtual, uintptr_t physical, uint64_t permissions) {
    if (((virtual | physical) & (PAGE_SIZE - 1)) != 0) return -1;
    uint64_t *entry = vm_walk(root, virtual, 1);
    if (entry == 0 || (*entry & PTE_V)) return -1;
    *entry = ((physical >> 12) << 10) | permissions | PTE_V | PTE_A | PTE_D;
    return 0;
}

static VmSpace *find_space(uintptr_t root) {
    if (root == 0) return 0;
    for (size_t i = 0; i < PROCESS_LIMIT + 1; ++i) if (spaces[i].root == root) return &spaces[i];
    return 0;
}

static uintptr_t page_address(size_t index) {
    if (index < FIXED_PAGES - 1) return USER_ENTRY + index * PAGE_SIZE;
    if (index == FIXED_PAGES - 1) return USER_STACK;
    return USER_HEAP_BASE + (index - FIXED_PAGES) * PAGE_SIZE;
}

static int page_index(const VmSpace *space, uintptr_t address) {
    if (space == 0) return -1;
    if (address >= USER_ENTRY && address < USER_DATA + PAGE_SIZE) {
        return (int)((address - USER_ENTRY) / PAGE_SIZE);
    }
    if (address >= USER_STACK && address < USER_STACK_TOP) return FIXED_PAGES - 1;
    /* Hardware mappings expose the rounded last page, as with a conventional brk. */
    const uintptr_t heap_top = (space->heap_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (address >= USER_HEAP_BASE && address < heap_top) {
        return (int)(FIXED_PAGES + (address - USER_HEAP_BASE) / PAGE_SIZE);
    }
    return -1;
}

static uint64_t permissions(size_t index) {
    return PTE_U | PTE_R | (index < USER_CODE_SIZE / PAGE_SIZE ? PTE_X : PTE_W);
}

/* FIFO among eligible pages. Hits never change arrival; pinned pages cannot move. */
static int evict_one(void) {
    uint64_t after = 0;
    int error = ERR_NO_MEMORY;
    for (;;) {
        VmPage *victim = 0;
        VmSpace *owner = 0;
        size_t selected = 0;
        for (size_t s = 0; s < PROCESS_LIMIT + 1; ++s) {
            if (spaces[s].root == 0) continue;
            for (size_t i = 0; i < SPACE_PAGES; ++i) {
                VmPage *page = &spaces[s].pages[i];
                if (page->physical && !page->pins && page->arrival > after &&
                    (victim == 0 || page->arrival < victim->arrival)) {
                    victim = page; owner = &spaces[s]; selected = i;
                }
            }
        }
        if (victim == 0) return error;
        after = victim->arrival;
        uint64_t *entry = vm_walk(owner->root, page_address(selected), 0);
        if (entry == 0 || !(*entry & PTE_V)) vm_panic("Resident metadata/PTE mismatch");
        const int dirty = victim->dirty || (*entry & PTE_D);
        unsigned int slot = victim->slot;
        if (dirty) {
            if (slot == 0) slot = swap_alloc();
            if (slot == 0) continue;
            if (swap_write(slot, victim->physical) < 0) {
                if (victim->slot == 0) swap_free(slot);
                ++stats.io_errors;
                error = ERR_IO;
                continue; /* Keep the valid RAM copy; try another candidate. */
            }
            victim->slot = slot;
            ++stats.outs;
        } else ++stats.drops;
        *entry = 0;
        flush(); /* Revoke the old virtual mapping before reusing its frame. */
        page_free(victim->physical);
        victim->physical = 0;
        victim->dirty = 0;
        --resident;
        return 0;
    }
}

static int obtain_frame(uintptr_t *result) {
    if (resident >= frame_limit) {
        const int error = evict_one();
        if (error < 0) return error;
    }
    *result = page_alloc();
    if (*result != 0) return 0;
    const int error = evict_one();
    if (error < 0) return error;
    *result = page_alloc();
    return *result == 0 ? ERR_NO_MEMORY : 0;
}

static void image_for(long program, const unsigned char **start, size_t *length) {
    *start = program == 0 ? __user_image_start :
        program == PROGRAM_FAN ? __fan_image_start : __app_image_start;
    const unsigned char *end = program == 0 ? __user_image_end :
        program == PROGRAM_FAN ? __fan_image_end : __app_image_end;
    *length = (uintptr_t)end - (uintptr_t)*start;
}

static int resident_page(VmSpace *space, size_t index, int write, uintptr_t *physical) {
    VmPage *page = &space->pages[index];
    uint64_t *entry = vm_walk(space->root, page_address(index), 1);
    if (entry == 0) return ERR_NO_MEMORY;
    const uint64_t old_entry = *entry;
    if (page->physical == 0) {
        uintptr_t frame;
        const int error = obtain_frame(&frame);
        if (error < 0) return error;
        if (page->slot != 0) {
            if (swap_read(page->slot, frame) < 0) {
                page_free(frame);
                ++stats.io_errors;
                return ERR_IO;
            }
            ++stats.ins;
        } else if (index < FIXED_PAGES - 1) {
            const unsigned char *image;
            size_t size;
            image_for(space->program, &image, &size);
            const size_t offset = index * PAGE_SIZE;
            size_t amount = offset < size ? size - offset : 0;
            if (amount > PAGE_SIZE) amount = PAGE_SIZE;
            for (size_t i = 0; i < amount; ++i) ((unsigned char *)frame)[i] = image[offset + i];
            ++stats.images;
        } else ++stats.zeroes;
        page->physical = frame;
        page->arrival = ++next_arrival;
        page->dirty = 0;
        ++resident;
        *entry = ((frame >> 12) << 10) | permissions(index) | PTE_V | PTE_A;
    }
    if (write) { page->dirty = 1; *entry |= PTE_D; }
    *entry |= PTE_A;
    *physical = page->physical;
    if (*entry != old_entry) flush();
    return 0;
}

int vm_handle_fault(uintptr_t root, uintptr_t address, unsigned long cause, uintptr_t pc) {
    ++stats.faults;
    VmSpace *space = find_space(root);
    const int index = page_index(space, address);
    const uint64_t needed = cause == 12 ? PTE_X : cause == 13 ? PTE_R : PTE_W;
    if (index < 0 || (permissions((size_t)index) & needed) != needed) {
        ++stats.invalid;
        return ERR_ADDRESS;
    }
    ++space->faults;
    /* Keep both possible instruction fragments while installing its missing operand.
       Otherwise a full swap disk can cause code/data faults to evict each other forever. */
    const int instruction = page_index(space, pc);
    const int fragment = pc <= UINTPTR_MAX - 3 ? page_index(space, pc + 3) : -1;
    VmPage *first = instruction >= 0 && (permissions((size_t)instruction) & PTE_X)
        ? &space->pages[instruction] : 0;
    VmPage *second = fragment >= 0 && fragment != instruction && (permissions((size_t)fragment) & PTE_X)
        ? &space->pages[fragment] : 0;
    if (first != 0) ++first->pins;
    if (second != 0) ++second->pins;
    uintptr_t physical;
    const int error = resident_page(space, (size_t)index, cause == 15, &physical);
    if (first != 0) --first->pins;
    if (second != 0) --second->pins;
    return error;
}

void vm_map_mmio(uintptr_t base) {
    if (base == 0 || base == UART_BASE) return;
    for (size_t i = 0; i < mmio_count; ++i) if (mmio[i] == base) return;
    if (mmio_count == 2) vm_panic("Too many block MMIO pages");
    mmio[mmio_count++] = base;
}

void vm_enable(uintptr_t root) {
    const uint64_t satp = (8UL << 60) | (root >> 12);
    flush();
    __asm__ volatile("csrw satp, %0\nsfence.vma zero, zero\nfence.i" : : "r"(satp) : "memory");
}
void vm_use_kernel(void) { vm_enable(kernel_root); }

void vm_init(const void *fdt) {
    for (long program = 0; program <= PROGRAM_FAN; ++program) {
        const unsigned char *image;
        size_t size;
        image_for(program, &image, &size);
        if (size > USER_CODE_SIZE + PAGE_SIZE) vm_panic("Embedded image too large");
    }
    page_init(fdt);
    kernel_root = page_alloc();
    if (kernel_root == 0) vm_panic("Cannot allocate kernel root");
    for (uintptr_t address = KERNEL_BASE; address < RAM_END; address += PAGE_SIZE) {
        uint64_t flags = PTE_R | PTE_W;
        if (address < (uintptr_t)__text_end) flags = PTE_R | PTE_X;
        else if (address < (uintptr_t)__rodata_end) flags = PTE_R;
        if (vm_map(kernel_root, address, address, flags) < 0) vm_panic("Cannot map kernel RAM");
    }
    if (vm_map(kernel_root, UART_BASE, UART_BASE, PTE_R | PTE_W) < 0) vm_panic("Cannot map UART");
    for (size_t i = 0; i < mmio_count; ++i) {
        if (vm_map(kernel_root, mmio[i], mmio[i], PTE_R | PTE_W) < 0) vm_panic("Cannot map block MMIO");
    }
    vm_use_kernel();
    swap_init();
    console_puts("[vm] Sv39 L2 -> L1 -> L0; demand paging; global FIFO; user frame budget=");
    console_number((long)frame_limit);
    console_puts("\n[vm] User code RX; data/stack/heap RW; kernel/RAM/MMIO supervisor-only\n");
    page_print_stats();
}

static void release_page(VmSpace *space, size_t index) {
    VmPage *page = &space->pages[index];
    if (page->pins) vm_panic("Release of pinned page");
    if (page->physical) {
        uint64_t *entry = vm_walk(space->root, page_address(index), 0);
        if (entry == 0) vm_panic("Missing resident PTE");
        *entry = 0;
        flush();
        page_free(page->physical);
        --resident;
    }
    if (page->slot) swap_free(page->slot);
    *page = (VmPage){0};
}

/* User frames were released through metadata; shared kernel branches stay alive. */
static void free_table(uintptr_t address, uintptr_t shared, int level) {
    const uint64_t *table = (const uint64_t *)address;
    const uint64_t *template = (const uint64_t *)shared;
    for (size_t i = 0; i < 512; ++i) {
        const uint64_t entry = table[i], common = template != 0 ? template[i] : 0;
        if (!(entry & PTE_V) || entry == common || (entry & PTE_LEAF)) continue;
        if (level == 0) vm_panic("Non-leaf at L0");
        const uintptr_t child = (common & PTE_V) && !(common & PTE_LEAF) ? entry_address(common) : 0;
        free_table(entry_address(entry), child, level - 1);
    }
    page_free(address);
}

void vm_destroy(uintptr_t root) {
    if (root == 0) return;
    uintptr_t satp;
    __asm__ volatile("csrr %0, satp" : "=r"(satp));
    if (root == kernel_root || (((satp & ((1UL << 44) - 1)) << 12) == root)) vm_panic("Destroy active root");
    VmSpace *space = find_space(root);
    if (space == 0) vm_panic("Unknown address space");
    for (size_t i = 0; i < SPACE_PAGES; ++i) release_page(space, i);
    free_table(root, kernel_root, 2);
    space->root = 0;
}

uintptr_t vm_create(long program) {
    VmSpace *space = 0;
    for (size_t i = 0; i < PROCESS_LIMIT + 1; ++i) if (spaces[i].root == 0) { space = &spaces[i]; break; }
    if (space == 0) return 0;
    unsigned char *bytes = (unsigned char *)space;
    for (size_t i = 0; i < sizeof(*space); ++i) bytes[i] = 0;
    const uintptr_t root = table_alloc();
    if (root == 0) return 0;
    space->root = root;
    space->program = program;
    space->heap_end = USER_HEAP_BASE;
    uint64_t *table = (uint64_t *)root;
    const uint64_t *common = (const uint64_t *)kernel_root;
    for (size_t i = 0; i < 512; ++i) table[i] = common[i];
    table[0] = 0;
    const uintptr_t low = table_alloc();
    if (low == 0) { vm_destroy(root); return 0; }
    const uint64_t *shared_low = (const uint64_t *)entry_address(common[0]);
    for (size_t i = 0; i < 512; ++i) ((uint64_t *)low)[i] = shared_low[i];
    table[0] = table_entry(low);
    return root;
}

int vm_clone(uintptr_t destination, uintptr_t source) {
    VmSpace *to = find_space(destination), *from = find_space(source);
    if (to == 0 || from == 0) return ERR_ADDRESS;
    to->heap_end = from->heap_end;
    for (size_t i = 0; i < SPACE_PAGES; ++i) {
        VmPage *page = &from->pages[i];
        if (page->physical) {
            ++page->pins;
            uintptr_t target;
            const int error = resident_page(to, i, 1, &target);
            if (error < 0) { --page->pins; return error; }
            for (size_t j = 0; j < PAGE_SIZE; ++j) ((unsigned char *)target)[j] = ((unsigned char *)page->physical)[j];
            --page->pins;
        } else if (page->slot) {
            const unsigned int slot = swap_alloc();
            if (slot == 0) return ERR_NO_MEMORY;
            if (swap_read(page->slot, (uintptr_t)clone_buffer) < 0 ||
                swap_write(slot, (uintptr_t)clone_buffer) < 0) {
                swap_free(slot);
                ++stats.io_errors;
                return ERR_IO;
            }
            to->pages[i].slot = slot;
            ++stats.copies;
        }
    }
    return 0;
}

long vm_sbrk(uintptr_t root, long increment) {
    VmSpace *space = find_space(root);
    if (space == 0) return ERR_ADDRESS;
    const uintptr_t old = space->heap_end;
    uintptr_t next;
    if (increment >= 0) {
        if ((uintptr_t)increment > USER_HEAP_BASE + USER_HEAP_PAGES * PAGE_SIZE - old) return ERR_NO_MEMORY;
        next = old + (uintptr_t)increment;
    } else {
        const uintptr_t amount = 0UL - (uintptr_t)increment;
        if (amount > old - USER_HEAP_BASE) return ERR_INVALID;
        next = old - amount;
    }
    if (next < old) {
        for (uintptr_t at = (next + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); at < old; at += PAGE_SIZE) {
            release_page(space, FIXED_PAGES + (at - USER_HEAP_BASE) / PAGE_SIZE);
        }
    }
    space->heap_end = next;
    return (long)old;
}

static int accessible(uintptr_t root, uintptr_t address, size_t length, uint64_t permission) {
    if (length == 0) return 1;
    if (length - 1 > UINTPTR_MAX - address) return 0;
    VmSpace *space = find_space(root);
    const uintptr_t last = address + length - 1;
    for (;;) {
        const int index = page_index(space, address);
        if (index < 0 || (permissions((size_t)index) & permission) != permission) return 0;
        if ((address >> 12) == (last >> 12)) return 1;
        address = (address & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }
}
int user_readable(uintptr_t root, uintptr_t address, size_t length) { return accessible(root, address, length, PTE_R); }
int user_writable(uintptr_t root, uintptr_t address, size_t length) { return accessible(root, address, length, PTE_R | PTE_W); }

static int copy_user(uintptr_t root, uintptr_t user, void *buffer, size_t length, int write) {
    if (!accessible(root, user, length, write ? PTE_R | PTE_W : PTE_R)) return ERR_ADDRESS;
    VmSpace *space = find_space(root);
    size_t copied = 0;
    while (copied < length) {
        const int index = page_index(space, user + copied);
        if (!space->pages[index].physical) ++stats.soft_loads;
        uintptr_t physical;
        const int error = resident_page(space, (size_t)index, write, &physical);
        if (error < 0) return error;
        const size_t offset = (user + copied) & (PAGE_SIZE - 1);
        size_t amount = PAGE_SIZE - offset;
        if (amount > length - copied) amount = length - copied;
        unsigned char *bytes = (unsigned char *)buffer;
        for (size_t i = 0; i < amount; ++i) {
            if (write) ((unsigned char *)physical)[offset + i] = bytes[copied + i];
            else bytes[copied + i] = ((unsigned char *)physical)[offset + i];
        }
        copied += amount;
    }
    return 0;
}
int user_copy_from(uintptr_t root, void *to, uintptr_t from, size_t length) { return copy_user(root, from, to, length, 0); }
int user_copy_to(uintptr_t root, uintptr_t to, const void *from, size_t length) { return copy_user(root, to, (void *)from, length, 1); }
int user_read_byte(uintptr_t root, uintptr_t address, unsigned char *result) { return user_copy_from(root, result, address, 1); }
int user_write_byte(uintptr_t root, uintptr_t address, unsigned char value) { return user_copy_to(root, address, &value, 1); }
int user_write_long(uintptr_t root, uintptr_t address, long value) { return user_copy_to(root, address, &value, sizeof(value)); }

long vm_control(long limit) {
    if (limit != 0) {
        if (limit < (long)VM_MIN_FRAMES || limit > (long)(RAM_END - RAM_BASE) / (long)PAGE_SIZE) return ERR_INVALID;
        while (resident > (size_t)limit) {
            const int error = evict_one();
            if (error < 0) return error;
        }
        frame_limit = (size_t)limit;
    }
    console_puts("VM FIFO: resident="); console_number((long)resident);
    console_puts(" limit="); console_number((long)frame_limit);
    console_puts(" swap_slots="); console_number(swap_used()); console_putc('/'); console_number(swap_total());
    console_puts("\nfaults="); console_number((long)stats.faults);
    console_puts(" kernel_loads="); console_number((long)stats.soft_loads);
    console_puts(" image="); console_number((long)stats.images);
    console_puts(" zero="); console_number((long)stats.zeroes);
    console_puts("\nswap_in="); console_number((long)stats.ins);
    console_puts(" swap_out="); console_number((long)stats.outs);
    console_puts(" clean_drop="); console_number((long)stats.drops);
    console_puts(" swap_fork_copies="); console_number((long)stats.copies);
    console_puts("\ninvalid_faults="); console_number((long)stats.invalid);
    console_puts(" io_errors="); console_number((long)stats.io_errors); console_putc('\n');
    return 0;
}

void vm_print_space(uintptr_t root) {
    VmSpace *space = find_space(root);
    if (space == 0) return;
    size_t in_ram = 0, on_disk = 0;
    for (size_t i = 0; i < SPACE_PAGES; ++i) {
        in_ram += space->pages[i].physical != 0;
        on_disk += !space->pages[i].physical && space->pages[i].slot != 0;
    }
    console_puts("VM resident="); console_number((long)in_ram);
    console_puts(" swapped="); console_number((long)on_disk);
    console_puts(" heap_bytes="); console_number((long)(space->heap_end - USER_HEAP_BASE));
    console_puts(" faults="); console_number((long)space->faults); console_putc('\n');
}
