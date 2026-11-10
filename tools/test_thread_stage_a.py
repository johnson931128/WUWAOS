#!/usr/bin/env python3
"""Build and verify stage A with fresh QEMU disks; never uses normal guest images."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
PORTABLE = ROOT / 'build/portable/msys64'


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def connect(port):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            sock = socket.create_connection(('127.0.0.1', port), timeout=1)
            sock.settimeout(None)
            return sock
        except OSError:
            time.sleep(.1)
    raise RuntimeError(f'QEMU did not open localhost port {port}')


class Guest:
    def __init__(self, qemu, folder, disk, swap, label, flash=False, gui=False):
        self.log = folder / (label + '.log')
        self.data = bytearray()
        serial_port, qmp_port = free_port(), free_port()
        while qmp_port == serial_port:
            qmp_port = free_port()
        args = [qemu, '-machine', 'virt', '-smp', '1', '-m', '128M',
                '-display', 'none', '-monitor', 'none', '-bios', 'default',
                '-serial', f'tcp:127.0.0.1:{serial_port},server=on,wait=on',
                '-qmp', f'tcp:127.0.0.1:{qmp_port},server=on,wait=off',
                '-global', 'virtio-mmio.force-legacy=false']
        if flash:
            args += ['-drive', 'if=pflash,unit=0,format=raw,readonly=on,file=build/boot-flash.img']
        else:
            args += ['-kernel', str(ROOT / 'build/kernel.elf')]
        if gui:
            for device in ['virtio-gpu-device', 'virtio-tablet-device', 'virtio-keyboard-device']:
                args += ['-device', device]
        for image, ident in [(disk, 'fs')] + ([] if flash else [(swap, 'swap')]):
            args += ['-drive', f'file={image},if=none,format=raw,id={ident},cache=writeback',
                     '-device', f'virtio-blk-device,drive={ident}']
        self.stderr = (folder / (label + '-qemu.log')).open('wb')
        self.process = subprocess.Popen(args, cwd=ROOT, stdin=subprocess.DEVNULL,
                                        stdout=self.stderr, stderr=subprocess.STDOUT)
        self.serial = self.qmp = self.qmp_file = None
        self.reader = None
        try:
            self.serial = connect(serial_port)
            self.qmp = connect(qmp_port)
            self.qmp.settimeout(5)
            self.qmp_file = self.qmp.makefile('r', encoding='utf-8')
            assert 'QMP' in json.loads(self.qmp_file.readline())
            self.command('qmp_capabilities')
            self.reader = threading.Thread(target=self.read, daemon=True)
            self.reader.start()
            self.wait('wuwaos/> ')
        except BaseException:
            self.close()
            raise

    def read(self):
        try:
            while chunk := self.serial.recv(4096):
                self.data.extend(chunk)
        except OSError:
            pass

    def command(self, name, arguments=None):
        self.qmp.sendall((json.dumps({'execute': name, 'arguments': arguments or {}}) + '\n').encode())
        while line := self.qmp_file.readline():
            response = json.loads(line)
            if 'error' in response:
                raise RuntimeError(response)
            if 'return' in response:
                return response['return']
        raise RuntimeError('QMP disconnected')

    def wait(self, marker, start=0, timeout=45):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if marker.encode() in self.data[start:]:
                return self.data[start:].decode(errors='replace')
            if self.process.poll() is not None:
                break
            time.sleep(.03)
        raise AssertionError(f'Missing {marker!r}; log: {self.log}\n' + self.data[-1600:].decode(errors='replace'))

    def send(self, text):
        for byte in text.encode():
            self.serial.sendall(bytes([byte]))
            time.sleep(.02)

    def shell(self, text, expected='', timeout=45):
        start = len(self.data)
        self.send(text + '\r')
        result = self.wait('wuwaos/> ', start, timeout)
        assert expected in result, (text, expected, result)
        assert 'FAIL:' not in result and '[panic]' not in result and '[trap] Kernel trap' not in result, result
        return result

    def close(self):
        if self.process.poll() is None:
            try:
                if self.qmp_file:
                    self.command('quit')
                else:
                    self.process.terminate()
            except (OSError, RuntimeError, ValueError):
                self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        if self.serial:
            self.serial.close()
        if self.reader:
            self.reader.join(timeout=5)
        if self.qmp_file:
            self.qmp_file.close()
        if self.qmp:
            self.qmp.close()
        self.stderr.close()
        self.log.write_bytes(self.data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gui', action='store_true', help='also exercise headless GPU/TTY creation and keyboard EOF')
    args = parser.parse_args()
    bash = PORTABLE / 'usr/bin/bash.exe'
    targets = ['all', 'build/kernel-disk.elf', 'build/boot-flash.img',
               'build/thread-stage-a-probe.elf', 'test-thread-core', 'test-kmutex']
    if bash.is_file():
        env = dict(os.environ, MSYSTEM='UCRT64', CHERE_INVOKING='1')
        subprocess.run([str(bash), '--noprofile', '--norc', '-c',
                        'export PATH=/ucrt64/bin:/usr/bin:/bin; make -j4 ' + ' '.join(targets)],
                       cwd=ROOT, env=env, check=True)
    else:
        subprocess.run(['make', '-j4', *targets], cwd=ROOT, check=True)
    bundled = PORTABLE / 'ucrt64/bin/qemu-system-riscv64.exe'
    qemu = str(bundled) if bundled.is_file() else shutil.which('qemu-system-riscv64')
    if not qemu:
        raise RuntimeError('qemu-system-riscv64 is required')
    folder = Path(tempfile.mkdtemp(prefix='thread-stage-a-', dir=ROOT / 'build'))
    disk, swap = folder / 'disk.img', folder / 'swap.img'
    for script, parameters in [
        ('make_disk.py', ['--image', disk, '--kernel', ROOT / 'build/kernel-disk.elf',
                          '--install', ROOT / 'build/thread-stage-a-probe.elf', '/bin/stage-a.elf']),
        ('make_swap.py', ['--image', swap])]:
        subprocess.run([sys.executable, '-B', str(ROOT / 'tools' / script), *map(str, parameters)], check=True)
    print(f'Test logs and fresh images: {folder}', flush=True)
    guest = Guest(qemu, folder, disk, swap, 'direct')
    try:
        guest.shell('pcb 1', 'process_state=Live TID=1')
        result = guest.shell('run /bin/stage-a.elf', 'PASS: THREAD STAGE A', timeout=90)
        preemptions = re.findall(r'preemptions=(\d+)', result)
        assert len(preemptions) >= 2 and all(int(n) > 0 for n in preemptions[:2]), preemptions
        result = guest.shell('ps', 'Free TCB slots: 31')
        assert 'Free PCB slots: 15' in result
        start = len(guest.data)
        guest.send('workstate\r')
        guest.wait('q: return to shell', start)
        guest.send('q')
        guest.wait('wuwaos/> ', start)
        guest.shell('ps', 'Free TCB slots: 31')
        print('PASS: QEMU lifecycle, waits/cancellation, preemption, VM and raw TTY', flush=True)
    finally:
        guest.close()
    guest = Guest(qemu, folder, disk, swap, 'flash', flash=True)
    try:
        guest.shell('pcb 1', 'process_state=Live TID=1')
        guest.shell('echo disk-boot-ok | cat', 'disk-boot-ok')
        guest.shell('ps', 'Free TCB slots: 31')
        print('PASS: flash -> WUFS -> kernel boot with TCB scheduler', flush=True)
    finally:
        guest.close()
    if args.gui:
        guest = Guest(qemu, folder, disk, swap, 'gui', gui=True)
        try:
            guest.command('input-send-event', {'events': [
                {'type': 'abs', 'data': {'axis': 'x', 'value': round(50 * 32767 / 1279)}},
                {'type': 'abs', 'data': {'axis': 'y', 'value': round(50 * 32767 / 719)}}]})
            guest.command('input-send-event', {'events': [{'type': 'btn', 'data': {'down': True, 'button': 'left'}}]})
            time.sleep(.2)
            guest.command('input-send-event', {'events': [{'type': 'btn', 'data': {'down': False, 'button': 'left'}}]})
            time.sleep(.3)
            result = guest.shell('ps', 'Free TCB slots: 30')
            assert '2 0 shell Waiting event 2' in result, result
            guest.command('send-key', {'keys': [{'type': 'qcode', 'data': 'ctrl'}, {'type': 'qcode', 'data': 'd'}], 'hold-time': 100})
            time.sleep(.4)
            guest.shell('ps', 'Free TCB slots: 31')
            print('PASS: GUI Terminal TCB creation, keyboard EOF and orphan cleanup', flush=True)
        finally:
            guest.close()
    print(f'PASS: stage A regression; logs retained at {folder}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        sys.exit(1)
