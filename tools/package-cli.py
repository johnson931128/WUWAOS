#!/usr/bin/env python3
"""Build the Windows CLI release using the host's RISC-V toolchain."""

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
NAME = "WUWAOS-cli-windows-x64"
BUILD = ROOT / "build"


def main():
    subprocess.run(
        ["make", "build/boot-flash.img", "build/kernel-disk.elf"], cwd=ROOT, check=True
    )
    archive = BUILD / f"{NAME}.zip"
    with tempfile.TemporaryDirectory(prefix="cli-release-", dir=BUILD) as temporary:
        workspace = Path(temporary)
        disk = workspace / "wuwaos-cli.img"
        subprocess.run(
            [
                sys.executable, "-B", str(ROOT / "tools/make_disk.py"),
                "--image", str(disk), "--kernel", str(BUILD / "kernel-disk.elf"),
            ],
            check=True,
        )
        staging = workspace / archive.name
        with ZipFile(staging, "w", compression=ZIP_DEFLATED) as bundle:
            for source in (ROOT / "WUWAOS.cmd", ROOT / "LICENSE",
                           BUILD / "boot-flash.img", disk):
                bundle.write(source, f"{NAME}/{source.name}")
            bundle.write(ROOT / "licenses/Cascadia-OFL.txt", f"{NAME}/licenses/Cascadia-OFL.txt")
        staging.replace(archive)

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(".zip.sha256").write_text(
        f"{digest}  {archive.name}\n", encoding="ascii"
    )
    print(f"{archive} ({archive.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
