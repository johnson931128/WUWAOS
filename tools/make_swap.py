#!/usr/bin/env python3
"""Create a dedicated WUWAOS swap disk; never reformat an existing file."""
import argparse
import pathlib
import struct

PAGE = 4096
MAGIC = b"WUSWAP01"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=pathlib.Path)
    parser.add_argument("--slots", type=int, default=4095, help="Number of 4 KiB slots (1..4096)")
    args = parser.parse_args()
    if not 1 <= args.slots <= 4096:
        parser.error("--slots must be in 1..4096")
    size = (args.slots + 1) * PAGE
    header = MAGIC + struct.pack("<II", PAGE, args.slots)
    if args.image.exists():
        with args.image.open("rb") as image:
            if image.read(len(header)) != header or args.image.stat().st_size != size:
                raise SystemExit("Existing swap image has an unsupported header or size")
    else:
        args.image.parent.mkdir(parents=True, exist_ok=True)
        with args.image.open("xb") as image:
            image.write(header)
            image.truncate(size)
    print(f"Swap disk ready: {args.image} ({args.slots} slots, {PAGE} bytes/page)")


if __name__ == "__main__":
    main()
