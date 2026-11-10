#include "drivers/uart/uart.h"
#include <stddef.h>
#include <stdint.h>

#define SECTOR_SIZE 512U
#define WUFS_INODES 128U
#define WUFS_INODE_SIZE 80U
#define WUFS_DIRECT 8U
#define WUFS_MAX_SECTORS 32768U
#define LOAD_BUFFER ((uint8_t *)(uintptr_t)0x81010000UL)
#define LOAD_BUFFER_SIZE (640U * 1024U)
#define KERNEL_LOW 0x80200000UL
#define KERNEL_HIGH 0x80400000UL
#define VIRTIO_MAGIC 0x000U
#define VIRTIO_VERSION 0x004U
#define VIRTIO_DEVICE_ID 0x008U
#define VIRTIO_MMIO_BASE 0x10001000UL
#define VIRTIO_MMIO_STRIDE 0x1000UL
#define VIRTIO_MMIO_COUNT 8U

typedef struct {
  uint64_t addr;
  uint32_t len;
  uint16_t flags, next;
} __attribute__((aligned(16))) Desc;
typedef struct {
  uint16_t flags;
  volatile uint16_t idx;
  uint16_t ring[8], event;
} __attribute__((aligned(2))) Avail;
typedef struct {
  uint32_t id, len;
} UsedElem;
typedef struct {
  uint16_t flags;
  volatile uint16_t idx;
  UsedElem ring[8];
  uint16_t event;
} __attribute__((aligned(4))) Used;
typedef struct {
  uint32_t type, reserved;
  uint64_t sector;
} Request;
typedef struct {
  uint32_t type, size, direct[WUFS_DIRECT], indirect[8], reserved[2];
} Inode;
typedef struct {
  char name[24];
  uint32_t inode;
} Dirent;

_Static_assert(sizeof(Inode) == WUFS_INODE_SIZE, "WUFS inode size");
_Static_assert(sizeof(Dirent) == 28, "WUFS directory entry size");

static volatile uint32_t *regs;
static uint64_t capacity;
static Desc desc[8] __attribute__((aligned(16)));
static Avail avail __attribute__((aligned(2)));
static Used used __attribute__((aligned(4)));
static Request request __attribute__((aligned(16)));
static volatile uint8_t request_status;
static uint8_t sector_buffer[SECTOR_SIZE] __attribute__((aligned(16)));
static uint8_t inode_bytes[WUFS_INODE_SIZE];
static uint8_t file_sector[SECTOR_SIZE];
static uint16_t last_used;

static uint32_t le32(const void *p) {
  const uint8_t *b = p;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
}
static uint16_t le16(const void *p) {
  const uint8_t *b = p;
  return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}
static void print(const char *s) {
  while (*s) {
    if (*s == '\n')
      uart_putc_raw('\r');
    uart_putc_raw(*s++);
  }
}
static int field_eq(const char *field, size_t length, const char *value) {
  size_t i = 0;
  while (i < length && value[i] && field[i] == value[i])
    ++i;
  return i < length && field[i] == '\0' && value[i] == '\0';
}
static int bytes_eq(const uint8_t *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (a[i] != (uint8_t)b[i])
      return 0;
  return 1;
}
static void copy(void *d, const void *s, size_t n) {
  uint8_t *a = d;
  const uint8_t *b = s;
  while (n--)
    *a++ = *b++;
}
static void zero(void *d, size_t n) {
  uint8_t *a = d;
  while (n--)
    *a++ = 0;
}

/* QEMU virt has eight 4 KiB virtio-mmio slots beginning at 0x10001000. */
static uintptr_t find_block(void) {
  uintptr_t found = 0;
  unsigned count = 0;
  for (unsigned i = 0; i < VIRTIO_MMIO_COUNT; ++i) {
    uintptr_t base = VIRTIO_MMIO_BASE + (uintptr_t)i * VIRTIO_MMIO_STRIDE;
    volatile uint32_t *candidate = (volatile uint32_t *)base;
    if (candidate[VIRTIO_MAGIC / 4] != 0x74726976U ||
        candidate[VIRTIO_VERSION / 4] != 2U ||
        candidate[VIRTIO_DEVICE_ID / 4] != 2U)
      continue;
    found = base;
    ++count;
  }
  return count == 1 ? found : 0;
}

