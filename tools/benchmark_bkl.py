#!/usr/bin/env python3
"""Measure Big Kernel Lock contention and syscall throughput across 1..4 harts."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from test_thread_stage_a import Guest, ROOT

LOCK_RE = re.compile(r'Kernel lock: acquires=(\d+) contentions=(\d+) wait_us=(\d+)')
RESULT_RE = re.compile(
    r'BKL BENCH RESULT threads=(\d+) iters=(\d+) total_calls=(\d+) ticks=(\d+)'
)


def parse_lock_stats(text):
    match = LOCK_RE.search(text)
    if not match:
        raise AssertionError(f'Missing Kernel lock stats in output:\n{text}')
    return tuple(map(int, match.groups()))


def tool(name, *arguments):
    subprocess.run(
        [sys.executable, '-B', str(ROOT / 'tools' / name), *map(str, arguments)],
        check=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iters', type=int, default=20000, help='getpid() calls per worker thread')
    parser.add_argument('--harts', type=int, nargs='+', default=[1, 2, 3, 4], help='hart counts to test')
    args = parser.parse_args()

    qemu = shutil.which('qemu-system-riscv64')
    if not qemu:
        raise RuntimeError('qemu-system-riscv64 is required')

    targets = ['all', 'build/kernel-disk.elf', 'build/boot-flash.img', 'build/bkl_bench_probe.elf']
    subprocess.run(['make', '-j4', *targets], cwd=ROOT, check=True)

    folder = Path(tempfile.mkdtemp(prefix='bkl-bench-', dir=ROOT / 'build'))
    disk = folder / 'disk.img'
    swap = folder / 'swap.img'
    tool(
        'make_disk.py',
        '--wufs3',
        '--image', disk,
        '--kernel', ROOT / 'build/kernel-disk.elf',
        '--install', ROOT / 'build/bkl_bench_probe.elf', '/bin/bkl-bench.elf',
    )
    tool('make_swap.py', '--image', swap)

    print(f'Benchmark logs: {folder}', flush=True)
    print(
        f'{"harts":>5} {"threads":>7} {"calls":>8} {"wall_ms":>9} '
        f'{"calls/s":>10} {"acquires":>10} {"contends":>10} {"contend%":>9} {"wait_ms":>9}',
        flush=True,
    )

    for harts in args.harts:
        guest = Guest(qemu, folder, disk, swap, f'bkl-{harts}', flash=False, harts=harts)
        try:
            guest.shell('trace off')
            before = parse_lock_stats(guest.shell('ps', 'Kernel lock:'))
            out = guest.shell(
                f'run /bin/bkl-bench.elf {harts} {args.iters}',
                'BKL BENCH RESULT',
                timeout=90,
            )
            after = parse_lock_stats(guest.shell('ps', 'Kernel lock:'))
        finally:
            guest.close()

        match = RESULT_RE.search(out)
        if not match:
            raise AssertionError(f'Missing benchmark result:\n{out}')
        threads, _iters, total_calls, ticks = map(int, match.groups())
        acquires = after[0] - before[0]
        contentions = after[1] - before[1]
        wait_us = after[2] - before[2]
        wall_ms = ticks / 10000.0
        calls_per_sec = total_calls / (ticks / 10000000.0) if ticks > 0 else 0.0
        contend_pct = (100.0 * contentions / acquires) if acquires > 0 else 0.0
        print(
            f'{harts:5d} {threads:7d} {total_calls:8d} {wall_ms:9.2f} '
            f'{calls_per_sec:10.0f} {acquires:10d} {contentions:10d} '
            f'{contend_pct:8.1f}% {wait_us / 1000.0:9.2f}',
            flush=True,
        )


if __name__ == '__main__':
    main()
