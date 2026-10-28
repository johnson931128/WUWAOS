#include "fs.h"
#include "block.h"
#include "console.h"
#include <stdint.h>

#define FS_MAGIC "WUFS0001"
#define FS_SECTOR_SIZE 512U
#define FS_INODE_COUNT 128U
#define FS_INODE_SIZE 80U
#define FS_DIRECT 8U
#define FS_INDIRECT 8U
#define FS_POINTERS_PER_SECTOR (FS_SECTOR_SIZE / sizeof(uint32_t))
#define FS_NAME_SIZE 24U
#define FS_TYPE_DIRECTORY 1U
#define FS_TYPE_FILE 2U
#define FS_ROOT_INODE 0U

typedef struct {
    uint32_t sector_count;
    uint32_t inode_count;
    uint32_t inode_bitmap_sector;
    uint32_t block_bitmap_sector;
    uint32_t block_bitmap_length;
    uint32_t inode_table_sector;
    uint32_t inode_table_length;
    uint32_t data_sector;
    uint32_t root_inode;
} Superblock;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t direct[FS_DIRECT];
    uint32_t indirect[FS_INDIRECT];
    uint32_t reserved[2];
} Inode;

typedef struct {
    char name[FS_NAME_SIZE];
    uint32_t inode;
} DirectoryEntry;

_Static_assert(sizeof(Inode) == FS_INODE_SIZE, "WUFS inode size");
_Static_assert(sizeof(DirectoryEntry) == FS_NAME_SIZE + sizeof(uint32_t), "WUFS directory entry size");

static Superblock superblock;
static int mounted;

static int read_sector(uint32_t sector, void *buffer) {
    return block_read(sector, buffer);
}

static int write_sector(uint32_t sector, const void *buffer) {
    return block_write(sector, buffer);
}

static int bitmap_get(uint32_t sector, uint32_t bit) {
    uint8_t bytes[FS_SECTOR_SIZE];
    const uint32_t bitmap_sector = sector + bit / (FS_SECTOR_SIZE * 8U);
    if (read_sector(bitmap_sector, bytes) < 0) {
        return -1;
    }
    return (bytes[(bit / 8U) % FS_SECTOR_SIZE] >> (bit % 8U)) & 1U;
}

static int bitmap_set(uint32_t sector, uint32_t bit, int value) {
    uint8_t bytes[FS_SECTOR_SIZE];
    const uint32_t bitmap_sector = sector + bit / (FS_SECTOR_SIZE * 8U);
    if (read_sector(bitmap_sector, bytes) < 0) {
        return -1;
    }
    const uint32_t offset = (bit / 8U) % FS_SECTOR_SIZE;
    const uint8_t mask = (uint8_t)(1U << (bit % 8U));
    bytes[offset] = value ? (uint8_t)(bytes[offset] | mask) : (uint8_t)(bytes[offset] & (uint8_t)~mask);
    return write_sector(bitmap_sector, bytes);
}

static int inode_read(uint32_t number, Inode *inode) {
    if (number >= superblock.inode_count) {
        return -1;
    }
    uint8_t sector[FS_SECTOR_SIZE];
    const uint32_t byte = number * FS_INODE_SIZE;
    size_t copied = 0;
    /* Packed 80-byte inodes can span two 512-byte sectors. */
    while (copied < sizeof(*inode)) {
        const uint32_t position = byte + (uint32_t)copied;
        const uint32_t lba = superblock.inode_table_sector + position / FS_SECTOR_SIZE;
        const uint32_t offset = position % FS_SECTOR_SIZE;
        if (read_sector(lba, sector) < 0) return -1;
        size_t amount = FS_SECTOR_SIZE - offset;
        if (amount > sizeof(*inode) - copied) amount = sizeof(*inode) - copied;
        for (size_t i = 0; i < amount; ++i) {
            ((uint8_t *)inode)[copied + i] = sector[offset + i];
        }
        copied += amount;
    }
    return 0;
}

