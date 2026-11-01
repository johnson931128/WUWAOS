#include "elf.h"
#include "fs.h"
#include "vm.h"
#include "syscall_numbers.h"
#include <stddef.h>
#include <stdint.h>

#define ELF_INVALID (-8L)
#define PT_LOAD 1U
#define PT_DYNAMIC 2U
#define PT_INTERP 3U
#define PT_TLS 7U
#define PT_GNU_STACK 0x6474e551U
#define ELF_READ 4U
#define ELF_WRITE 2U
#define ELF_EXEC 1U
#define MAX_HEADERS 16U

typedef struct {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ElfHeader;
typedef struct {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
} ProgramHeader;
_Static_assert(sizeof(ElfHeader) == 64, "ELF64 header size");
_Static_assert(sizeof(ProgramHeader) == 56, "ELF64 program header size");

long elf_load(const char *path, uintptr_t *root_out, uintptr_t *entry_out) {
    long size = fs_size(path);
    if (size < 0) return size;
    ElfHeader header;
    if (size < (long)sizeof(header)) return ELF_INVALID;
    long n = fs_read(path, 0, &header, sizeof(header));
    if (n < 0) return n;
    if (n != sizeof(header) || header.ident[0] != 0x7f || header.ident[1] != 'E' ||
        header.ident[2] != 'L' || header.ident[3] != 'F' || header.ident[4] != 2 ||
        header.ident[5] != 1 || header.ident[6] != 1 || header.type != 2 ||
        header.machine != 243 || header.version != 1 || (header.flags & ~1U) ||
        header.ehsize != sizeof(header) || header.phentsize != sizeof(ProgramHeader) ||
        !header.phnum || header.phnum > MAX_HEADERS || header.phoff > (uint64_t)size ||
        (uint64_t)header.phnum * sizeof(ProgramHeader) > (uint64_t)size - header.phoff ||
        (header.entry & ((header.flags & 1U) ? 1U : 3U))) return ELF_INVALID;
    ProgramHeader segments[MAX_HEADERS];
    size_t bytes = (size_t)header.phnum * sizeof(ProgramHeader);
    n = fs_read(path, (size_t)header.phoff, segments, bytes);
    if (n < 0) return n;
    if ((size_t)n != bytes) return ELF_INVALID;
    unsigned int used = 0;
    int entry_valid = 0;
    for (size_t i = 0; i < header.phnum; ++i) {
        ProgramHeader *p = &segments[i];
        if (p->type == PT_DYNAMIC || p->type == PT_INTERP || p->type == PT_TLS ||
            (p->type == PT_GNU_STACK && (p->flags & ELF_EXEC))) return ELF_INVALID;
        if (p->type != PT_LOAD) continue;
        if (p->filesz > p->memsz || p->offset > (uint64_t)size || p->filesz > (uint64_t)size - p->offset ||
            (p->align > 1 && ((p->align & (p->align - 1)) || ((p->vaddr - p->offset) & (p->align - 1))))) return ELF_INVALID;
        if (!p->memsz) continue;
        uint64_t limit;
        if (p->flags == (ELF_READ | ELF_EXEC)) limit = USER_DATA;
        else if (p->flags == (ELF_READ | ELF_WRITE)) limit = USER_DATA + PAGE_SIZE;
        else return ELF_INVALID;
        if (p->vaddr < (p->flags & ELF_EXEC ? USER_ENTRY : USER_DATA) || p->vaddr >= limit ||
            p->memsz > limit - p->vaddr) return ELF_INVALID;
        size_t first = (size_t)((p->vaddr - USER_ENTRY) / PAGE_SIZE);
        size_t last = (size_t)((p->vaddr + p->memsz - 1 - USER_ENTRY) / PAGE_SIZE);
        for (size_t j = first; j <= last; ++j) {
            if (used & (1U << j)) return ELF_INVALID;
            used |= 1U << j;
        }
        if ((p->flags & ELF_EXEC) && header.entry >= p->vaddr && header.entry - p->vaddr < p->memsz) entry_valid = 1;
    }
    if (!entry_valid) return ELF_INVALID;
    uintptr_t root = vm_create(PROGRAM_ELF);
    if (!root) return ERR_NO_MEMORY;
    long result = 0;
    for (size_t i = 0; i < header.phnum; ++i) {
        ProgramHeader *p = &segments[i];
        if (p->type != PT_LOAD || !p->memsz) continue;
        result = vm_elf_allow(root, (uintptr_t)p->vaddr, (size_t)p->memsz);
        if (result < 0) goto fail;
        unsigned char block[512];
        for (size_t pos = 0; pos < p->memsz;) {
            size_t amount = (size_t)p->memsz - pos;
            if (amount > sizeof(block)) amount = sizeof(block);
            size_t file_amount = pos < p->filesz ? (size_t)p->filesz - pos : 0;
            if (file_amount > amount) file_amount = amount;
            if (file_amount) {
                n = fs_read(path, (size_t)p->offset + pos, block, file_amount);
                if (n < 0) { result = n; goto fail; }
                if ((size_t)n != file_amount) { result = ELF_INVALID; goto fail; }
            }
            for (size_t j = file_amount; j < amount; ++j) block[j] = 0;
            result = vm_elf_write(root, (uintptr_t)p->vaddr + pos, block, amount);
            if (result < 0) goto fail;
            pos += amount;
        }
    }
    *root_out = root;
    *entry_out = (uintptr_t)header.entry;
    return 0;
fail:
    vm_destroy(root);
    return result;
}
