#include "kernel/fs/fs.h"
#include "kernel/proc/gate.h"
#include "drivers/virtio/block.h"
#include "kernel/io/console.h"
#include "kernel/proc/process.h"
#include "kernel/io/io.h"
#include "kernel/mm/page.h"
#include <stdint.h>

#define FS_MAGIC "WUFS0001"
#define FS_MAGIC_V2 "WUFS0002"
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
#define FS_MAX_SECTORS 32768U
#define FS_JOURNAL_START 1U
#define FS_JOURNAL_HEADER_SECTORS 1U
#define FS_JOURNAL_TARGET_SECTORS 9U
#define FS_JOURNAL_CAPACITY 1152U
#define FS_JOURNAL_DATA_START (FS_JOURNAL_START + FS_JOURNAL_HEADER_SECTORS + FS_JOURNAL_TARGET_SECTORS)
#define FS_JOURNAL_SECTORS (FS_JOURNAL_HEADER_SECTORS + FS_JOURNAL_TARGET_SECTORS + FS_JOURNAL_CAPACITY)

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
static int read_only;
static int offline;
static uint32_t format_version;
static uint32_t transaction_depth;
static uint32_t transaction_count;
static int transaction_error;
static uint32_t transaction_target_storage[FS_JOURNAL_CAPACITY];
#define transaction_targets transaction_target_storage
static uint8_t *transaction_payload;
static KernelGate filesystem_gate;

int fs_scope_enter(void) { return kernel_gate_enter(&filesystem_gate); }

int fs_scope_leave(void) { return kernel_gate_leave(&filesystem_gate); }

static long fs_scope_finish(long result) {
    const int error = fs_scope_leave();
    return error < 0 ? error : result;
}

static int raw_read_sector(uint32_t sector, void *buffer) { return block_read(sector, buffer); }
static int raw_write_sector(uint32_t sector, const void *buffer) { return block_write(sector, buffer); }

static int read_sector(uint32_t sector, void *buffer);
static int write_sector(uint32_t sector, const void *buffer);

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (uint32_t)-(int32_t)(crc & 1U));
    }
    return crc;
}

static int bytes_equal(const void *left, const char *right, size_t length) {
    const uint8_t *a = left;
    for (size_t i = 0; i < length; ++i) if (a[i] != (uint8_t)right[i]) return 0;
    return 1;
}

static int transaction_slot(uint32_t sector) {
    for (uint32_t i = 0; i < transaction_count; ++i)
        if (transaction_targets[i] == sector) return (int)i;
    return -1;
}

static int transaction_begin(void) {
    if (offline || !mounted) return -5;
    if (read_only) return -30;
    if (transaction_payload == 0) return -5;
    if (transaction_depth++ == 0) {
        transaction_count = 0;
        transaction_error = 0;
    }
    return 0;
}

static int transaction_commit(void) {
    uint8_t header[FS_SECTOR_SIZE] = {0};
    uint8_t targets[FS_JOURNAL_TARGET_SECTORS * FS_SECTOR_SIZE] = {0};
    uint32_t crc = 0xffffffffU;
    if (transaction_error) return transaction_error;
    if (transaction_count == 0) return 0;
    /* Journal target table is always written in full, with unused entries zeroed. */
    for (uint32_t i = 0; i < transaction_count; ++i)
        ((uint32_t *)targets)[i] = transaction_targets[i];
    for (uint32_t i = 0; i < FS_JOURNAL_TARGET_SECTORS; ++i)
        if (raw_write_sector(FS_JOURNAL_START + 1U + i, targets + i * FS_SECTOR_SIZE) < 0) goto io_failure;
    for (uint32_t i = 0; i < transaction_count; ++i)
        if (raw_write_sector(FS_JOURNAL_DATA_START + i, transaction_payload + (size_t)i * FS_SECTOR_SIZE) < 0) goto io_failure;
    if (block_flush() < 0) goto io_failure;
    for (unsigned int i = 0; i < 8; ++i) header[i] = (uint8_t)"WUJNL002"[i];
    ((uint32_t *)header)[2] = 2;
    ((uint32_t *)header)[3] = transaction_count;
    ((uint32_t *)header)[4] = 1;
    crc = crc32_update(crc, header + 8, 12);
    crc = crc32_update(crc, targets, sizeof(targets));
    crc = crc32_update(crc, transaction_payload, (size_t)transaction_count * FS_SECTOR_SIZE);
    ((uint32_t *)header)[5] = ~crc;
    if (raw_write_sector(FS_JOURNAL_START, header) < 0 || block_flush() < 0) goto io_failure;
    for (uint32_t i = 0; i < transaction_count; ++i)
        if (raw_write_sector(transaction_targets[i], transaction_payload + (size_t)i * FS_SECTOR_SIZE) < 0) goto io_failure;
    if (block_flush() < 0) goto io_failure;
    for (unsigned int i = 0; i < FS_SECTOR_SIZE; ++i) header[i] = 0;
    if (raw_write_sector(FS_JOURNAL_START, header) < 0 || block_flush() < 0) goto io_failure;
    return 0;
io_failure:
    offline = 1;
    mounted = 0;
    console_puts("[fs] Transaction I/O failed; volume offline until restart/recovery\n");
    return -5;
}

