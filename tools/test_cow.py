#!/usr/bin/env python3
"""Run COW regression cases on fresh disks, including small and read-only swap."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from test_kernel_cli import tool
from test_thread_stage_a import Guest, ROOT

PROBE = ROOT / 'build/cow_probe.elf'
KERNEL = ROOT / 'build/kernel.elf'


def build():
    subprocess.run(['make', '-j4', 'all', 'build/kernel-disk.elf', str(PROBE.relative_to(ROOT))],
                   cwd=ROOT, check=True)


def images(folder, label, slots=4095):
    disk, swap = (folder / f'{label}-{name}.img' for name in ('disk', 'swap'))
    tool('make_disk.py', '--image', disk, '--kernel', ROOT / 'build/kernel-disk.elf',
         '--install', PROBE, '/bin/cow.elf')
    tool('make_swap.py', '--image', swap, '--slots', slots)
    return disk, swap


def counters(output, marker):
    lines = output.split(marker + '\r\n', 1)[1].splitlines()
    assert lines[0].startswith('VM FIFO:'), lines
    return {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)', '\n'.join(lines[:5]))}


def check_cleanup(guest):
    start = len(guest.data)
    guest.send('exit\r')
    guest.wait('[exit] PID=1 status=0', start)
    symbols = subprocess.check_output(['llvm-nm', '-n', KERNEL], text=True)
    values = {}
    for name, width in (('resident', 'g'), ('used', 'w'), ('free_count', 'g')):
        address, = re.findall(rf'(?m)^([\da-f]+) b {name}$', symbols)
        value = guest.command('human-monitor-command', {'command-line': f'xp /1{width}x 0x{address}'})
        values[name] = int(value.split(':')[-1].strip(), 16)
    assert values['resident'] == values['used'] == 0, values
    return values['free_count']


def run_case(qemu, folder, case, harts=1, slots=4095, readonly=False):
    label = f'{case}-{harts}'
    disk, swap = images(folder, label, slots)
    control = Guest(qemu, folder, disk, swap, f'{label}-control', harts=harts, swap_readonly=readonly)
    try:
        expected_free = check_cleanup(control)
    finally:
        control.close()
    guest = Guest(qemu, folder, disk, swap, label, harts=harts, swap_readonly=readonly)
    try:
        guest.shell('ps', f'Online hart mask=0x{(1 << harts) - 1:016x}')
        output = guest.shell(f'run /bin/cow.elf {case}', 'status=0', timeout=90)
        assert 'PASS: COW ' in output, output
        if case == 'isolation':
            sharing = counters(output, 'COW SHARING')
            before = counters(output, 'COW BEFORE REUSE')
            after = counters(output, 'COW AFTER REUSE')
            assert sharing['shared_frames'] >= 6 and sharing['fork_shared'] >= 6, sharing
            assert before['cow_faults'] > sharing['cow_faults'], before
            assert before['cow_copyouts'] > sharing['cow_copyouts'], before
            assert before['cow_copies'] > sharing['cow_copies'], before
            assert after['cow_reuses'] > before['cow_reuses'], after
            assert after['cow_copies'] == before['cow_copies'], (before, after)
        elif case in ('pressure', 'eio'):
            pressure = counters(output, 'COW PRESSURE')
            if readonly:
                assert pressure['shared_rollbacks'] > 0 and pressure['io_errors'] > 0, pressure
                assert pressure['swap_slots'] == 0, pressure
            else:
                assert pressure['shared_evictions'] > 0 and pressure['swap_out'] > 0, pressure
                assert any(int(value) > 0 for value in re.findall(r'swap_in=(\d+)', output)), output
        elif case == 'rollback':
            before = counters(output, 'COW BEFORE FORK FAILURE')
            after = counters(output, 'COW AFTER FORK FAILURE')
            for key in ('shared_frames', 'fork_shared', 'free_frames', 'swap_slots'):
                assert before[key] == after[key], (key, before, after)
            assert after['swap_fork_copies'] > before['swap_fork_copies'], (before, after)
        elif case == 'no-frame':
            before = counters(output, 'COW BEFORE NO FRAME')
            after = counters(output, 'COW AFTER NO FRAME')
            assert after['cow_copyouts'] == before['cow_copyouts'] + 1, (before, after)
            for key in ('cow_copies', 'shared_frames', 'free_frames', 'swap_slots'):
                assert before[key] == after[key], (key, before, after)
        assert check_cleanup(guest) == expected_free, 'physical frame count differed from clean shutdown'
        print(f'PASS: {label}, including physical frame and swap slot reclamation', flush=True)
    finally:
        guest.close()


def main():
    qemu = shutil.which('qemu-system-riscv64')
    if not qemu:
        raise RuntimeError('qemu-system-riscv64 is required')
    build()
    folder = Path(tempfile.mkdtemp(prefix='cow-regression-', dir=ROOT / 'build'))
    print(f'COW logs: {folder}', flush=True)
    run_case(qemu, folder, 'isolation')
    run_case(qemu, folder, 'pressure')
    run_case(qemu, folder, 'eio', readonly=True)
    run_case(qemu, folder, 'no-frame', readonly=True)
    run_case(qemu, folder, 'rollback', slots=24)
    for harts in (2, 3):
        run_case(qemu, folder, 'stores', harts=harts)


if __name__ == '__main__':
    main()