static void mmio_addr(unsigned off, uintptr_t addr) {
  regs[off / 4] = (uint32_t)addr;
  regs[(off + 4) / 4] = (uint32_t)(addr >> 32);
}
static int disk_init(uintptr_t base) {
  regs = (volatile uint32_t *)base;
  regs[0x70 / 4] = 0;
  for (volatile unsigned i = 0; i < 10000 && regs[0x70 / 4]; ++i) {
  }
  if (regs[0x70 / 4])
    return -1;
  regs[0x70 / 4] = 1;
  regs[0x70 / 4] |= 2;
  regs[0x14 / 4] = 1;
  uint32_t hi = regs[0x10 / 4];
  if (!(hi & 1U))
    return -1;
  regs[0x24 / 4] = 0;
  regs[0x20 / 4] = 0;
  regs[0x24 / 4] = 1;
  regs[0x20 / 4] = 1;
  regs[0x70 / 4] |= 8;
  if (!(regs[0x70 / 4] & 8))
    return -1;
  regs[0x30 / 4] = 0;
  if (regs[0x34 / 4] < 8)
    return -1;
  regs[0x38 / 4] = 8;
  zero(&avail, sizeof(avail));
  zero(&used, sizeof(used));
  zero(desc, sizeof(desc));
  avail.idx = 0;
  used.idx = 0;
  last_used = 0;
  mmio_addr(0x80, (uintptr_t)desc);
  mmio_addr(0x90, (uintptr_t)&avail);
  mmio_addr(0xa0, (uintptr_t)&used);
  regs[0x44 / 4] = 1;
  regs[0x70 / 4] |= 4;
  volatile uint32_t *cfg = (volatile uint32_t *)((uintptr_t)regs + 0x100);
  uint32_t g0, g1, l = 0, h = 0;
  unsigned generation_spins = 1000000;
  do {
    if (!generation_spins--)
      return -1;
    g0 = regs[0xfc / 4];
    l = cfg[0];
    h = cfg[1];
    g1 = regs[0xfc / 4];
  } while (g0 != g1);
  capacity = ((uint64_t)h << 32) | l;
  return capacity ? 0 : -1;
}
static int read_sector(uint32_t sector, void *buffer) {
  if (!regs || sector >= capacity)
    return -1;
  request = (Request){0, 0, sector};
  request_status = 0xff;
  desc[0] = (Desc){(uintptr_t)&request, sizeof(request), 1, 1};
  desc[1] = (Desc){(uintptr_t)buffer, SECTOR_SIZE, 3, 2};
  desc[2] = (Desc){(uintptr_t)&request_status, 1, 2, 0};
  avail.ring[avail.idx % 8] = 0;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  ++avail.idx;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  regs[0x50 / 4] = 0;
  unsigned spins = 10000000;
  while (used.idx == last_used && spins--)
    __asm__ volatile("nop");
  if ((uint16_t)(used.idx - last_used) != 1)
    return -1;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  uint32_t used_id = used.ring[last_used % 8].id,
           used_len = used.ring[last_used % 8].len;
  ++last_used;
  uint32_t irq = regs[0x60 / 4];
  if (irq)
    regs[0x64 / 4] = irq;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return used_id == 0 && used_len == SECTOR_SIZE + 1 && request_status == 0
             ? 0
             : -1;
}

