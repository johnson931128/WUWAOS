#ifndef WUWAOS_VM_H
#define WUWAOS_VM_H

#include <stddef.h>
#include <stdint.h>
#include "page.h"
#include "vm_limits.h"

#define USER_ENTRY 0x400000UL
#define USER_CODE_SIZE (4 * PAGE_SIZE)
#define USER_DATA (USER_ENTRY + USER_CODE_SIZE)
#define USER_STACK 0x800000UL
#define USER_STACK_TOP (USER_STACK + PAGE_SIZE)

void vm_init(const void *fdt);
void vm_map_mmio(uintptr_t base);
uintptr_t vm_create(long program);
int vm_clone(uintptr_t destination, uintptr_t source);
void vm_destroy(uintptr_t root);
void vm_use_kernel(void);
void vm_enable(uintptr_t root);
int user_readable(uintptr_t root, uintptr_t address, size_t length);
int user_writable(uintptr_t root, uintptr_t address, size_t length);
int user_read_byte(uintptr_t root, uintptr_t address, unsigned char *result);
int user_write_long(uintptr_t root, uintptr_t address, long value);
int user_write_byte(uintptr_t root, uintptr_t address, unsigned char value);
int user_copy_from(uintptr_t root, void *to, uintptr_t from, size_t length);
int user_copy_to(uintptr_t root, uintptr_t to, const void *from, size_t length);
int vm_handle_fault(uintptr_t root, uintptr_t address, unsigned long cause, uintptr_t pc);
long vm_sbrk(uintptr_t root, long increment);
long vm_control(long limit);
void vm_print_space(uintptr_t root);

#endif