static int inode_write(uint32_t number, const Inode *inode) {
    if (number >= superblock.inode_count) {
        return -1;
    }
    uint8_t sector[FS_SECTOR_SIZE];
    const uint32_t byte = number * FS_INODE_SIZE;
    size_t copied = 0;
    while (copied < sizeof(*inode)) {
        const uint32_t position = byte + (uint32_t)copied;
        const uint32_t lba = superblock.inode_table_sector + position / FS_SECTOR_SIZE;
        const uint32_t offset = position % FS_SECTOR_SIZE;
        /* Read-modify-write preserves adjacent inodes in each sector. */
        if (read_sector(lba, sector) < 0) return -1;
        size_t amount = FS_SECTOR_SIZE - offset;
        if (amount > sizeof(*inode) - copied) amount = sizeof(*inode) - copied;
        for (size_t i = 0; i < amount; ++i) {
            sector[offset + i] = ((const uint8_t *)inode)[copied + i];
        }
        if (write_sector(lba, sector) < 0) return -1;
        copied += amount;
    }
    return 0;
}

static long inode_allocate(uint32_t type) {
    for (uint32_t i = 0; i < superblock.inode_count; ++i) {
        const int used = bitmap_get(superblock.inode_bitmap_sector, i);
        if (used == 0) {
            Inode inode = { .type = type };
            if (bitmap_set(superblock.inode_bitmap_sector, i, 1) < 0 || inode_write(i, &inode) < 0) {
                return -1;
            }
            return (long)i;
        }
    }
    return -1;
}

static long block_allocate(void) {
    for (uint32_t sector = superblock.data_sector; sector < superblock.sector_count; ++sector) {
        const int used = bitmap_get(superblock.block_bitmap_sector, sector);
        if (used == 0 && bitmap_set(superblock.block_bitmap_sector, sector, 1) == 0) {
            uint8_t zero[FS_SECTOR_SIZE] = {0};
            if (write_sector(sector, zero) == 0) {
                return (long)sector;
            }
        }
    }
    return -1;
}

static int inode_block(const Inode *inode, uint32_t index, int allocate, uint32_t *result) {
    if (index < FS_DIRECT) {
        uint32_t *entry = (uint32_t *)&inode->direct[index];
        if (*entry == 0 && allocate) {
            const long block = block_allocate();
            if (block < 0) return -1;
            *entry = (uint32_t)block;
        }
        *result = *entry;
        return 0;
    }
    index -= FS_DIRECT;
    const uint32_t indirect_index = index / FS_POINTERS_PER_SECTOR;
    const uint32_t pointer_index = index % FS_POINTERS_PER_SECTOR;
    if (indirect_index >= FS_INDIRECT) return -1;
    uint32_t indirect_sector = inode->indirect[indirect_index];
    if (indirect_sector == 0 && allocate) {
        const long block = block_allocate();
        if (block < 0) return -1;
        ((Inode *)inode)->indirect[indirect_index] = (uint32_t)block;
        indirect_sector = (uint32_t)block;
    }
    if (indirect_sector == 0) {
        *result = 0;
        return 0;
    }
    uint32_t pointers[FS_POINTERS_PER_SECTOR];
    if (read_sector(indirect_sector, pointers) < 0) return -1;
    if (pointers[pointer_index] == 0 && allocate) {
        const long block = block_allocate();
        if (block < 0) return -1;
        pointers[pointer_index] = (uint32_t)block;
        if (write_sector(indirect_sector, pointers) < 0) return -1;
    }
    *result = pointers[pointer_index];
    return 0;
}

static int dir_entry_read(const Inode *directory, uint32_t index, DirectoryEntry *entry) {
    const uint32_t byte = index * (uint32_t)sizeof(*entry);
    size_t copied = 0;
    while (copied < sizeof(*entry)) {
        const uint32_t position = byte + (uint32_t)copied;
        const uint32_t offset = position % FS_SECTOR_SIZE;
        uint32_t sector;
        uint8_t data[FS_SECTOR_SIZE];
        if (inode_block(directory, position / FS_SECTOR_SIZE, 0, &sector) < 0 ||
            sector == 0 || read_sector(sector, data) < 0) return -1;
        size_t amount = FS_SECTOR_SIZE - offset;
        if (amount > sizeof(*entry) - copied) amount = sizeof(*entry) - copied;
        for (size_t i = 0; i < amount; ++i) ((uint8_t *)entry)[copied + i] = data[offset + i];
        copied += amount;
    }
    return 0;
}

