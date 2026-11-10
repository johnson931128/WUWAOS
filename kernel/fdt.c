#include "fdt.h"

#define FDT_MAGIC 0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U
#define FDT_DEPTH_LIMIT 32U
#define FDT_SIZE_LIMIT (1024U * 1024U)

uint32_t fdt_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static int equal(const char *left, const char *right) {
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}

static int terminated(const uint8_t *bytes, uint32_t size, uint32_t at) {
    while (at < size) if (bytes[at++] == 0) return 1;
    return 0;
}

int fdt_open(Fdt *tree, const void *blob) {
    *tree = (Fdt){0};
    if (!blob) return -1;
    const uint8_t *bytes = blob;
    if (fdt_be32(bytes) != FDT_MAGIC) return -1;
    const uint32_t total = fdt_be32(bytes + 4);
    if (total < 40U || total > FDT_SIZE_LIMIT || fdt_be32(bytes + 20) < 17U ||
        fdt_be32(bytes + 24) > 17U) return -1;
    const uint32_t structure = fdt_be32(bytes + 8), strings = fdt_be32(bytes + 12);
    tree->structure_size = fdt_be32(bytes + 36);
    tree->strings_size = fdt_be32(bytes + 32);
    if ((structure & 3U) || structure < 40U || strings < 40U ||
        structure > total || tree->structure_size > total - structure ||
        strings > total || tree->strings_size > total - strings) return -1;
    if (structure < strings + tree->strings_size && strings < structure + tree->structure_size) return -1;
    tree->structure = bytes + structure;
    tree->strings = bytes + strings;
    unsigned int stack[FDT_DEPTH_LIMIT], depth = 0;
    uint32_t at = 0;
    while (at <= tree->structure_size && tree->structure_size - at >= 4U) {
        const uint32_t token = fdt_be32(tree->structure + at);
        at += 4U;
        if (token == FDT_BEGIN_NODE) {
            if (depth == FDT_DEPTH_LIMIT || tree->count == FDT_NODE_LIMIT ||
                (!depth && tree->count)) return -1;
            if (!terminated(tree->structure, tree->structure_size, at)) return -1;
            while (tree->structure[at]) ++at;
            at = (at + 4U) & ~3U;
            if (at > tree->structure_size) return -1;
            const unsigned int index = tree->count++;
            tree->nodes[index] = (FdtNode){ .properties = at,
                .parent = depth ? (int)stack[depth - 1U] : -1 };
            if (depth) tree->nodes[stack[depth - 1U]].has_children = 1;
            stack[depth++] = index;
        } else if (token == FDT_END_NODE) {
            if (!depth) return -1;
            --depth;
        } else if (token == FDT_PROP) {
            if (!depth || tree->nodes[stack[depth - 1U]].has_children ||
                at > tree->structure_size || tree->structure_size - at < 8U) return -1;
            const uint32_t length = fdt_be32(tree->structure + at);
            const uint32_t name = fdt_be32(tree->structure + at + 4U);
            at += 8U;
            if (!terminated(tree->strings, tree->strings_size, name) ||
                length > tree->structure_size - at) return -1;
            at = (at + length + 3U) & ~3U;
            if (at > tree->structure_size) return -1;
        } else if (token == FDT_END) {
            return tree->count && !depth ? 0 : -1;
        } else if (token != FDT_NOP) return -1;
    }
    return -1;
}

const uint8_t *fdt_property(const Fdt *tree, unsigned int node, const char *name, uint32_t *length) {
    if (length) *length = 0;
    if (node >= tree->count) return 0;
    uint32_t at = tree->nodes[node].properties;
    /* fdt_open validates property bounds, names, and the properties-before-children rule. */
    while (at < tree->structure_size) {
        const uint32_t token = fdt_be32(tree->structure + at);
        at += 4U;
        if (token == FDT_NOP) continue;
        if (token != FDT_PROP) return 0;
        const uint32_t size = fdt_be32(tree->structure + at);
        const uint32_t name_offset = fdt_be32(tree->structure + at + 4U);
        at += 8U;
        if (equal((const char *)(tree->strings + name_offset), name)) {
            if (length) *length = size;
            return tree->structure + at;
        }
        at = (at + size + 3U) & ~3U;
    }
    return 0;
}

int fdt_u32(const Fdt *tree, unsigned int node, const char *name, uint32_t *value) {
    uint32_t length;
    const uint8_t *bytes = fdt_property(tree, node, name, &length);
    if (!bytes || length != 4U) return -1;
    *value = fdt_be32(bytes);
    return 0;
}

int fdt_compatible(const Fdt *tree, unsigned int node, const char *compatible) {
    uint32_t length;
    const uint8_t *bytes = fdt_property(tree, node, "compatible", &length);
    if (!bytes || !length || bytes[length - 1U] != 0) return 0;
    for (uint32_t at = 0; at < length;) {
        if (equal((const char *)(bytes + at), compatible)) return 1;
        while (bytes[at]) ++at;
        ++at;
    }
    return 0;
}

int fdt_reg(const Fdt *tree, unsigned int node, uint64_t *address, uint64_t *size) {
    if (node >= tree->count || tree->nodes[node].parent < 0) return -1;
    const unsigned int parent = (unsigned int)tree->nodes[node].parent;
    uint32_t address_cells = 2U, size_cells = 1U, length;
    const uint8_t *cells = fdt_property(tree, parent, "#address-cells", &length);
    if (cells) {
        if (length != 4U) return -1;
        address_cells = fdt_be32(cells);
    }
    cells = fdt_property(tree, parent, "#size-cells", &length);
    if (cells) {
        if (length != 4U) return -1;
        size_cells = fdt_be32(cells);
    }
    if (!address_cells || address_cells > 2U || size_cells > 2U) return -1;
    const uint8_t *reg = fdt_property(tree, node, "reg", &length);
    if (!reg || length < (address_cells + size_cells) * 4U) return -1;
    *address = *size = 0;
    for (uint32_t i = 0; i < address_cells; ++i) *address = (*address << 32) | fdt_be32(reg + 4U * i);
    for (uint32_t i = 0; i < size_cells; ++i) *size = (*size << 32) | fdt_be32(reg + 4U * (address_cells + i));
    /* QEMU virt uses identity buses. Translated ranges need a future bus mapper. */
    for (int ancestor = (int)parent; ancestor >= 0; ancestor = tree->nodes[ancestor].parent) {
        const uint8_t *ranges = fdt_property(tree, (unsigned int)ancestor, "ranges", &length);
        if (ranges && length) return -1;
    }
    return 0;
}

int fdt_phandle(const Fdt *tree, uint32_t phandle) {
    if (!phandle || phandle == UINT32_MAX) return -1;
    int found = -1;
    for (unsigned int i = 0; i < tree->count; ++i) {
        uint32_t value;
        if (fdt_u32(tree, i, "phandle", &value) < 0 &&
            fdt_u32(tree, i, "linux,phandle", &value) < 0) continue;
        if (value == phandle) {
            if (found >= 0) return -1;
            found = (int)i;
        }
    }
    return found;
}
