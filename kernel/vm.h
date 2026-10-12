#ifndef WUWAOS_VM_H
#define WUWAOS_VM_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096UL
#define USER_ENTRY 0x400000UL
#define USER_DATA 0x401000UL
#define USER_STACK 0x800000UL
#define USER_STACK_TOP (USER_STACK + PAGE_SIZE)

uintptr_t vm_init(void);
void vm_enable(uintptr_t root);
int user_readable(uintptr_t address, size_t length);
unsigned char user_read_byte(uintptr_t address);

#endif