static int read_inode(uint32_t id, Inode *out, uint32_t table_sector) {
  if (id >= WUFS_INODES)
    return -1;
  uint32_t off = id * WUFS_INODE_SIZE, done = 0;
  while (done < WUFS_INODE_SIZE) {
    uint32_t pos = off + done, s = table_sector + pos / SECTOR_SIZE,
             at = pos % SECTOR_SIZE;
    if (read_sector(s, sector_buffer) < 0)
      return -1;
    uint32_t n = SECTOR_SIZE - at;
    if (n > WUFS_INODE_SIZE - done)
      n = WUFS_INODE_SIZE - done;
    copy(inode_bytes + done, sector_buffer + at, n);
    done += n;
  }
  copy(out, inode_bytes, sizeof(*out));
  return 0;
}
static int file_block(const Inode *ino, uint32_t index, uint32_t data_start,
                      uint32_t sectors, uint32_t *out) {
  uint32_t p = 0;
  if (index < WUFS_DIRECT)
    p = ino->direct[index];
  else {
    index -= WUFS_DIRECT;
    uint32_t table = index / 128, slot = index % 128;
    if (table >= 8 || !ino->indirect[table])
      return -1;
    if (ino->indirect[table] < data_start || ino->indirect[table] >= sectors ||
        read_sector(ino->indirect[table], sector_buffer) < 0)
      return -1;
    p = le32(sector_buffer + slot * 4);
  }
  if (p < data_start || p >= sectors)
    return -1;
  *out = p;
  return 0;
}
static int file_read(const Inode *ino, uint32_t data_start, uint32_t sectors,
                     uint32_t off, void *dst, uint32_t len) {
  if (off > ino->size || len > ino->size - off)
    return -1;
  uint8_t *d = dst;
  while (len) {
    uint32_t block, bi = off / SECTOR_SIZE, at = off % SECTOR_SIZE,
                    n = SECTOR_SIZE - at;
    if (n > len)
      n = len;
    if (file_block(ino, bi, data_start, sectors, &block) < 0 ||
        read_sector(block, file_sector) < 0)
      return -1;
    copy(d, file_sector + at, n);
    d += n;
    off += n;
    len -= n;
  }
  return 0;
}
static int find_kernel(uint32_t itable, uint32_t data_start, uint32_t sectors,
                       Inode *result) {
  Inode dir;
  if (read_inode(0, &dir, itable) < 0 || dir.type != 1)
    return -1;
  uint32_t boot = UINT32_MAX;
  for (uint32_t off = 0; off + sizeof(Dirent) <= dir.size; off += sizeof(Dirent)) {
    Dirent de;
    if (file_read(&dir, data_start, sectors, off, &de, sizeof(de)) < 0)
      return -1;
    if (field_eq(de.name, sizeof(de.name), "boot")) {
      boot = de.inode;
      break;
    }
  }
  if (boot >= WUFS_INODES)
    return -1;
  if (read_inode(boot, &dir, itable) < 0 || dir.type != 1)
    return -1;
  for (uint32_t off = 0; off + sizeof(Dirent) <= dir.size; off += sizeof(Dirent)) {
    Dirent de;
    if (file_read(&dir, data_start, sectors, off, &de, sizeof(de)) < 0)
      return -1;
    if (field_eq(de.name, sizeof(de.name), "kernel.elf")) {
      if (read_inode(de.inode, result, itable) < 0)
        return -1;
      return result->type == 2 ? 0 : -1;
    }
  }
  return -1;
}

static int load_elf(const Inode *ino, uint32_t data_start, uint32_t sectors,
                    uintptr_t *entry_out) {
  if (ino->size < 64 || ino->size > LOAD_BUFFER_SIZE ||
      file_read(ino, data_start, sectors, 0, LOAD_BUFFER, ino->size) < 0)
    return -1;
  const uint8_t *e = LOAD_BUFFER;
  if (e[0] != 0x7f || e[1] != 'E' || e[2] != 'L' || e[3] != 'F' || e[4] != 2 ||
      e[5] != 1 || e[6] != 1 || le16(e + 16) != 2 || le16(e + 18) != 243 ||
      le32(e + 20) != 1)
    return -1;
  uint64_t entry = (uint64_t)le32(e + 24) | ((uint64_t)le32(e + 28) << 32);
  uint64_t phoff = (uint64_t)le32(e + 32) | ((uint64_t)le32(e + 36) << 32);
  uint16_t phentsize = (uint16_t)(e[54] | ((uint16_t)e[55] << 8)),
           phnum = (uint16_t)(e[56] | ((uint16_t)e[57] << 8));
  if (phentsize < 56 || phnum == 0 || phoff > ino->size ||
      (uint64_t)phnum * phentsize > ino->size - phoff)
    return -1;
  int entry_ok = 0;
  uintptr_t starts[32], ends[32];
  unsigned nseg = 0;
  if (phnum > 32)
    return -1;
  for (unsigned i = 0; i < phnum; ++i) {
    const uint8_t *p = e + phoff + (uint64_t)i * phentsize;
    if (le32(p) != 1)
      continue;
    uint32_t flags = le32(p + 4);
    uint64_t off = (uint64_t)le32(p + 8) | ((uint64_t)le32(p + 12) << 32);
    uint64_t va = (uint64_t)le32(p + 16) | ((uint64_t)le32(p + 20) << 32);
    uint64_t pa = (uint64_t)le32(p + 24) | ((uint64_t)le32(p + 28) << 32);
    uint64_t fs = (uint64_t)le32(p + 32) | ((uint64_t)le32(p + 36) << 32);
    uint64_t ms = (uint64_t)le32(p + 40) | ((uint64_t)le32(p + 44) << 32);
    if (fs > ms || off > ino->size || fs > ino->size - off || pa < KERNEL_LOW ||
        pa > KERNEL_HIGH || ms > KERNEL_HIGH - pa || va != pa)
      return -1;
    if (ms) {
      uintptr_t end = (uintptr_t)(pa + ms);
      for (unsigned j = 0; j < nseg; ++j)
        if ((uintptr_t)pa < ends[j] && end > starts[j])
          return -1;
      starts[nseg] = (uintptr_t)pa;
      ends[nseg++] = end;
      if ((flags & 1) && entry >= pa && entry < pa + fs && !(entry & 1U))
        entry_ok = 1;
    }
  }
  if (!entry_ok || nseg == 0)
    return -1;
  /* All PT_LOAD headers are validated before any kernel RAM is modified. */
  for (unsigned i = 0; i < phnum; ++i) {
    const uint8_t *p = e + phoff + (uint64_t)i * phentsize;
    if (le32(p) != 1)
      continue;
    uint64_t off = (uint64_t)le32(p + 8) | ((uint64_t)le32(p + 12) << 32),
             pa = (uint64_t)le32(p + 24) | ((uint64_t)le32(p + 28) << 32);
    uint64_t fs = (uint64_t)le32(p + 32) | ((uint64_t)le32(p + 36) << 32),
             ms = (uint64_t)le32(p + 40) | ((uint64_t)le32(p + 44) << 32);
    copy((void *)(uintptr_t)pa, e + off, (size_t)fs);
    zero((void *)(uintptr_t)(pa + fs), (size_t)(ms - fs));
  }
  *entry_out = (uintptr_t)entry;
  return 0;
}