static int dir_entry_write(Inode *directory, uint32_t index, const DirectoryEntry *entry) {
    const uint32_t byte = index * (uint32_t)sizeof(*entry);
    size_t copied = 0;
    while (copied < sizeof(*entry)) {
        const uint32_t position = byte + (uint32_t)copied;
        const uint32_t offset = position % FS_SECTOR_SIZE;
        uint32_t sector;
        uint8_t data[FS_SECTOR_SIZE];
        if (inode_block(directory, position / FS_SECTOR_SIZE, 1, &sector) < 0 ||
            sector == 0 || read_sector(sector, data) < 0) return -1;
        size_t amount = FS_SECTOR_SIZE - offset;
        if (amount > sizeof(*entry) - copied) amount = sizeof(*entry) - copied;
        for (size_t i = 0; i < amount; ++i) data[offset + i] = ((const uint8_t *)entry)[copied + i];
        if (write_sector(sector, data) < 0) return -1;
        copied += amount;
    }
    directory->size = (index + 1U) * (uint32_t)sizeof(*entry);
    return 0;
}

static int dir_find(const Inode *directory, const char *name, uint32_t *inode_number) {
    if (directory->type != FS_TYPE_DIRECTORY) return -2;
    const uint32_t count = directory->size / (uint32_t)sizeof(DirectoryEntry);
    for (uint32_t i = 0; i < count; ++i) {
        DirectoryEntry entry;
        if (dir_entry_read(directory, i, &entry) < 0) return -1;
        uint32_t at = 0;
        while (at < FS_NAME_SIZE && entry.name[at] == name[at] && name[at] != 0) ++at;
        if (at < FS_NAME_SIZE && entry.name[at] == 0 && name[at] == 0) {
            *inode_number = entry.inode;
            return 0;
        }
    }
    return -1;
}

static int path_lookup(const char *path, uint32_t *number) {
    if (!mounted || path == 0 || path[0] != '/') return -22;
    uint32_t current = superblock.root_inode;
    const char *cursor = path;
    while (*cursor == '/') ++cursor;
    while (*cursor) {
        char name[FS_NAME_SIZE] = {0};
        uint32_t length = 0;
        while (*cursor && *cursor != '/') {
            if (length + 1 >= FS_NAME_SIZE) return -36;
            name[length++] = *cursor++;
        }
        while (*cursor == '/') ++cursor;
        Inode directory;
        if (inode_read(current, &directory) < 0 || directory.type != FS_TYPE_DIRECTORY) return -20;
        if (dir_find(&directory, name, &current) < 0) return -2;
    }
    *number = current;
    return 0;
}

static int parent_lookup(const char *path, uint32_t *parent, char *name) {
    if (path == 0 || path[0] != '/') return -22;
    size_t length = 0;
    while (path[length]) ++length;
    if (length < 2 || length >= 128 || path[length - 1] == '/') return -22;
    size_t split = length;
    while (split > 0 && path[split - 1] != '/') --split;
    const size_t name_length = length - split;
    if (name_length == 0 || name_length >= FS_NAME_SIZE) return -36;
    for (size_t i = 0; i < name_length; ++i) name[i] = path[split + i];
    name[name_length] = 0;
    if (split == 1) {
        *parent = superblock.root_inode;
        return 0;
    }
    char parent_path[128];
    for (size_t i = 0; i < split - 1; ++i) parent_path[i] = path[i];
    parent_path[split - 1] = 0;
    return path_lookup(parent_path, parent);
}

static int directory_add(uint32_t parent_number, const char *name, uint32_t child_number) {
    Inode parent;
    if (inode_read(parent_number, &parent) < 0 || parent.type != FS_TYPE_DIRECTORY) return -20;
    uint32_t found;
    if (dir_find(&parent, name, &found) == 0) return -17;
    DirectoryEntry entry = {0};
    size_t i = 0;
    while (i + 1 < FS_NAME_SIZE && name[i]) {
        entry.name[i] = name[i];
        ++i;
    }
    entry.inode = child_number;
    const uint32_t index = parent.size / (uint32_t)sizeof(entry);
    if (dir_entry_write(&parent, index, &entry) < 0 || inode_write(parent_number, &parent) < 0) return -5;
    return 0;
}