static int transaction_end(long operation_result) {
    if (!transaction_depth) return -5;
    if (--transaction_depth) return transaction_error ? transaction_error : (int)operation_result;
    if (operation_result < 0 || transaction_error) return transaction_error ? transaction_error : (int)operation_result;
    return transaction_commit();
}

static int journal_recover(uint32_t sectors) {
    uint8_t header[FS_SECTOR_SIZE];
    uint8_t table[FS_JOURNAL_TARGET_SECTORS * FS_SECTOR_SIZE];
    if (raw_read_sector(FS_JOURNAL_START, header) < 0) return -1;
    int all_zero = 1;
    for (size_t i = 0; i < FS_SECTOR_SIZE; ++i) if (header[i]) { all_zero = 0; break; }
    if (all_zero) return 0;
    if (transaction_payload == 0 || !bytes_equal(header, "WUJNL002", 8) ||
        ((uint32_t *)header)[2] != 2U || ((uint32_t *)header)[3] == 0 ||
        ((uint32_t *)header)[3] > FS_JOURNAL_CAPACITY) return -1;
    if (((uint32_t *)header)[4] != 1U) return -1;
    for (size_t i = 24; i < FS_SECTOR_SIZE; ++i) if (header[i]) return -1;
    const uint32_t count = ((uint32_t *)header)[3];
    for (uint32_t i = 0; i < FS_JOURNAL_TARGET_SECTORS; ++i)
        if (raw_read_sector(FS_JOURNAL_START + 1U + i, table + i * FS_SECTOR_SIZE) < 0) return -1;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t target = ((uint32_t *)table)[i];
        if (target < FS_JOURNAL_START + FS_JOURNAL_SECTORS || target >= sectors) return -1;
        for (uint32_t j = 0; j < i; ++j) if (transaction_targets[j] == target) return -1;
        transaction_targets[i] = target;
        if (raw_read_sector(FS_JOURNAL_DATA_START + i,
                            transaction_payload + (size_t)i * FS_SECTOR_SIZE) < 0) return -1;
    }
    for (uint32_t i = count; i < FS_JOURNAL_CAPACITY; ++i)
        if (((uint32_t *)table)[i] != 0) return -1;
    uint32_t crc = 0xffffffffU;
    crc = crc32_update(crc, header + 8, 12);
    crc = crc32_update(crc, table, sizeof(table));
    crc = crc32_update(crc, transaction_payload, (size_t)count * FS_SECTOR_SIZE);
    if (~crc != ((uint32_t *)header)[5]) return -1;
    /* Validate every record before replaying any home sector. */
    for (uint32_t i = 0; i < count; ++i)
        if (raw_write_sector(transaction_targets[i], transaction_payload + (size_t)i * FS_SECTOR_SIZE) < 0) return -1;
    if (block_flush() < 0) return -1;
    for (size_t i = 0; i < FS_SECTOR_SIZE; ++i) header[i] = 0;
    if (raw_write_sector(FS_JOURNAL_START, header) < 0 || block_flush() < 0) return -1;
    transaction_count = 0;
    return 1;
}

static int read_sector(uint32_t sector, void *buffer) {
    if (offline) return -1;
    if (transaction_depth) {
        const int slot = transaction_slot(sector);
        if (slot >= 0) {
            const uint8_t *source = transaction_payload + (size_t)slot * FS_SECTOR_SIZE;
            for (size_t i = 0; i < FS_SECTOR_SIZE; ++i) ((uint8_t *)buffer)[i] = source[i];
            return 0;
        }
    }
    const int result = raw_read_sector(sector, buffer);
    if (result < 0 && transaction_depth && !transaction_error) transaction_error = -5;
    return result;
}

/*
 * Read-only blank-media check: 1 = every byte is zero, 0 = data found,
 * -1 = a sector could not be read. An I/O error is never evidence of blankness.
 * Checking only sector 0 would miss files behind a cleared/corrupt superblock.
 * The full scan is needed only before automatic formatting; valid WUFS mounts
 * skip it. Cost is O(sectors * FS_SECTOR_SIZE) with one sector of stack storage.
 * Boot-time caller must keep the device free of writers through formatting.
 */
static int disk_is_blank(uint32_t sectors) {
    uint8_t buffer[FS_SECTOR_SIZE];
    for (uint32_t sector = 0; sector < sectors; ++sector) {
        if (read_sector(sector, buffer) < 0) return -1;
        for (uint32_t byte = 0; byte < FS_SECTOR_SIZE; ++byte) {
            if (buffer[byte] != 0) return 0;
        }
    }
    return 1;
}

static int write_sector(uint32_t sector, const void *buffer) {
    if (offline || read_only) return -1;
    if (!transaction_depth) return raw_write_sector(sector, buffer);
    if (transaction_error) return -1;
    if (sector < FS_JOURNAL_START + FS_JOURNAL_SECTORS || sector >= superblock.sector_count) {
        transaction_error = -5;
        return -1;
    }
    int slot = transaction_slot(sector);
    if (slot < 0) {
        if (transaction_count >= FS_JOURNAL_CAPACITY) {
            transaction_error = -28;
            return -1;
        }
        slot = (int)transaction_count++;
        transaction_targets[slot] = sector;
    }
    uint8_t *destination = transaction_payload + (size_t)slot * FS_SECTOR_SIZE;
    for (size_t i = 0; i < FS_SECTOR_SIZE; ++i) destination[i] = ((const uint8_t *)buffer)[i];
    return 0;
}

