#!/usr/bin/env python3
"""Interactive filesystem checks using fresh disks and the bundled Windows tools."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PORTABLE = ROOT / 'build/portable/msys64'
IMAGE_SIZE = 16 * 1024 * 1024
SECTOR_SIZE = 512
MAGIC = b'WUFS0001'
EXPECTED = {
    'valid': '[fs] Mounted existing WUFS volume',
    'blank': '[fs] Initializing blank disk as WUFS',
    'damaged': '[fs] Unknown or damaged filesystem on nonblank disk; refusing format',
    'tail': '[fs] Unknown or damaged filesystem on nonblank disk; refusing format',
}


def run_host_tool(name, *arguments):
    subprocess.run([sys.executable, '-B', str(ROOT / 'tools' / name), *map(str, arguments)],
                   cwd=ROOT, check=True)


def digest(image):
    with image.open('rb') as disk:
        return hashlib.file_digest(disk, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', nargs='?', choices=tuple(EXPECTED), default='valid')
    parser.add_argument('--reverse-disks', action='store_true',
                        help='attach swap first to check discovery and IRQ routes without fixed disk slots')
    args = parser.parse_args()
    bash = PORTABLE / 'usr/bin/bash.exe'
    qemu = PORTABLE / 'ucrt64/bin/qemu-system-riscv64.exe'
    if not bash.is_file() or not qemu.is_file():
        parser.error('The complete portable environment is required.')

    # Build only the kernel artifacts. The disk-image target would prepare the
    # normal user disk and would also hide the kernel's blank-disk test path.
    subprocess.run([str(bash), '--noprofile', '--norc', '-c',
                    'export PATH=/ucrt64/bin:/usr/bin:/bin; make -j4 all build/kernel-disk.elf'],
                   cwd=ROOT, check=True)

    # A fresh directory per invocation makes every blank case repeatable and
    # keeps all test writes away from build/wuwaos.img and the user's swap disk.
    folder = Path(tempfile.mkdtemp(prefix=f'fs-check-{args.case}-', dir=ROOT / 'build'))
    image = folder / 'disk.img'
    swap = folder / 'swap.img'
    print(f'Test files: {folder}', flush=True)
    if args.case in ('valid', 'damaged'):
        run_host_tool('make_disk.py', '--image', image, '--kernel', ROOT / 'build/kernel-disk.elf')
        if args.case == 'damaged':
            # Destroy only the header; the rest of the valid WUFS still has data.
            with image.open('r+b') as disk:
                disk.write(bytes(SECTOR_SIZE))
    else:
        image.write_bytes(bytes(IMAGE_SIZE))
        if args.case == 'tail':
            # The last byte forces a complete scan; sector 0 alone looks blank.
            with image.open('r+b') as disk:
                disk.seek(-1, 2)
                disk.write(b'\xa5')
    run_host_tool('make_swap.py', '--image', swap)
    before = digest(image)
    print(f'Expected kernel message: {EXPECTED[args.case]}', flush=True)
    print('Use help/ls only; do not create or modify files during this check.', flush=True)
    print('Press Ctrl+A, release, then X to exit QEMU and check the disk hash.', flush=True)

    # Intentionally omit -snapshot: an overlay would hide unwanted guest writes
    # and could make a destructive formatter falsely pass the preservation check.
    qemu_arguments = [
        str(qemu), '-machine', 'virt', '-smp', '1', '-m', '128M', '-nographic',
        '-bios', 'default', '-kernel', str(ROOT / 'build/kernel.elf'),
        '-global', 'virtio-mmio.force-legacy=false',
    ]
    disks = [(image, 'fs'), (swap, 'swap')]
    if args.reverse_disks:
        disks.reverse()
    # Keep the same disk contents while changing VirtIO attachment/IRQ order.
    for disk, identifier in disks:
        qemu_arguments.extend(['-drive', f'file={disk},if=none,format=raw,id={identifier}',
                               '-device', f'virtio-blk-device,drive={identifier}'])
    subprocess.run(qemu_arguments, cwd=ROOT, check=True)

    after = digest(image)
    print(f'SHA256 before: {before}')
    print(f'SHA256 after:  {after}')
    # Hash checks cover disk mutations. The user separately confirms the boot
    # message above, so a passing hash is not presented as proof of a full mount.
    if args.case == 'blank':
        with image.open('rb') as disk:
            formatted = disk.read(len(MAGIC)) == MAGIC
        passed = before != after and formatted
        result = 'blank disk changed and now has the WUFS signature'
    else:
        passed = before == after
        result = 'disk contents unchanged'
    print(f'{"PASS" if passed else "FAIL"}: {result}; also confirm the expected boot message.')
    print(f'Test files retained: {folder}')
    return 0 if passed else 1


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print('Check interrupted; test files are retained.', file=sys.stderr)
        sys.exit(130)