static long create_node(const char *path, uint32_t type) {
    uint32_t parent_number;
    char name[FS_NAME_SIZE] = {0};
    const int parent_result = parent_lookup(path, &parent_number, name);
    if (parent_result < 0) return parent_result;
    Inode parent;
    if (inode_read(parent_number, &parent) < 0 || parent.type != FS_TYPE_DIRECTORY) return -20;
    uint32_t existing;
    if (dir_find(&parent, name, &existing) == 0) return -17;
    const long node_number = inode_allocate(type);
    if (node_number < 0) return -28;
    if (type == FS_TYPE_DIRECTORY) {
        Inode node;
        if (inode_read((uint32_t)node_number, &node) < 0) return -5;
        DirectoryEntry dot = { .name = ".", .inode = (uint32_t)node_number };
        DirectoryEntry dotdot = { .name = "..", .inode = parent_number };
        if (dir_entry_write(&node, 0, &dot) < 0 || dir_entry_write(&node, 1, &dotdot) < 0 ||
            inode_write((uint32_t)node_number, &node) < 0) return -5;
    }
    const int added = directory_add(parent_number, name, (uint32_t)node_number);
    if (added < 0) return added;
    return node_number;
}

static int format_filesystem(uint32_t sectors) {
    const uint32_t block_bitmap_length = (sectors + FS_SECTOR_SIZE * 8U - 1U) / (FS_SECTOR_SIZE * 8U);
    superblock = (Superblock){
        .sector_count = sectors,
        .inode_count = FS_INODE_COUNT,
        .inode_bitmap_sector = 1,
        .block_bitmap_sector = 2,
        .block_bitmap_length = block_bitmap_length,
        .inode_table_sector = 2 + block_bitmap_length,
        .inode_table_length = (FS_INODE_COUNT * FS_INODE_SIZE) / FS_SECTOR_SIZE,
        .data_sector = 2 + block_bitmap_length + (FS_INODE_COUNT * FS_INODE_SIZE) / FS_SECTOR_SIZE,
        .root_inode = FS_ROOT_INODE
    };
    uint8_t sector[FS_SECTOR_SIZE] = {0};
    if (write_sector(1, sector) < 0) return -1;
    for (uint32_t i = 0; i < superblock.block_bitmap_length; ++i) {
        if (write_sector(superblock.block_bitmap_sector + i, sector) < 0) return -1;
    }
    for (uint32_t lba = 0; lba < superblock.data_sector; ++lba) {
        if (bitmap_set(superblock.block_bitmap_sector, lba, 1) < 0) return -1;
    }
    if (bitmap_set(superblock.inode_bitmap_sector, FS_ROOT_INODE, 1) < 0) return -1;
    for (uint32_t i = 0; i < superblock.inode_table_length; ++i) {
        if (write_sector(superblock.inode_table_sector + i, sector) < 0) return -1;
    }
    for (uint32_t i = 0; i < sizeof(FS_MAGIC) - 1; ++i) sector[i] = (uint8_t)FS_MAGIC[i];
    ((uint32_t *)sector)[2] = superblock.sector_count;
    ((uint32_t *)sector)[3] = superblock.inode_count;
    ((uint32_t *)sector)[4] = superblock.inode_bitmap_sector;
    ((uint32_t *)sector)[5] = superblock.block_bitmap_sector;
    ((uint32_t *)sector)[6] = superblock.block_bitmap_length;
    ((uint32_t *)sector)[7] = superblock.inode_table_sector;
    ((uint32_t *)sector)[8] = superblock.inode_table_length;
    ((uint32_t *)sector)[9] = superblock.data_sector;
    ((uint32_t *)sector)[10] = superblock.root_inode;
    if (write_sector(0, sector) < 0) return -1;
    Inode root = { .type = FS_TYPE_DIRECTORY };
    if (inode_write(FS_ROOT_INODE, &root) < 0) return -1;
    mounted = 1;
    DirectoryEntry dot = { .name = ".", .inode = FS_ROOT_INODE };
    DirectoryEntry dotdot = { .name = "..", .inode = FS_ROOT_INODE };
    if (dir_entry_write(&root, 0, &dot) < 0 || dir_entry_write(&root, 1, &dotdot) < 0 ||
        inode_write(FS_ROOT_INODE, &root) < 0) return -1;
    return 0;
}