static int bitmap_get(uint32_t sector, uint32_t bit) {
    if ((sector == superblock.inode_bitmap_sector && bit >= superblock.inode_count) ||
        (sector == superblock.block_bitmap_sector && bit >= superblock.sector_count) ||
        (sector != superblock.inode_bitmap_sector && sector != superblock.block_bitmap_sector)) return -1;
    uint8_t bytes[FS_SECTOR_SIZE];
    const uint32_t bitmap_sector = sector + bit / (FS_SECTOR_SIZE * 8U);
    if (read_sector(bitmap_sector, bytes) < 0) {
        return -1;
    }
    return (bytes[(bit / 8U) % FS_SECTOR_SIZE] >> (bit % 8U)) & 1U;
}

static int bitmap_set(uint32_t sector, uint32_t bit, int value) {
    if ((sector == superblock.inode_bitmap_sector && bit >= superblock.inode_count) ||
        (sector == superblock.block_bitmap_sector && bit >= superblock.sector_count) ||
        (sector != superblock.inode_bitmap_sector && sector != superblock.block_bitmap_sector)) return -1;
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
        if (used < 0) return -5;
        if (used == 0) {
            Inode inode = { .type = type };
            if (bitmap_set(superblock.inode_bitmap_sector, i, 1) < 0 || inode_write(i, &inode) < 0) return -5;
            return (long)i;
        }
    }
    return -28;
}

static long block_allocate(void) {
    uint8_t bits[FS_SECTOR_SIZE];
    for (uint32_t map = 0; map < superblock.block_bitmap_length; ++map) {
        if (read_sector(superblock.block_bitmap_sector + map, bits) < 0) return -5;
        const uint32_t first = map * FS_SECTOR_SIZE * 8U;
        for (uint32_t byte = 0; byte < FS_SECTOR_SIZE; ++byte) {
            if (bits[byte] == 0xffU) continue;
            for (uint32_t bit = 0; bit < 8; ++bit) {
                const uint32_t sector = first + byte * 8U + bit;
                if (sector < superblock.data_sector || sector >= superblock.sector_count ||
                    (bits[byte] & (uint8_t)(1U << bit))) continue;
                if (bitmap_set(superblock.block_bitmap_sector, sector, 1) < 0) return -5;
                uint8_t zero[FS_SECTOR_SIZE] = {0};
                if (write_sector(sector, zero) < 0) return transaction_error ? transaction_error : -5;
                return (long)sector;
            }
        }
    }
    return -28;
}

