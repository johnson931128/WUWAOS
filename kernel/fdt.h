#ifndef WUWAOS_FDT_H
#define WUWAOS_FDT_H

#include <stddef.h>
#include <stdint.h>

#define FDT_NODE_LIMIT 128U

typedef struct {
    uint32_t properties;
    int parent;
    int has_children;
} FdtNode;

/* A bounded, allocation-free view. Property pointers borrow the boot DTB. */
typedef struct {
    const uint8_t *structure, *strings;
    uint32_t structure_size, strings_size;
    unsigned int count;
    FdtNode nodes[FDT_NODE_LIMIT];
} Fdt;

int fdt_open(Fdt *tree, const void *blob);
const uint8_t *fdt_property(const Fdt *tree, unsigned int node, const char *name, uint32_t *length);
int fdt_u32(const Fdt *tree, unsigned int node, const char *name, uint32_t *value);
int fdt_compatible(const Fdt *tree, unsigned int node, const char *compatible);
int fdt_reg(const Fdt *tree, unsigned int node, uint64_t *address, uint64_t *size);
int fdt_phandle(const Fdt *tree, uint32_t phandle);
uint32_t fdt_be32(const uint8_t *bytes);

#endif