int fs_init(void) {
    uint8_t sector[FS_SECTOR_SIZE] = {0};
    const uint32_t sectors = block_sector_count();
    if (sectors == 0 || read_sector(0, sector) < 0) return -1;
    int valid = 1;
    for (uint32_t i = 0; i < sizeof(FS_MAGIC) - 1; ++i) valid &= sector[i] == (uint8_t)FS_MAGIC[i];
    if (!valid) {
        console_puts("[fs] Initializing blank disk as WUFS\n");
        if (format_filesystem(sectors) < 0) return -1;
        mounted = 1;
        (void)fs_mkdir("/bin");
        (void)fs_mkdir("/boot");
        (void)fs_mkdir("/dev");
        (void)fs_mkdir("/home");
        (void)fs_mkdir("/home/user");
        console_puts("[fs] Created /bin /boot /dev /home/user\n");
    } else {
        const uint32_t *fields = (const uint32_t *)sector;
        superblock.sector_count = fields[2];
        superblock.inode_count = fields[3];
        superblock.inode_bitmap_sector = fields[4];
        superblock.block_bitmap_sector = fields[5];
        superblock.block_bitmap_length = fields[6];
        superblock.inode_table_sector = fields[7];
        superblock.inode_table_length = fields[8];
        superblock.data_sector = fields[9];
        superblock.root_inode = fields[10];
        if (superblock.sector_count > sectors || superblock.inode_count != FS_INODE_COUNT ||
            superblock.data_sector >= superblock.sector_count || superblock.root_inode >= superblock.inode_count) return -1;
        mounted = 1;
        console_puts("[fs] Mounted existing WUFS volume\n");
    }
    return 0;
}

long fs_list(const char *path, char *output, size_t capacity) {
    uint32_t number;
    const int found = path_lookup(path, &number);
    if (found < 0) return found;
    Inode directory;
    if (inode_read(number, &directory) < 0 || directory.type != FS_TYPE_DIRECTORY) return -20;
    size_t used = 0;
    const uint32_t count = directory.size / (uint32_t)sizeof(DirectoryEntry);
    for (uint32_t i = 0; i < count; ++i) {
        DirectoryEntry entry;
        Inode child;
        if (dir_entry_read(&directory, i, &entry) < 0 || inode_read(entry.inode, &child) < 0) return -5;
        if (entry.name[0] == '.' && (entry.name[1] == 0 || entry.name[1] == '.')) continue;
        size_t length = 0;
        while (length < FS_NAME_SIZE && entry.name[length]) ++length;
        const size_t extra = child.type == FS_TYPE_DIRECTORY ? 2U : 1U;
        if (used + length + extra > capacity) return -28;
        for (size_t j = 0; j < length; ++j) output[used++] = entry.name[j];
        if (child.type == FS_TYPE_DIRECTORY) output[used++] = '/';
        output[used++] = '\n';
    }
    return (long)used;
}

long fs_mkdir(const char *path) {
    const long result = create_node(path, FS_TYPE_DIRECTORY);
    return result < 0 ? result : 0;
}

long fs_touch(const char *path) {
    uint32_t number;
    const int result = path_lookup(path, &number);
    if (result == 0) {
        Inode inode;
        if (inode_read(number, &inode) < 0) return -5;
        return inode.type == FS_TYPE_FILE ? 0 : -21;
    }
    if (result != -2) return result;
    const long created = create_node(path, FS_TYPE_FILE);
    return created < 0 ? created : 0;
}

