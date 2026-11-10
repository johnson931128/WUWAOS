#!/usr/bin/env python3
"""Create or refresh the WUFS QEMU disk image while preserving user files."""

import argparse
import math
import pathlib
import struct
import os
import tempfile

SECTOR = 512
IMAGE_SIZE = 16 * 1024 * 1024
INODE_COUNT = 128
INODE_SIZE = 80
DIRECT = 8
INDIRECT = 8
NAME_SIZE = 24
MAGIC = b"WUFS0002"
LEGACY_MAGIC = b"WUFS0001"
JOURNAL_START = 1
JOURNAL_CAPACITY = 1152
JOURNAL_TARGET_SECTORS = 9
JOURNAL_SECTORS = 1 + JOURNAL_TARGET_SECTORS + JOURNAL_CAPACITY
JOURNAL_HEADER_OFFSET = JOURNAL_START * SECTOR


class WufsImage:
    def __init__(self, image: bytearray):
        self.image = image
        self.sectors, self.inodes, self.ibitmap, self.bbitmap, self.bbitmap_len, self.itable, self.itable_len, self.data_start, self.root = struct.unpack_from("<9I", image, 8)
        self.validate_layout()

    def validate_layout(self):
        sectors = len(self.image) // SECTOR
        bbitmap_len = math.ceil(self.sectors / (SECTOR * 8))
        expected_ibitmap = JOURNAL_START + JOURNAL_SECTORS
        expected_bbitmap = expected_ibitmap + 1
        expected_itable = expected_bbitmap + bbitmap_len
        expected_data = expected_itable + INODE_COUNT * INODE_SIZE // SECTOR
        journal = struct.unpack_from("<3I", self.image, 8 + 9 * 4)
        if (len(self.image) % SECTOR or self.sectors != sectors or self.inodes != INODE_COUNT or
                self.ibitmap != expected_ibitmap or self.bbitmap != expected_bbitmap or
                self.bbitmap_len != bbitmap_len or self.itable != expected_itable or
                self.itable_len != INODE_COUNT * INODE_SIZE // SECTOR or
                self.data_start != expected_data or self.root != 0 or self.data_start >= self.sectors or
                self.sectors > 32768 or journal != (JOURNAL_START, JOURNAL_SECTORS, JOURNAL_CAPACITY) or
                any(self.image[14 * 4:SECTOR])):
            raise RuntimeError("invalid or unsupported WUFS0002 layout")
        if any(self.image[JOURNAL_HEADER_OFFSET:JOURNAL_HEADER_OFFSET + SECTOR]):
            raise RuntimeError("journal header is nonzero (pending or damaged); boot WUFS0002 for recovery before host updates")

    def validate_metadata(self):
        used_blocks = set()
        allocated_inodes = set()
        for number in range(self.inodes):
            allocated = self.bit(self.ibitmap, number)
            inode = self.read_inode(number)
            kind, size = inode[:2]
            blocks = list(inode[2:10])
            tables = list(inode[10:18])
            reserved = inode[18:20]
            if not allocated:
                if kind or size or any(blocks) or any(tables) or any(reserved):
                    raise RuntimeError(f"unused inode {number} contains metadata")
                continue
            allocated_inodes.add(number)
            if any(reserved) or kind not in (1, 2) or size > (DIRECT + INDIRECT * 128) * SECTOR:
                raise RuntimeError(f"invalid inode {number}")
            if kind == 1 and size % (NAME_SIZE + 4):
                raise RuntimeError(f"directory inode {number} has a partial entry")
            for block in blocks + tables:
                if block:
                    self._claim_block(block, used_blocks)
            for table in tables:
                if table:
                    pointers = struct.unpack_from("<128I", self.image, table * SECTOR)
                    for block in pointers:
                        if block:
                            self._claim_block(block, used_blocks)
            required = math.ceil(size / SECTOR)
            logical = blocks[:]
            for table in tables:
                logical.extend(struct.unpack_from("<128I", self.image, table * SECTOR) if table else [0] * 128)
            if any(block == 0 for block in logical[:required]):
                raise RuntimeError(f"inode {number} is missing a required data block")
        if 0 not in allocated_inodes or self.read_inode(0)[0] != 1:
            raise RuntimeError("root inode is missing or not a directory")
        for sector in range(self.data_start):
            if not self.bit(self.bbitmap, sector):
                raise RuntimeError("reserved layout sector is marked free")
        for sector in range(self.data_start, self.sectors):
            if self.bit(self.bbitmap, sector) != (sector in used_blocks):
                raise RuntimeError(f"block bitmap disagrees with inode ownership at sector {sector}")

        reached = {0}
        queue = [0]
        while queue:
            parent = queue.pop(0)
            entries = self.read_dir(parent)
            if len(entries) < 2 or entries[0] != (".", parent):
                raise RuntimeError(f"directory inode {parent} has invalid dot entries")
            if parent == 0 and entries[1] != ("..", 0):
                raise RuntimeError("root parent entry is invalid")
            directory_names = set()
            for name, child in entries[2:]:
                if child not in allocated_inodes or name in ("", ".", "..") or "/" in name:
                    raise RuntimeError(f"invalid directory entry in inode {parent}")
                if name in directory_names:
                    raise RuntimeError(f"directory inode {parent} contains duplicate name {name!r}")
                directory_names.add(name)
                if child in reached:
                    raise RuntimeError("directory tree contains a cycle or multiply linked inode")
                reached.add(child)
                if self.read_inode(child)[0] == 1:
                    child_entries = self.read_dir(child)
                    if len(child_entries) < 2 or child_entries[1][0] != ".." or child_entries[1][1] != parent:
                        raise RuntimeError(f"directory inode {child} has an invalid parent entry")
                    queue.append(child)
        if reached != allocated_inodes:
            raise RuntimeError("directory tree does not reach every allocated inode")

    def _claim_block(self, block, used_blocks):
        if block < self.data_start or block >= self.sectors or not self.bit(self.bbitmap, block) or block in used_blocks:
            raise RuntimeError(f"invalid or multiply owned block {block}")
        used_blocks.add(block)

    @classmethod
    def format(cls):
        sectors = IMAGE_SIZE // SECTOR
        bbitmap_len = math.ceil(sectors / (SECTOR * 8))
        ibitmap = JOURNAL_START + JOURNAL_SECTORS
        bbitmap = ibitmap + 1
        itable = bbitmap + bbitmap_len
        itable_len = INODE_COUNT * INODE_SIZE // SECTOR
        data_start = itable + itable_len
        image = bytearray(IMAGE_SIZE)
        image[:8] = MAGIC
        struct.pack_into("<9I", image, 8, sectors, INODE_COUNT, ibitmap, bbitmap, bbitmap_len, itable, itable_len, data_start, 0)
        struct.pack_into("<3I", image, 8 + 9 * 4, JOURNAL_START, JOURNAL_SECTORS, JOURNAL_CAPACITY)
        fs = cls(image)
        for sector in range(data_start):
            fs.set_block(sector, True)
        fs.set_inode(0, True)
        fs.write_inode(0, 1, 0, [], [])
        fs.mkdir_fixed(0, 0, ".", "..")
        root_entries = [(".", 0), ("..", 0)]
        child = 1
        for name in ("bin", "boot", "dev", "home"):
            fs.make_dir(child, 0)
            root_entries.append((name, child))
            child += 1
        fs.write_dir(0, root_entries)
        fs.make_dir(child, 4)
        fs.write_dir(4, [(".", 4), ("..", 0), ("user", child)])
        fs.validate_metadata()
        return fs

    def inode_offset(self, number):
        return self.itable * SECTOR + number * INODE_SIZE

    def read_inode(self, number):
        return struct.unpack_from("<20I", self.image, self.inode_offset(number))

    def write_inode(self, number, kind, size, direct, indirect):
        values = [kind, size] + list(direct[:DIRECT]) + [0] * (DIRECT - len(direct))
        values += list(indirect[:INDIRECT]) + [0] * (INDIRECT - len(indirect)) + [0, 0]
        struct.pack_into("<20I", self.image, self.inode_offset(number), *values)

    def bit(self, start, number):
        offset = start * SECTOR + number // 8
        return bool(self.image[offset] & (1 << (number % 8)))

    def set_bit(self, start, number, value):
        offset = start * SECTOR + number // 8
        mask = 1 << (number % 8)
        self.image[offset] = self.image[offset] | mask if value else self.image[offset] & ~mask

    def set_inode(self, number, value):
        self.set_bit(self.ibitmap, number, value)

    def set_block(self, number, value):
        self.set_bit(self.bbitmap, number, value)

    def alloc_inode(self):
        for number in range(self.inodes):
            if not self.bit(self.ibitmap, number):
                self.set_inode(number, True)
                return number
        raise RuntimeError("WUFS inode table is full")

    def alloc_block(self):
        for number in range(self.data_start, self.sectors):
            if not self.bit(self.bbitmap, number):
                self.set_block(number, True)
                return number
        raise RuntimeError("WUFS disk is full")

    def inode_blocks(self, inode):
        direct = list(inode[2:10])
        indirect_blocks = []
        for table in inode[10:18]:
            if table:
                pointers = struct.unpack_from("<128I", self.image, table * SECTOR)
                indirect_blocks.extend(block for block in pointers if block)
        return [block for block in direct if block] + indirect_blocks

    def read_dir(self, number):
        inode = self.read_inode(number)
        entries = []
        contents = b"".join(self.image[block * SECTOR:(block + 1) * SECTOR]
                             for block in self.inode_blocks(inode))
        for at in range(0, inode[1], NAME_SIZE + 4):
            name_field = contents[at:at + NAME_SIZE]
            if b"\0" not in name_field:
                raise RuntimeError(f"directory inode {number} has an unterminated name")
            raw_name = name_field.split(b"\0", 1)[0]
            if not raw_name or len(raw_name) >= NAME_SIZE:
                raise RuntimeError(f"directory inode {number} has an invalid name")
            child = struct.unpack_from("<I", contents, at + NAME_SIZE)[0]
            try:
                decoded = raw_name.decode()
            except UnicodeDecodeError as error:
                raise RuntimeError(f"directory inode {number} contains invalid UTF-8") from error
            entries.append((decoded, child))
        return entries

    def write_data(self, number, data):
        old = self.read_inode(number)
        for block in self.inode_blocks(old):
            self.set_block(block, False)
            self.image[block * SECTOR:(block + 1) * SECTOR] = bytes(SECTOR)
        for table in old[10:18]:
            if table:
                self.set_block(table, False)
                self.image[table * SECTOR:(table + 1) * SECTOR] = bytes(SECTOR)
        data_blocks = []
        for at in range(0, len(data), SECTOR):
            block = self.alloc_block()
            data_blocks.append(block)
            chunk = data[at:at + SECTOR]
            self.image[block * SECTOR:block * SECTOR + len(chunk)] = chunk
        direct = data_blocks[:DIRECT]
        indirect = []
        remaining = data_blocks[DIRECT:]
        while remaining:
            table = self.alloc_block()
            indirect.append(table)
            batch = remaining[:128]
            struct.pack_into("<128I", self.image, table * SECTOR, *(batch + [0] * (128 - len(batch))))
            remaining = remaining[128:]
        if len(indirect) > INDIRECT:
            raise RuntimeError("file exceeds WUFS maximum size")
        self.write_inode(number, old[0], len(data), direct, indirect)

    def write_dir(self, number, entries):
        payload = bytearray()
        for name, child in entries:
            encoded = name.encode()
            if len(encoded) >= NAME_SIZE:
                raise RuntimeError("directory name too long")
            payload += encoded + bytes(NAME_SIZE - len(encoded)) + struct.pack("<I", child)
        self.write_data(number, payload)

    def mkdir_fixed(self, number, parent, name, parent_name):
        self.write_dir(number, [(name, number), (parent_name, parent)])

    def make_dir(self, number, parent):
        self.set_inode(number, True)
        self.write_inode(number, 1, 0, [], [])
        self.write_dir(number, [(".", number), ("..", parent)])

    def find(self, path):
        if not path.startswith("/"):
            raise RuntimeError("path must be absolute")
        current = self.root
        for part in (piece for piece in path.split("/") if piece):
            match = next((child for name, child in self.read_dir(current) if name == part), None)
            if match is None:
                raise FileNotFoundError(path)
            current = match
        return current

    def install_file(self, destination, data):
        if not destination.startswith("/") or destination.endswith("/"):
            raise ValueError("destination must be an absolute file path")
        parent_path, name = destination.rsplit("/", 1)
        if name in ("", ".", "..") or len(name.encode()) >= NAME_SIZE:
            raise ValueError("invalid WUFS filename")
        parent = self.find(parent_path or "/")
        if self.read_inode(parent)[0] != 1:
            raise ValueError("destination parent is not a directory")
        entries = self.read_dir(parent)
        if any(existing == name for existing, _ in entries):
            raise FileExistsError(destination)
        if len(data) > (DIRECT + INDIRECT * 128) * SECTOR:
            raise ValueError("file exceeds WUFS maximum size")
        inode = self.alloc_inode()
        self.write_inode(inode, 2, 0, [], [])
        self.write_data(inode, data)
        entries.append((name, inode))
        self.write_dir(parent, entries)

    def add_kernel(self, kernel):
        try:
            inode_number = self.find("/boot/kernel.elf")
        except FileNotFoundError:
            inode_number = self.alloc_inode()
            self.write_inode(inode_number, 2, 0, [], [])
            boot = self.find("/boot")
            entries = self.read_dir(boot)
            entries.append(("kernel.elf", inode_number))
            self.write_dir(boot, entries)
        if self.read_inode(inode_number)[0] != 2:
            raise RuntimeError("/boot/kernel.elf exists but is not a regular file")
        self.write_data(inode_number, kernel)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=pathlib.Path)
    parser.add_argument("--kernel", required=True, type=pathlib.Path)
    parser.add_argument("--install", nargs=2, action="append", default=[],
                        metavar=("HOST_FILE", "WUFS_PATH"),
                        help="install a new file; destination must not exist")
    args = parser.parse_args()
    if args.image.exists():
        image = bytearray(args.image.read_bytes())
        if image[:8] == LEGACY_MAGIC:
            if args.install:
                parser.error("WUFS0001 is preserved read-only; choose a fresh --image path for WUFS0002 installs")
            print("Legacy WUFS0001 preserved; kernel will attempt read-only validation; use a fresh DISK_IMAGE for WUFS0002")
            return
        if image[:8] == MAGIC:
            try:
                fs = WufsImage(image)
                fs.validate_metadata()
            except (OSError, RuntimeError, ValueError, struct.error) as error:
                parser.error(f"invalid WUFS0002 image; left unchanged: {error}")
        else:
            # The launcher runs this tool before the guest kernel can inspect
            # the disk. Apply the same preservation rule here: missing magic
            # does not authorize overwriting data, even if sector 0 is zeroed.
            if any(image):
                parser.error("unknown or damaged filesystem on nonblank image; left unchanged")
            fs = WufsImage.format()
    else:
        args.image.parent.mkdir(parents=True, exist_ok=True)
        fs = WufsImage.format()
    try:
        kernel = args.kernel.read_bytes()
        fs.add_kernel(kernel)
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        parser.error(f"cannot install /boot/kernel.elf: {error}")
    for source, destination in args.install:
        try:
            fs.install_file(destination, pathlib.Path(source).read_bytes())
        except (OSError, RuntimeError, ValueError) as error:
            parser.error(f"cannot install {source} -> {destination}: {error}")
    try:
        fs.validate_metadata()
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        parser.error(f"refusing to publish invalid WUFS image: {error}")
    try:
        atomic_replace(args.image, fs.image)
    except OSError as error:
        parser.error(f"cannot atomically update image {args.image}: {error}")
    for source, destination in args.install:
        print(f"Installed {source} -> {destination}")
    print(f"WUFS disk ready: {args.image} ({len(fs.image)} bytes), /boot/kernel.elf ({len(kernel)} bytes)")


def atomic_replace(path, contents):
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(contents)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        if os.name == "posix":
            directory = os.open(path.parent, os.O_RDONLY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


if __name__ == "__main__":
    main()
