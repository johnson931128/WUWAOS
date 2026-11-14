#!/usr/bin/env python3
"""Verify the xv6 roadmap changes through UART on fresh QEMU disk images."""

import hashlib
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

from make_disk import WufsImage
from test_thread_stage_a import Guest, ROOT


def tool(name, *arguments):
    subprocess.run([sys.executable, '-B', str(ROOT / 'tools' / name), *map(str, arguments)], check=True)



def check_wufs(qemu, folder, disk, swap):
    guest = Guest(qemu, folder, disk, swap, 'wufs', flash=True)
    try:
        guest.shell('run /bin/exec-fd.elf', 'status=0')
        guest.shell('run /bin/exec-fd.elf hello world', 'PASS: argv, extended BSS')
        guest.shell('cat /keep', 'preserved')
        guest.shell('run /bin/wufs.elf', 'PASS: hard link, unlink, last close, console node, batch writes')
        guest.shell('ln /keep /keep2', 'ln=0')
        before = guest.shell('ps', 'write_batches=1 batched_writes=2')
        guest.shell('cat /keep2', 'preserved')
        guest.shell('cat /keep2', 'preserved')
        after = guest.shell('ps', 'Free TCB slots: 31')
        stats = r'WUFS cache hits=(\d+) misses=(\d+)'
        old_hits, old_misses = map(int, re.search(stats, before).groups())
        hits, misses = map(int, re.search(stats, after).groups())
        assert hits - old_hits > misses - old_misses, after
        assert re.search(r'IRQ \d+ UART handled=[1-9]', after), after
    finally:
        guest.close()


def check_replay(qemu, folder, disk, swap):
    guest = Guest(qemu, folder, disk, swap, 'wufs-reboot', flash=True)
    try:
        guest.shell('cat /keep2', 'preserved')
        guest.shell('cat /batch1', 'batch')
        guest.shell('cat /batch2', 'batch')
        guest.shell('echo reboot-console > /dev/console', '\r\nreboot-console')
    finally:
        guest.close()

    recovery = folder / 'replay.img'
    image = bytearray(disk.read_bytes())
    fs = WufsImage(image)
    inode = fs.read_inode(fs.find('/batch1'))
    sector = fs.inode_blocks(inode)[0]
    payload = bytearray(image[sector * 512:(sector + 1) * 512])
    payload[:5] = b'redo!'
    table = struct.pack('<I', sector) + bytes(9 * 512 - 4)
    fields = struct.pack('<III', 2, 1, 1)
    checksum = zlib.crc32(fields + table + payload)
    header = b'WUJNL002' + fields + struct.pack('<I', checksum)
    image[512:1024] = header + bytes(512 - len(header))
    image[1024:11 * 512] = table
    image[11 * 512:12 * 512] = payload
    recovery.write_bytes(image)
    guest = Guest(qemu, folder, recovery, swap, 'replay')
    try:
        guest.wait('Replayed committed WUFS journal')
        guest.shell('cat /batch1', 'redo!')
        guest.shell('cat /batch2', 'batch')
    finally:
        guest.close()
    assert not any(recovery.read_bytes()[512:1024])


def check_harts(qemu, folder, disk, swap):
    for harts in (2, 3):
        for flash in (False, True):
            label = f'{"flash" if flash else "direct"}-{harts}'
            guest = Guest(qemu, folder, disk, swap, label, flash=flash, harts=harts)
            try:
                guest.shell('ps', f'Online hart mask=0x{(1 << harts) - 1:016x}')
                guest.shell('run /bin/smp.elf', 'PASS: syscall loop survived timer slices', timeout=90)
                if not flash:
                    guest.shell('run /bin/stage-a.elf', 'PASS: THREAD STAGE A')
                guest.shell('echo smp-ok | cat', 'smp-ok')
                stats = guest.shell('ps', 'Free TCB slots: 31')
                assert re.search(r'kernel_preemptions=[1-9]', stats), stats
                print(f'PASS: {label} shared VM, concurrent dispatch and kernel timer', flush=True)
            finally:
                guest.close()


def main():
    qemu = shutil.which('qemu-system-riscv64')
    if not qemu:
        raise RuntimeError('qemu-system-riscv64 is required')
    targets = ['all', 'build/kernel-disk.elf', 'build/boot-flash.img', 'build/exec_fd_probe.elf',
               'build/wufs_links_probe.elf', 'build/smp_probe.elf', 'build/thread-stage-a-probe.elf']
    subprocess.run(['make', '-j4', *targets], cwd=ROOT, check=True)
    folder = Path(tempfile.mkdtemp(prefix='kernel-cli-', dir=ROOT / 'build'))
    print(f'CLI logs and disk images: {folder}', flush=True)
    original, disk, swap = [folder / name for name in ('old.img', 'disk.img', 'swap.img')]
    keep = folder / 'keep'
    keep.write_bytes(b'preserved')
    install = ['--install', keep, '/keep']
    for source, target in [('exec_fd_probe', 'exec-fd'), ('wufs_links_probe', 'wufs'),
                           ('smp_probe', 'smp'), ('thread-stage-a-probe', 'stage-a')]:
        install += ['--install', ROOT / f'build/{source}.elf', f'/bin/{target}.elf']
    tool('make_disk.py', '--image', original, '--kernel', ROOT / 'build/kernel-disk.elf', *install)
    digest = hashlib.sha256(original.read_bytes()).digest()
    tool('make_disk.py', '--image', disk, '--upgrade-from', original)
    assert hashlib.sha256(original.read_bytes()).digest() == digest
    tool('make_swap.py', '--image', swap)
    check_wufs(qemu, folder, disk, swap)
    check_replay(qemu, folder, disk, swap)
    check_harts(qemu, folder, disk, swap)
    print('PASS: CLI roadmap regression', flush=True)


if __name__ == '__main__':
    main()