static int inode_free_blocks(Inode *inode) {
    uint8_t zero[FS_SECTOR_SIZE] = {0};
    for (uint32_t i = 0; i < FS_DIRECT; ++i) {
        if (inode->direct[i] != 0) {
            if (bitmap_set(superblock.block_bitmap_sector, inode->direct[i], 0) < 0) return -1;
            inode->direct[i] = 0;
        }
    }
    for (uint32_t i = 0; i < FS_INDIRECT; ++i) {
        if (inode->indirect[i] == 0) continue;
        uint32_t pointers[FS_POINTERS_PER_SECTOR];
        if (read_sector(inode->indirect[i], pointers) < 0) return -1;
        for (uint32_t j = 0; j < FS_POINTERS_PER_SECTOR; ++j) {
            if (pointers[j] != 0 && bitmap_set(superblock.block_bitmap_sector, pointers[j], 0) < 0) return -1;
        }
        if (bitmap_set(superblock.block_bitmap_sector, inode->indirect[i], 0) < 0 || write_sector(inode->indirect[i], zero) < 0) return -1;
        inode->indirect[i] = 0;
    }
    inode->size = 0;
    return 0;
}

/* Distinguish a missing entry from an I/O error before destructive operations. */
static int directory_find_entry(const Inode *directory, const char *name,
                                uint32_t *index, DirectoryEntry *result) {
    if (directory->type != FS_TYPE_DIRECTORY) return -20;
    const uint32_t count = directory->size / (uint32_t)sizeof(DirectoryEntry);
    for (uint32_t i = 0; i < count; ++i) {
        DirectoryEntry entry;
        if (dir_entry_read(directory, i, &entry) < 0) return -5;
        size_t j = 0;
        while (j < FS_NAME_SIZE && entry.name[j] == name[j] && name[j]) ++j;
        if (j < FS_NAME_SIZE && entry.name[j] == 0 && name[j] == 0) {
            *index = i;
            *result = entry;
            return 0;
        }
    }
    return -2;
}

static int special_name(const char *name) {
    return name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0));
}

/* Only call on an inode which is no longer reachable from a directory. */
static int inode_release(uint32_t number, Inode *inode) {
    if (inode_free_blocks(inode) < 0) return -5;
    *inode = (Inode){0};
    if (inode_write(number, inode) < 0) return -5;
    if (bitmap_set(superblock.inode_bitmap_sector, number, 0) < 0) return -5;
    return 0;
}

long fs_remove(const char *path) {
    if (!mounted) return -22;
    uint32_t parent_number, index;
    char name[FS_NAME_SIZE] = {0};
    int error = parent_lookup(path, &parent_number, name);
    if (error < 0) return error;
    if (special_name(name)) return -22;
    Inode parent, inode;
    DirectoryEntry entry;
    if (inode_read(parent_number, &parent) < 0) return -5;
    error = directory_find_entry(&parent, name, &index, &entry);
    if (error < 0) return error;
    if (inode_read(entry.inode, &inode) < 0) return -5;
    if (inode.type != FS_TYPE_FILE) return -21;

    const uint32_t count = parent.size / (uint32_t)sizeof(DirectoryEntry);
    const uint32_t new_size = parent.size - (uint32_t)sizeof(DirectoryEntry);
    if (index != count - 1) {
        DirectoryEntry last;
        if (dir_entry_read(&parent, count - 1, &last) < 0 ||
            dir_entry_write(&parent, index, &last) < 0) return -5;
    }
    parent.size = new_size;
    if (inode_write(parent_number, &parent) < 0) return -5;
    return inode_release(entry.inode, &inode);
}

