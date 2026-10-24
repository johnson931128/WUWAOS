#!/usr/bin/env python3
"""Create or refresh the WUFS QEMU disk image while preserving user files."""

import argparse
import math
import pathlib
import struct

SECTOR = 512
IMAGE_SIZE = 16 * 1024 * 1024
INODE_COUNT = 128
INODE_SIZE = 80
DIRECT = 8
INDIRECT = 8
NAME_SIZE = 24
MAGIC = b"WUFS0001"


class WufsImage:
    def __init__(self, image: bytearray):
        self.image = image
        self.sectors, self.inodes, self.ibitmap, self.bbitmap, self.bbitmap_len, self.itable, self.itable_len, self.data_start, self.root = struct.unpack_from("<9I", image, 8)

    @classmethod
    def format(cls):
        sectors = IMAGE_SIZE // SECTOR
        bbitmap_len = math.ceil(sectors / (SECTOR * 8))
        itable = 2 + bbitmap_len
        itable_len = INODE_COUNT * INODE_SIZE // SECTOR
        data_start = itable + itable_len
        image = bytearray(IMAGE_SIZE)
        image[:8] = MAGIC
        struct.pack_into("<9I", image, 8, sectors, INODE_COUNT, 1, 2, bbitmap_len, itable, itable_len, data_start, 0)
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
            raw_name = contents[at:at + NAME_SIZE].split(b"\0", 1)[0]
            child = struct.unpack_from("<I", contents, at + NAME_SIZE)[0]
            entries.append((raw_name.decode(), child))
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
        self.write_data(inode_number, kernel)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=pathlib.Path)
    parser.add_argument("--kernel", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if args.image.exists():
        image = bytearray(args.image.read_bytes())
        if image[:8] == MAGIC:
            fs = WufsImage(image)
        else:
            fs = WufsImage.format()
    else:
        args.image.parent.mkdir(parents=True, exist_ok=True)
        fs = WufsImage.format()
    kernel = args.kernel.read_bytes()
    fs.add_kernel(kernel)
    args.image.write_bytes(fs.image)
    print(f"WUFS disk ready: {args.image} ({len(fs.image)} bytes), /boot/kernel.elf ({len(kernel)} bytes)")


if __name__ == "__main__":
    main()