void boot_main(uintptr_t hartid, uintptr_t fdt) {
  print("WUWAOS firmware loader\n");
  uintptr_t base = find_block();
  if (!base || disk_init(base) < 0) {
    print("boot: virtio-blk init failed\n");
    return;
  }
  if (read_sector(0, sector_buffer) < 0) {
    print("boot: superblock read failed\n");
    return;
  }
  if (!bytes_eq(sector_buffer, "WUFS0002", 8)) {
    print("boot: unsupported WUFS disk\n");
    return;
  }
  uint32_t sectors = le32(sector_buffer + 8), inodes = le32(sector_buffer + 12),
           ibitmap = le32(sector_buffer + 16),
           bbitmap = le32(sector_buffer + 20), blen = le32(sector_buffer + 24),
           itable = le32(sector_buffer + 28), ilen = le32(sector_buffer + 32),
           data = le32(sector_buffer + 36), root = le32(sector_buffer + 40);
  if (sectors > capacity || sectors > WUFS_MAX_SECTORS ||
      inodes != WUFS_INODES || root || ibitmap != 1163 ||
      bbitmap != ibitmap + 1 || blen != (sectors + 4095) / 4096 ||
      itable != bbitmap + blen || ilen != 20 || data != itable + ilen ||
      data >= sectors || le32(sector_buffer + 44) != 1 ||
      le32(sector_buffer + 48) != 1162 || le32(sector_buffer + 52) != 1152) {
    print("boot: invalid WUFS layout\n");
    return;
  }
  for (unsigned i = 56; i < SECTOR_SIZE; ++i)
    if (sector_buffer[i]) {
      print("boot: invalid superblock padding\n");
      return;
    }
  /* The loader is read-only; a pending redo transaction requires kernel replay.
   */
  if (read_sector(1, sector_buffer) < 0) {
    print("boot: journal read failed\n");
    return;
  }
  for (unsigned i = 0; i < SECTOR_SIZE; ++i)
    if (sector_buffer[i]) {
      print("boot: pending or damaged journal\n");
      return;
    }
  Inode kernel;
  if (find_kernel(itable, data, sectors, &kernel) < 0) {
    print("boot: /boot/kernel.elf missing\n");
    return;
  }
  uintptr_t entry;
  if (load_elf(&kernel, data, sectors, &entry) < 0) {
    print("boot: invalid kernel ELF\n");
    return;
  }
  __asm__ volatile("fence.i" ::: "memory");
  regs[0x70 / 4] = 0;
  unsigned spins = 1000000;
  while (regs[0x70 / 4] && spins--)
    __asm__ volatile("nop");
  if (regs[0x70 / 4]) {
    print("boot: virtio reset failed\n");
    return;
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  print("boot: entering kernel\n");
  __asm__ volatile("mv a0, %0\n mv a1, %1\n jr %2" ::"r"(hartid), "r"(fdt),
                   "r"(entry)
                   : "a0", "a1", "memory");
  __builtin_unreachable();
}
