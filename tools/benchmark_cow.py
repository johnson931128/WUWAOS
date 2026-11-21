#!/usr/bin/env python3
"""Compare fork workloads against the pre-COW kernel with identical guest binaries."""

import argparse
import csv
from pathlib import Path
import re
import shutil
from statistics import median
import subprocess
import tempfile

from test_cow import KERNEL, build, counters, images
from test_thread_stage_a import Guest, ROOT

BASELINE = '74fe07e84dc190edf73e78066c4c23522952d636'
WORKLOADS = {'r': 'read', 'e': 'exec', 's': 'write-one', 'a': 'write-all'}
PAGE_COUNTS = (4, 16, 48)
PAGE_BYTES = 4096


def build_baseline(folder):
    source = folder / 'baseline'
    source.mkdir()
    with tempfile.TemporaryFile() as archive:
        subprocess.run(['git', 'archive', BASELINE], cwd=ROOT, stdout=archive, check=True)
        archive.seek(0)
        subprocess.run(['tar', '-xf', '-'], cwd=source, stdin=archive, check=True)
    subprocess.run(['make', '-j4', 'all'], cwd=source, check=True)
    return source / 'build/kernel.elf'


def sample(guest, variant, pages, workload):
    output = guest.shell(f'run /bin/cow.elf bench {pages} {workload}', 'status=0')
    match = re.search(r'COW BENCH RESULT (\d+) (\d+) (\d+)', output)
    assert match, output
    resident, fork_ticks, total_ticks = map(int, match.groups())
    before, after = (counters(output, f'COW BENCH {label}') for label in ('BEFORE', 'AFTER'))

    def delta(key):
        return after.get(key, 0) - before.get(key, 0)

    swap_copies = delta('swap_fork_copies')
    swap_in = delta('swap_in') + swap_copies
    swap_out = delta('swap_out') + swap_copies
    assert swap_in == swap_out == 0, output  # Keep pressure outside this workload matrix.
    if variant == 'cow':
        assert delta('fork_shared') == resident, output
    # The eager VM allocates one user data frame per resident source page.
    # The COW VM allocates none during fork. Page-table frames are excluded.
    fork_frames = resident if variant == 'eager' else 0
    return {'variant': variant, 'heap_pages': pages, 'workload': WORKLOADS[workload],
            'source_resident': resident, 'fork_us': fork_ticks / 10, 'total_us': total_ticks / 10,
            'fork_user_frames': fork_frames, 'copied_bytes': (fork_frames + delta('cow_copies')) * PAGE_BYTES,
            'page_faults': delta('faults'), 'cow_faults': delta('cow_faults'),
            'swap_in_pages': swap_in, 'swap_out_pages': swap_out}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runs', type=int, default=5, help='measured runs of each sample')
    args = parser.parse_args()
    if args.runs < 1:
        parser.error('--runs must be positive')
    qemu = shutil.which('qemu-system-riscv64')
    if not qemu:
        parser.error('qemu-system-riscv64 is required')
    build()
    folder = Path(tempfile.mkdtemp(prefix='cow-benchmark-', dir=ROOT / 'build'))
    print(f'Benchmark files: {folder}', flush=True)
    baseline = build_baseline(folder)
    data = []
    for variant, kernel in (('eager', baseline), ('cow', KERNEL)):
        disk, swap = images(folder, variant)
        guest = Guest(qemu, folder, disk, swap, variant, kernel=kernel)
        try:
            for pages in PAGE_COUNTS:
                for workload, label in WORKLOADS.items():
                    sample(guest, variant, pages, workload)  # Warm-up, excluded from results.
                    rows = [sample(guest, variant, pages, workload) for _ in range(args.runs)]
                    data.extend(rows)
                    fork = median(row['fork_us'] for row in rows)
                    total = median(row['total_us'] for row in rows)
                    print(f'{variant:5} {pages:2} {label:9} fork={fork:8.1f} µs total={total:8.1f} µs',
                          flush=True)
        finally:
            guest.close()
    with (folder / 'samples.csv').open('w', newline='', encoding='utf-8') as output:
        writer = csv.DictWriter(output, fieldnames=list(data[0]))
        writer.writeheader()
        writer.writerows(data)


if __name__ == '__main__':
    main()
