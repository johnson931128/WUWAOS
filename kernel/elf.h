#ifndef WUWAOS_ELF_H
#define WUWAOS_ELF_H
#include <stdint.h>
long elf_load(const char *path, uintptr_t *root, uintptr_t *entry);
#endif