static int inode_block(const Inode *inode, uint32_t index, int allocate, uint32_t *result) {
    if (index < FS_DIRECT) {
        uint32_t *entry = (uint32_t *)&inode->direct[index];
        if (*entry && (*entry < superblock.data_sector || *entry >= superblock.sector_count)) return -1;
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
    if (indirect_sector && (indirect_sector < superblock.data_sector || indirect_sector >= superblock.sector_count)) return -1;
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
    if (pointers[pointer_index] && (pointers[pointer_index] < superblock.data_sector ||
                                    pointers[pointer_index] >= superblock.sector_count)) return -1;
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
    if (offline) return -5;
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

static long fs_is_directory_impl(const char *path) {
    uint32_t number;
    int result = path_lookup(path, &number);
    if (result < 0) return result;
    Inode inode;
    if (inode_read(number, &inode) < 0) return -5;
    return inode.type == FS_TYPE_DIRECTORY ? 0 : -20;
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
    if (node_number < 0) return node_number;
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
        .inode_bitmap_sector = FS_JOURNAL_START + FS_JOURNAL_SECTORS,
        .block_bitmap_sector = FS_JOURNAL_START + FS_JOURNAL_SECTORS + 1U,
        .block_bitmap_length = block_bitmap_length,
        .inode_table_sector = FS_JOURNAL_START + FS_JOURNAL_SECTORS + 1U + block_bitmap_length,
        .inode_table_length = (FS_INODE_COUNT * FS_INODE_SIZE) / FS_SECTOR_SIZE,
        .data_sector = FS_JOURNAL_START + FS_JOURNAL_SECTORS + 1U + block_bitmap_length + (FS_INODE_COUNT * FS_INODE_SIZE) / FS_SECTOR_SIZE,
        .root_inode = FS_ROOT_INODE
    };
    if (sectors <= superblock.data_sector) return -1;
    format_version = 2;
    uint8_t sector[FS_SECTOR_SIZE] = {0};
    for (uint32_t lba = FS_JOURNAL_START; lba < FS_JOURNAL_START + FS_JOURNAL_SECTORS; ++lba)
        if (raw_write_sector(lba, sector) < 0) return -1;
    if (write_sector(superblock.inode_bitmap_sector, sector) < 0) return -1;
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
    Inode root = { .type = FS_TYPE_DIRECTORY };
    if (inode_write(FS_ROOT_INODE, &root) < 0) return -1;
    mounted = 1;
    DirectoryEntry dot = { .name = ".", .inode = FS_ROOT_INODE };
    DirectoryEntry dotdot = { .name = "..", .inode = FS_ROOT_INODE };
    if (dir_entry_write(&root, 0, &dot) < 0 || dir_entry_write(&root, 1, &dotdot) < 0 ||
        inode_write(FS_ROOT_INODE, &root) < 0) return -1;
    if (block_flush() < 0) return -1;
    for (uint32_t i = 0; i < sizeof(FS_MAGIC_V2) - 1; ++i) sector[i] = (uint8_t)FS_MAGIC_V2[i];
    ((uint32_t *)sector)[2] = superblock.sector_count;
    ((uint32_t *)sector)[3] = superblock.inode_count;
    ((uint32_t *)sector)[4] = superblock.inode_bitmap_sector;
    ((uint32_t *)sector)[5] = superblock.block_bitmap_sector;
    ((uint32_t *)sector)[6] = superblock.block_bitmap_length;
    ((uint32_t *)sector)[7] = superblock.inode_table_sector;
    ((uint32_t *)sector)[8] = superblock.inode_table_length;
    ((uint32_t *)sector)[9] = superblock.data_sector;
    ((uint32_t *)sector)[10] = superblock.root_inode;
    ((uint32_t *)sector)[11] = FS_JOURNAL_START;
    ((uint32_t *)sector)[12] = FS_JOURNAL_SECTORS;
    ((uint32_t *)sector)[13] = FS_JOURNAL_CAPACITY;
    if (raw_write_sector(0, sector) < 0 || block_flush() < 0) return -1;
    return 0;
}

static int layout_valid(int v2, uint32_t device_sectors) {
    const uint32_t expected_bitmap_length =
        (superblock.sector_count + FS_SECTOR_SIZE * 8U - 1U) / (FS_SECTOR_SIZE * 8U);
    if (superblock.sector_count > device_sectors || superblock.sector_count > FS_MAX_SECTORS ||
        superblock.sector_count <= superblock.data_sector || superblock.inode_count != FS_INODE_COUNT ||
        superblock.root_inode != FS_ROOT_INODE || superblock.inode_bitmap_sector != (v2 ? FS_JOURNAL_START + FS_JOURNAL_SECTORS : 1U) ||
        superblock.block_bitmap_length != expected_bitmap_length || superblock.inode_table_length != FS_INODE_COUNT * FS_INODE_SIZE / FS_SECTOR_SIZE)
        return 0;
    const uint32_t expected_block_bitmap = superblock.inode_bitmap_sector + 1U;
    const uint32_t expected_inode_table = expected_block_bitmap + expected_bitmap_length;
    const uint32_t expected_data = expected_inode_table + superblock.inode_table_length;
    return superblock.block_bitmap_sector == expected_block_bitmap &&
           superblock.inode_table_sector == expected_inode_table &&
           superblock.data_sector == expected_data &&
           (!v2 || (superblock.inode_bitmap_sector == FS_JOURNAL_START + FS_JOURNAL_SECTORS &&
                    superblock.data_sector > FS_JOURNAL_START + FS_JOURNAL_SECTORS));
}

static int claim_block(uint32_t sector, uint8_t *seen) {
    if (sector < superblock.data_sector || sector >= superblock.sector_count) return -1;
    const int used = bitmap_get(superblock.block_bitmap_sector, sector);
    if (used != 1 || (seen[sector / 8U] & (uint8_t)(1U << (sector % 8U)))) return -1;
    seen[sector / 8U] |= (uint8_t)(1U << (sector % 8U));
    return 0;
}

static int filesystem_validate(void) {
    uint8_t seen[FS_MAX_SECTORS / 8U] = {0};
    uint32_t max_file = (FS_DIRECT + FS_INDIRECT * FS_POINTERS_PER_SECTOR) * FS_SECTOR_SIZE;
    if (superblock.sector_count > FS_MAX_SECTORS) return -1;
    for (uint32_t sector = 0; sector < superblock.data_sector; ++sector)
        if (bitmap_get(superblock.block_bitmap_sector, sector) != 1) return -1;
    for (uint32_t number = 0; number < superblock.inode_count; ++number) {
        const int allocated = bitmap_get(superblock.inode_bitmap_sector, number);
        if (allocated < 0) return -1;
        Inode inode;
        if (inode_read(number, &inode) < 0) return -1;
        if (inode.reserved[0] || inode.reserved[1]) return -1;
        if (!allocated) {
            if (inode.type || inode.size) return -1;
            for (uint32_t i = 0; i < FS_DIRECT; ++i) if (inode.direct[i]) return -1;
            for (uint32_t i = 0; i < FS_INDIRECT; ++i) if (inode.indirect[i]) return -1;
            continue;
        }
        if ((number == superblock.root_inode && inode.type != FS_TYPE_DIRECTORY) ||
            (inode.type != FS_TYPE_DIRECTORY && inode.type != FS_TYPE_FILE) || inode.size > max_file ||
            (inode.type == FS_TYPE_DIRECTORY && inode.size % sizeof(DirectoryEntry))) return -1;
        for (uint32_t i = 0; i < FS_DIRECT; ++i)
            if (inode.direct[i] && claim_block(inode.direct[i], seen) < 0) return -1;
        for (uint32_t i = 0; i < FS_INDIRECT; ++i) {
            if (!inode.indirect[i]) continue;
            if (claim_block(inode.indirect[i], seen) < 0) return -1;
            uint32_t pointers[FS_POINTERS_PER_SECTOR];
            if (read_sector(inode.indirect[i], pointers) < 0) return -1;
            for (uint32_t j = 0; j < FS_POINTERS_PER_SECTOR; ++j)
                if (pointers[j] && claim_block(pointers[j], seen) < 0) return -1;
        }
        const uint32_t needed_blocks = (inode.size + FS_SECTOR_SIZE - 1U) / FS_SECTOR_SIZE;
        for (uint32_t i = 0; i < needed_blocks; ++i) {
            uint32_t block;
            if (inode_block(&inode, i, 0, &block) < 0 || block == 0) return -1;
        }
        if (inode.type == FS_TYPE_DIRECTORY) {
            const uint32_t entries = inode.size / (uint32_t)sizeof(DirectoryEntry);
            if (entries < 2) return -1;
            for (uint32_t i = 0; i < entries; ++i) {
                DirectoryEntry entry;
                if (dir_entry_read(&inode, i, &entry) < 0 || entry.inode >= superblock.inode_count ||
                    bitmap_get(superblock.inode_bitmap_sector, entry.inode) != 1) return -1;
                size_t length = 0;
                while (length < FS_NAME_SIZE && entry.name[length]) ++length;
                if (length == 0 || length == FS_NAME_SIZE) return -1;
                for (size_t j = 0; j < length; ++j) if (entry.name[j] == '/') return -1;
                Inode child;
                if (inode_read(entry.inode, &child) < 0 ||
                    (child.type != FS_TYPE_DIRECTORY && child.type != FS_TYPE_FILE)) return -1;
                if (i == 0 && (!bytes_equal(entry.name, ".", 2) || entry.name[1] != 0 || entry.inode != number)) return -1;
                if (i == 1 && (!bytes_equal(entry.name, "..", 3) || entry.name[2] != 0)) return -1;
            }
            if (number == superblock.root_inode) {
                DirectoryEntry parent_entry;
                if (dir_entry_read(&inode, 1, &parent_entry) < 0 || parent_entry.inode != number) return -1;
            }
        }
    }
    for (uint32_t sector = superblock.data_sector; sector < superblock.sector_count; ++sector) {
        const int allocated = bitmap_get(superblock.block_bitmap_sector, sector);
        const int claimed = (seen[sector / 8U] >> (sector % 8U)) & 1U;
        if (allocated < 0 || allocated != claimed) return -1;
    }
    /* Every directory must be reachable exactly once from root, with a correct '..'. */
    uint32_t queue[FS_INODE_COUNT];
    uint8_t reached[FS_INODE_COUNT] = {0};
    uint32_t head = 0, tail = 1;
    queue[0] = superblock.root_inode;
    reached[superblock.root_inode] = 1;
    while (head < tail) {
        const uint32_t parent_number = queue[head++];
        Inode parent;
        if (inode_read(parent_number, &parent) < 0) return -1;
        const uint32_t entries = parent.size / (uint32_t)sizeof(DirectoryEntry);
        for (uint32_t i = 2; i < entries; ++i) {
            DirectoryEntry entry;
            Inode child;
            if (dir_entry_read(&parent, i, &entry) < 0 || inode_read(entry.inode, &child) < 0) return -1;
            if (reached[entry.inode]) return -1;
            for (uint32_t j = 2; j < i; ++j) {
                DirectoryEntry previous;
                if (dir_entry_read(&parent, j, &previous) < 0) return -1;
                size_t k = 0;
                while (k < FS_NAME_SIZE && previous.name[k] == entry.name[k] && entry.name[k]) ++k;
                if (k < FS_NAME_SIZE && previous.name[k] == 0 && entry.name[k] == 0) return -1;
            }
            reached[entry.inode] = 1;
            if (child.type != FS_TYPE_DIRECTORY) continue;
            if (tail >= FS_INODE_COUNT) return -1;
            DirectoryEntry dotdot;
            if (dir_entry_read(&child, 1, &dotdot) < 0 || dotdot.inode != parent_number) return -1;
            queue[tail++] = entry.inode;
        }
    }
    for (uint32_t number = 0; number < superblock.inode_count; ++number) {
        Inode inode;
        if (inode_read(number, &inode) < 0) return -1;
        if (inode.type && !reached[number]) return -1;
    }
    return bitmap_get(superblock.inode_bitmap_sector, superblock.root_inode) == 1 ? 0 : -1;
}

int fs_init(void) {
    /* A failed initialization must not retain a previous mounted state. */
    mounted = 0;
    read_only = 0;
    offline = 0;
    transaction_depth = 0;
    uint8_t sector[FS_SECTOR_SIZE] = {0};
    const uint32_t sectors = block_sector_count();
    if (sectors == 0 || sectors > FS_MAX_SECTORS) {
        console_puts("[fs] Unsupported device capacity; disk left unchanged\n");
        return -1;
    }
    if (read_sector(0, sector) < 0) {
        console_puts("[fs] Cannot read filesystem header; disk left unchanged\n");
        return -1;
    }
    int v1 = bytes_equal(sector, FS_MAGIC, 8);
    int v2 = bytes_equal(sector, FS_MAGIC_V2, 8);
    if (!v1 && !v2) {
        /*
         * Missing magic can mean an unsupported format or damaged WUFS, not
         * empty media. No disk writes are allowed until blankness is proven.
         * block_sector_count() saturates at UINT32_MAX; at that limit a scan
         * could cover only a prefix of a larger device, so refuse auto-format.
         */
        if (sectors == UINT32_MAX) {
            console_puts("[fs] Cannot verify full disk capacity; refusing automatic format\n");
            return -1;
        }
        const int blank = disk_is_blank(sectors);
        if (blank < 0) {
            console_puts("[fs] Blank-disk check failed: read error; disk left unchanged\n");
            return -1;
        }
        if (!blank) {
            console_puts("[fs] Unknown or damaged filesystem on nonblank disk; refusing format\n");
            return -1;
        }
        if (sectors <= FS_JOURNAL_START + FS_JOURNAL_SECTORS + 1U + 8U + FS_INODE_COUNT * FS_INODE_SIZE / FS_SECTOR_SIZE ||
            !block_flush_supported()) {
            console_puts("[fs] Device capacity or FLUSH support is insufficient; disk left unchanged\n");
            return -1;
        }
        console_puts("[fs] Initializing blank disk as WUFS0002\n");
        const size_t cache_pages = (FS_JOURNAL_CAPACITY * FS_SECTOR_SIZE + PAGE_SIZE - 1U) / PAGE_SIZE;
        transaction_payload = (uint8_t *)page_alloc_contiguous(cache_pages);
        if (!transaction_payload) { console_puts("[fs] Journal cache allocation failed; disk unchanged\n"); return -1; }
        if (format_filesystem(sectors) < 0) {
            /* The formatter may have enabled inode access before failing. */
            mounted = 0;
            return -1;
        }
        mounted = 1;
        if (fs_mkdir("/bin") < 0 || fs_mkdir("/boot") < 0 || fs_mkdir("/dev") < 0 ||
            fs_mkdir("/home") < 0 || fs_mkdir("/home/user") < 0) {
            mounted = 0;
            offline = 1;
            console_puts("[fs] Initial directory creation failed; volume offline\n");
            return -1;
        }
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
        format_version = v2 ? 2 : 1;
        if (!layout_valid(v2, sectors)) { console_puts("[fs] Invalid WUFS layout; refusing mount\n"); return -1; }
        if (v2) {
            if (fields[11] != FS_JOURNAL_START || fields[12] != FS_JOURNAL_SECTORS || fields[13] != FS_JOURNAL_CAPACITY) return -1;
            for (size_t i = 14U * sizeof(uint32_t); i < FS_SECTOR_SIZE; ++i)
                if (sector[i]) { console_puts("[fs] Nonzero reserved superblock bytes; refusing mount\n"); return -1; }
            const size_t cache_pages = (FS_JOURNAL_CAPACITY * FS_SECTOR_SIZE + PAGE_SIZE - 1U) / PAGE_SIZE;
            transaction_payload = (uint8_t *)page_alloc_contiguous(cache_pages);
            if (!transaction_payload) { console_puts("[fs] Journal cache allocation failed; refusing mount\n"); return -1; }
            if (!block_flush_supported()) {
                uint8_t journal_header[FS_SECTOR_SIZE];
                if (raw_read_sector(FS_JOURNAL_START, journal_header) < 0) return -1;
                for (size_t i = 0; i < FS_SECTOR_SIZE; ++i)
                    if (journal_header[i]) { offline = 1; console_puts("[fs] Pending journal requires FLUSH support; refusing mount\n"); return -1; }
                read_only = 1;
                console_puts("[fs] Device lacks FLUSH; WUFS0002 mounted read-only\n");
            } else {
                const int recovered = journal_recover(superblock.sector_count);
                if (recovered < 0) { offline = 1; console_puts("[fs] Invalid or unrecoverable journal; volume unavailable, journal retained when possible\n"); return -1; }
                if (block_flush() < 0) { offline = 1; return -1; }
                if (recovered > 0) console_puts("[fs] Replayed committed WUFS journal\n");
            }
        } else {
            read_only = 1;
            console_puts("[fs] WUFS0001 mounted read-only; use a new WUFS0002 disk for writes\n");
        }
        mounted = 1;
        if (filesystem_validate() < 0) { mounted = 0; offline = 1; console_puts("[fs] Metadata validation failed; refusing mount\n"); return -1; }
        console_puts("[fs] Mounted existing WUFS volume\n");
    }
    return 0;
}

static long fs_list_impl(const char *path, char *output, size_t capacity) {
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

static long fs_mkdir_impl(const char *path) {
    const long result = create_node(path, FS_TYPE_DIRECTORY);
    return result < 0 ? result : 0;
}

static long fs_touch_impl(const char *path) {
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
            if (inode->direct[i] < superblock.data_sector || inode->direct[i] >= superblock.sector_count) return -1;
            if (bitmap_set(superblock.block_bitmap_sector, inode->direct[i], 0) < 0) return -1;
            inode->direct[i] = 0;
        }
    }
    for (uint32_t i = 0; i < FS_INDIRECT; ++i) {
        if (inode->indirect[i] == 0) continue;
        if (inode->indirect[i] < superblock.data_sector || inode->indirect[i] >= superblock.sector_count) return -1;
        uint32_t pointers[FS_POINTERS_PER_SECTOR];
        if (read_sector(inode->indirect[i], pointers) < 0) return -1;
        for (uint32_t j = 0; j < FS_POINTERS_PER_SECTOR; ++j) {
            if (pointers[j] && (pointers[j] < superblock.data_sector || pointers[j] >= superblock.sector_count)) return -1;
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

static long fs_remove_impl(const char *path) {
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
    if (io_inode_is_open(entry.inode)) return -16;

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

/* Validate the whole tree before deleting: no root, cycles or live CWDs.
   A bounded breadth-first list reversed for deletion gives child-before-parent. */
static long fs_remove_tree_impl(const char *path) {
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
    if (entry.inode == superblock.root_inode) return -16;
    if (inode_read(entry.inode, &inode) < 0) return -5;
    if (inode.type != FS_TYPE_DIRECTORY) return -20;

    uint32_t nodes[FS_INODE_COUNT], parents[FS_INODE_COUNT];
    uint8_t seen[FS_INODE_COUNT] = {0};
    uint32_t count = 1;
    nodes[0] = entry.inode; parents[0] = parent_number;
    if (entry.inode >= FS_INODE_COUNT) return -5;
    seen[entry.inode] = 1;
    for (uint32_t i = 0; i < count; ++i) {
        if (inode_read(nodes[i], &inode) < 0) return -5;
        if (io_inode_is_open(nodes[i])) return -16;
        if (inode.type == FS_TYPE_FILE) continue;
        if (inode.type != FS_TYPE_DIRECTORY || inode.size % sizeof(DirectoryEntry)) return -5;
        for (int slot = 0; slot < PROCESS_LIMIT; ++slot) {
            const Process *p = process_slot(slot);
            if (p->state == PROCESS_UNUSED || p->state == PROCESS_ZOMBIE) continue;
            uint32_t cwd;
            error = path_lookup(p->cwd, &cwd);
            if (error < 0) return error;
            if (cwd == nodes[i]) return -16;
        }
        uint32_t entries = inode.size / (uint32_t)sizeof(DirectoryEntry);
        for (uint32_t j = 0; j < entries; ++j) {
            if (dir_entry_read(&inode, j, &entry) < 0) return -5;
            if (special_name(entry.name)) continue;
            if (entry.inode >= FS_INODE_COUNT || seen[entry.inode] || count == FS_INODE_COUNT) return -5;
            seen[entry.inode] = 1;
            nodes[count] = entry.inode; parents[count++] = nodes[i];
        }
    }
    while (count > 0) {
        --count;
        if (inode_read(parents[count], &parent) < 0 || inode_read(nodes[count], &inode) < 0) return -5;
        uint32_t entries = parent.size / (uint32_t)sizeof(DirectoryEntry);
        uint32_t found = entries;
        for (uint32_t j = 0; j < entries; ++j) {
            if (dir_entry_read(&parent, j, &entry) < 0) return -5;
            if (entry.inode == nodes[count] && !special_name(entry.name)) { found = j; break; }
        }
        if (found == entries) return -5;
        if (found != entries - 1) {
            DirectoryEntry last;
            if (dir_entry_read(&parent, entries - 1, &last) < 0 || dir_entry_write(&parent, found, &last) < 0) return -5;
        }
        parent.size -= (uint32_t)sizeof(DirectoryEntry);
        if (inode_write(parents[count], &parent) < 0 || inode_release(nodes[count], &inode) < 0) return -5;
    }
    return 0;
}

static long fs_copy_impl(const char *source, const char *destination) {
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
        if (io_inode_is_open(entry.inode)) return -16;
        if (inode_read(entry.inode, &old) < 0) return -5;
        if (old.type != FS_TYPE_FILE) return -21;
    }

    const long allocated = inode_allocate(FS_TYPE_FILE);
    if (allocated < 0) return allocated;
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

static long fs_size_impl(const char *path) {
    uint32_t number;
    int error = path_lookup(path, &number);
    if (error < 0) return error;
    Inode inode;
    if (inode_read(number, &inode) < 0) return -5;
    return inode.type == FS_TYPE_FILE ? (long)inode.size : -21;
}

static long fs_read_impl(const char *path, size_t offset, void *buffer, size_t length) {
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

static long fs_write_impl(const char *path, const void *buffer, size_t length) {
    if (length > (FS_DIRECT + FS_INDIRECT * FS_POINTERS_PER_SECTOR) * FS_SECTOR_SIZE) return -27;
    uint32_t number;
    int found = path_lookup(path, &number);
    if (found == -2) {
        const long created = create_node(path, FS_TYPE_FILE);
        if (created < 0) return created;
        number = (uint32_t)created;
    } else if (found < 0) {
        return found;
    } else if (io_inode_is_open(number)) {
        return -16;
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

static long fs_open_inode_impl(const char *path, int create, uint32_t *number) {
    if (!mounted) return -22;
    uint32_t inode_number;
    int found = path_lookup(path, &inode_number);
    if (found == -2 && create) {
        const long created = fs_touch(path);
        if (created < 0) return created;
        found = path_lookup(path, &inode_number);
    }
    if (found < 0) return found;
    Inode inode;
    if (inode_read(inode_number, &inode) < 0) return -5;
    if (inode.type != FS_TYPE_FILE) return -21;
    *number = inode_number;
    return 0;
}

static long fs_inode_size_impl(uint32_t number) {
    Inode inode;
    if (inode_read(number, &inode) < 0) return -5;
    return inode.type == FS_TYPE_FILE ? (long)inode.size : -21;
}

static long fs_read_inode_impl(uint32_t number, size_t offset, void *buffer, size_t length) {
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
        if (inode_block(&inode, (uint32_t)(position / FS_SECTOR_SIZE), 0, &sector) < 0 ||
            sector == 0 || read_sector(sector, data) < 0) return -5;
        const size_t in_sector = position % FS_SECTOR_SIZE;
        size_t amount = FS_SECTOR_SIZE - in_sector;
        if (amount > length - copied) amount = length - copied;
        for (size_t i = 0; i < amount; ++i) ((uint8_t *)buffer)[copied + i] = data[in_sector + i];
        copied += amount;
    }
    return (long)copied;
}

static long fs_write_inode_impl(uint32_t number, size_t offset, const void *buffer, size_t length) {
    const size_t maximum = (FS_DIRECT + FS_INDIRECT * FS_POINTERS_PER_SECTOR) * FS_SECTOR_SIZE;
    if (offset > maximum || length > maximum - offset || offset + length > UINT32_MAX) return -27;
    Inode inode;
    if (inode_read(number, &inode) < 0 || inode.type != FS_TYPE_FILE) return -21;
    size_t copied = 0;
    long failure = -5;
    while (copied < length) {
        const size_t position = offset + copied;
        uint32_t sector, existing;
        uint8_t data[FS_SECTOR_SIZE] = {0};
        const uint32_t block = (uint32_t)(position / FS_SECTOR_SIZE);
        if (inode_block(&inode, block, 0, &existing) < 0) { failure = -5; break; }
        if (existing) sector = existing;
        else if (inode_block(&inode, block, 1, &sector) < 0 || sector == 0) { failure = -28; break; }
        const size_t in_sector = position % FS_SECTOR_SIZE;
        if (existing && read_sector(sector, data) < 0) { failure = -5; break; }
        size_t amount = FS_SECTOR_SIZE - in_sector;
        if (amount > length - copied) amount = length - copied;
        for (size_t i = 0; i < amount; ++i) data[in_sector + i] = ((const uint8_t *)buffer)[copied + i];
        if (write_sector(sector, data) < 0) { failure = -5; break; }
        copied += amount;
    }
    if (copied != length && length != 0) return failure;
    if (length && offset + copied > inode.size) inode.size = (uint32_t)(offset + copied);
    if (inode_write(number, &inode) < 0) return -5;
    return (long)copied;
}

static long fs_truncate_inode_impl(uint32_t number) {
    Inode inode;
    if (inode_read(number, &inode) < 0 || inode.type != FS_TYPE_FILE) return -21;
    if (inode_free_blocks(&inode) < 0) return -5;
    inode.size = 0;
    return inode_write(number, &inode) < 0 ? -5 : 0;
}

long fs_mkdir(const char *path) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_mkdir_impl(path);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_touch(const char *path) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_touch_impl(path);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_remove(const char *path) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_remove_impl(path);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_remove_tree(const char *path) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_remove_tree_impl(path);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_copy(const char *source, const char *destination) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_copy_impl(source, destination);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_write(const char *path, const void *buffer, size_t length) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_write_impl(path, buffer, length);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_write_inode(uint32_t number, size_t offset, const void *buffer, size_t length) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_write_inode_impl(number, offset, buffer, length);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_truncate_inode(uint32_t number) {
    int error = fs_scope_enter();
    if (error < 0) return error;
    error = transaction_begin();
    if (error < 0) return fs_scope_finish(error);
    const long result = fs_truncate_inode_impl(number);
    error = transaction_end(result);
    return fs_scope_finish(error < 0 ? error : result);
}

long fs_is_directory(const char *path) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_is_directory_impl(path));
}

long fs_list(const char *path, char *output, size_t capacity) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_list_impl(path, output, capacity));
}

long fs_size(const char *path) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_size_impl(path));
}

long fs_read(const char *path, size_t offset, void *buffer, size_t length) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_read_impl(path, offset, buffer, length));
}

long fs_open_inode(const char *path, int create, uint32_t *number) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_open_inode_impl(path, create, number));
}

long fs_inode_size(uint32_t number) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_inode_size_impl(number));
}

long fs_read_inode(uint32_t number, size_t offset, void *buffer, size_t length) {
    int error = fs_scope_enter();
    return error < 0 ? error : fs_scope_finish(fs_read_inode_impl(number, offset, buffer, length));
}