long fs_copy(const char *source, const char *destination) {
    if (!mounted) return -22;
    uint32_t source_number, parent_number, index = 0;
    int error = path_lookup(source, &source_number);
    if (error < 0) return error;
    Inode original;
    if (inode_read(source_number, &original) < 0) return -5;
    if (original.type != FS_TYPE_FILE) return -21;

    char name[FS_NAME_SIZE] = {0};
    error = parent_lookup(destination, &parent_number, name);
    if (error < 0) return error;
    if (special_name(name)) return -21;
    Inode parent, old = {0};
    DirectoryEntry entry;
    if (inode_read(parent_number, &parent) < 0) return -5;
    error = directory_find_entry(&parent, name, &index, &entry);
    const int exists = error == 0;
    if (error != 0 && error != -2) return error;
    if (exists) {
        if (entry.inode == source_number) return -22;
        if (inode_read(entry.inode, &old) < 0) return -5;
        if (old.type != FS_TYPE_FILE) return -21;
    }

    const long allocated = inode_allocate(FS_TYPE_FILE);
    if (allocated < 0) return -28;
    Inode copy = { .type = FS_TYPE_FILE };
    for (uint32_t position = 0; position < original.size; position += FS_SECTOR_SIZE) {
        uint32_t from, to;
        uint8_t data[FS_SECTOR_SIZE];
        const uint32_t block = position / FS_SECTOR_SIZE;
        if (inode_block(&original, block, 0, &from) < 0 || from == 0 ||
            read_sector(from, data) < 0) { error = -5; goto discard; }
        if (inode_block(&copy, block, 1, &to) < 0 || to == 0) { error = -28; goto discard; }
        if (write_sector(to, data) < 0) { error = -5; goto discard; }
    }
    copy.size = original.size;
    if (inode_write((uint32_t)allocated, &copy) < 0) { error = -5; goto discard; }

    /* Do not free the new inode on a failed publication: it may already be linked. */
    if (exists) {
        const uint32_t old_number = entry.inode;
        entry.inode = (uint32_t)allocated;
        if (dir_entry_write(&parent, index, &entry) < 0) return -5;
        if (inode_release(old_number, &old) < 0) return -5;
    } else {
        error = directory_add(parent_number, name, (uint32_t)allocated);
        if (error < 0) return error;
    }
    return (long)copy.size;

discard:
    if (inode_release((uint32_t)allocated, &copy) < 0) return -5;
    return error;
}

long fs_read(const char *path, size_t offset, void *buffer, size_t length) {
    uint32_t number;
    const int found = path_lookup(path, &number);
    if (found < 0) return found;
    Inode inode;
    if (inode_read(number, &inode) < 0) return -5;
    if (inode.type != FS_TYPE_FILE) return -21;
    if (offset >= inode.size) return 0;
    if (length > inode.size - offset) length = inode.size - offset;
    size_t copied = 0;
    while (copied < length) {
        const size_t position = offset + copied;
        uint32_t sector;
        uint8_t data[FS_SECTOR_SIZE];
        if (inode_block(&inode, (uint32_t)(position / FS_SECTOR_SIZE), 0, &sector) < 0 || sector == 0 || read_sector(sector, data) < 0) return -5;
        const size_t in_sector = position % FS_SECTOR_SIZE;
        size_t amount = FS_SECTOR_SIZE - in_sector;
        if (amount > length - copied) amount = length - copied;
        for (size_t i = 0; i < amount; ++i) ((uint8_t *)buffer)[copied + i] = data[in_sector + i];
        copied += amount;
    }
    return (long)copied;
}

long fs_write(const char *path, const void *buffer, size_t length) {
    if (length > (FS_DIRECT + FS_INDIRECT * FS_POINTERS_PER_SECTOR) * FS_SECTOR_SIZE) return -27;
    uint32_t number;
    int found = path_lookup(path, &number);
    if (found == -2) {
        const long created = create_node(path, FS_TYPE_FILE);
        if (created < 0) return created;
        number = (uint32_t)created;
    } else if (found < 0) {
        return found;
    }
    Inode inode;
    if (inode_read(number, &inode) < 0 || inode.type != FS_TYPE_FILE) return -21;
    if (inode_free_blocks(&inode) < 0) return -5;
    size_t copied = 0;
    while (copied < length) {
        uint32_t sector;
        uint8_t data[FS_SECTOR_SIZE] = {0};
        if (inode_block(&inode, (uint32_t)(copied / FS_SECTOR_SIZE), 1, &sector) < 0 || sector == 0) return -28;
        size_t amount = length - copied;
        if (amount > FS_SECTOR_SIZE) amount = FS_SECTOR_SIZE;
        for (size_t i = 0; i < amount; ++i) data[i] = ((const uint8_t *)buffer)[copied + i];
        if (write_sector(sector, data) < 0) return -5;
        copied += amount;
    }
    inode.size = (uint32_t)length;
    if (inode_write(number, &inode) < 0) return -5;
    return (long)length;
}
