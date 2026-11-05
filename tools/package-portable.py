#!/usr/bin/env python3
"""Package this checkout and its installed MSYS2 environment without host data."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parent.parent
ENVIRONMENT = ROOT / 'build/portable/msys64'
PREFIX = 'WUWAOS-portable-windows-x64'
EXTRA_SOURCE_FILES = (
    'Start-WUWAOS.cmd',
    'tools/portable-entry.sh',
    'tools/package-portable.py',
    'docs/portable-windows.md',
    'THIRD_PARTY_NOTICES.md',
)
EXCLUDED_DIRECTORIES = {
    'home', 'tmp', 'dev', 'var/cache', 'var/log', 'var/tmp',
    'var/run', 'etc/pacman.d/gnupg', 'etc/fstab.d',
}
EXCLUDED_FILES = {
    'etc/hosts', 'etc/networks', 'etc/protocols', 'etc/services',
    'etc/mtab', 'etc/localtime', 'etc/passwd', 'etc/group',
    'etc/machine-id', 'var/lib/pacman/db.lck',
}


def git(*arguments):
    return subprocess.check_output(['git', '-C', str(ROOT), *arguments])


def environment_files():
    resolved_root = ENVIRONMENT.resolve()
    for directory, directories, filenames in os.walk(ENVIRONMENT, followlinks=False):
        current = Path(directory)
        relative = current.relative_to(ENVIRONMENT)
        directories[:] = sorted(
            name for name in directories
            if (relative / name).as_posix() not in EXCLUDED_DIRECTORIES
            and not (current / name).is_symlink()
        )
        for name in sorted(filenames):
            source = current / name
            relative_file = (relative / name).as_posix()
            if relative_file not in EXCLUDED_FILES:
                # Resolve file links so extraction does not need symlink privileges.
                if source.is_symlink() and not source.resolve().is_relative_to(resolved_root):
                    raise RuntimeError(f'Environment file points outside its root: {source}')
                yield source, f'build/portable/msys64/{relative_file}'


def packages():
    result = []
    for description in sorted((ENVIRONMENT / 'var/lib/pacman/local').glob('*/desc')):
        fields = {}
        for block in description.read_text(encoding='utf-8').split('\n\n'):
            lines = block.splitlines()
            if lines and lines[0].startswith('%'):
                fields[lines[0].strip('%')] = lines[1:]
        result.append({
            'name': fields['NAME'][0],
            'version': fields['VERSION'][0],
            'licenses': fields.get('LICENSE', []),
            'upstream': next(iter(fields.get('URL', [])), ''),
            'source_archive': (
                'https://repo.msys2.org/mingw/sources/'
                if fields['NAME'][0].startswith('mingw-w64-')
                else 'https://repo.msys2.org/msys/sources/'
            ) + fields['BASE'][0] + '-' + fields['VERSION'][0] + '.src.tar.zst',
            'recipe': 'https://github.com/msys2/' + (
                'MINGW-packages/tree/master/mingw-w64-' + fields['BASE'][0].removeprefix('mingw-w64-')
                if fields['NAME'][0].startswith('mingw-w64-')
                else 'MSYS2-packages/tree/master/' + fields['BASE'][0]
            ),
        })
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/releases' / f'{PREFIX}.zip')
    args = parser.parse_args()
    for required in ('usr/bin/bash.exe', 'usr/bin/make.exe', 'ucrt64/bin/clang.exe',
                     'ucrt64/bin/ld.lld.exe', 'ucrt64/bin/llvm-objcopy.exe',
                     'ucrt64/bin/qemu-system-riscv64.exe',
                     'ucrt64/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin'):
        if not (ENVIRONMENT / required).is_file():
            parser.error(f'Missing installed tool: {required}')
    if (ENVIRONMENT / 'var/lib/pacman/db.lck').exists():
        parser.error('The package manager is active. Finish its transaction before packaging.')

    source_names = set(git('ls-files', '-z').decode('utf-8').rstrip('\0').split('\0'))
    source_names.update(EXTRA_SOURCE_FILES)
    files = []
    for name in sorted(source_names):
        if name.startswith(('build/', '.git/')):
            continue
        source = ROOT / name
        if source.is_file():
            files.append((source, name))
        else:
            parser.error(f'Source file missing: {name}')
    files.extend(environment_files())
    total_bytes = sum(source.stat().st_size for source, _ in files)
    package_list = packages()
    manifest = {
        'format': 1,
        'platform': 'Windows x64',
        'source_commit': git('rev-parse', 'HEAD').decode().strip(),
        'working_tree_modified': bool(git('status', '--porcelain')),
        'created_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'files': len(files),
        'uncompressed_bytes': total_bytes,
        'packages': package_list,
        'excluded': [
            'Git metadata and existing build outputs',
            'Existing WUFS/swap images and all guest user files',
            'MSYS2 home directories, caches, logs, temporary files and signing keyring',
            'Host network files and user account files',
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    print(f'Packaging {len(files)} files, {total_bytes / 1024**3:.2f} GiB uncompressed.', flush=True)
    with zipfile.ZipFile(args.output, 'x', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=1, allowZip64=True, strict_timestamps=False) as archive:
        for index, (source, name) in enumerate(files, 1):
            archive.write(source, f'{PREFIX}/{name}')
            if index % 5000 == 0:
                print(f'Packed {index}/{len(files)} files', flush=True)
        archive.writestr(f'{PREFIX}/PORTABLE-MANIFEST.json', json.dumps(manifest, indent=2) + '\n')
        archive.writestr(f'{PREFIX}/THIRD-PARTY.txt',
                         'This bundle includes MSYS2 packages listed in PORTABLE-MANIFEST.json.\n'
                         'Original license files are retained in the bundled usr/share/licenses\n'
                         'and ucrt64/share/licenses directories. Package recipes and upstream\n'
                         'projects are recorded in the manifest. Source package archives:\n'
                         'https://repo.msys2.org/msys/sources/\n'
                         'https://repo.msys2.org/mingw/sources/\n')
        # Empty writable directories are needed even though host files are omitted.
        for directory in ('tmp', 'home', 'var/tmp', 'var/log', 'var/cache/pacman/pkg'):
            archive.mkdir(f'{PREFIX}/build/portable/msys64/{directory}/')

    with args.output.open('rb') as archive_file:
        digest = hashlib.file_digest(archive_file, 'sha256').hexdigest()
    checksum = args.output.with_suffix(args.output.suffix + '.sha256')
    checksum.write_text(f'{digest}  {args.output.name}\n', encoding='ascii')
    args.output.with_suffix('.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    if args.output.stat().st_size >= 2 * 1024**3:
        raise SystemExit('Archive exceeds the GitHub Release per-asset limit of 2 GiB.')
    print(f'Created: {args.output} ({args.output.stat().st_size / 1024**2:.1f} MiB)', flush=True)
    print(f'SHA256: {digest}', flush=True)


if __name__ == '__main__':
    main()
