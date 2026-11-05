# WUWAOS

![Language: C](https://img.shields.io/badge/language-C-00599C?logo=c&logoColor=white)
![Architecture: RISC-V 64](https://img.shields.io/badge/architecture-RISC--V%2064-283272?logo=riscv&logoColor=white)
![Platform: QEMU](https://img.shields.io/badge/platform-QEMU-FF6600?logo=qemu&logoColor=white)
![Project: Educational OS](https://img.shields.io/badge/project-educational%20OS-2E8B57)
![License: MIT](https://img.shields.io/badge/license-MIT-green)

WUWAOS is a small operating system written in **C and RISC-V assembly** that boots on a QEMU-emulated RISC-V 64-bit machine. The project explores how processes, scheduling, memory, and file systems work together by implementing a kernel and user programs.

[Documentation website](https://johnson931128.github.io/WUWAOS/) · [Windows portable guide](docs/portable-windows.md) · [Releases](https://github.com/johnson931128/WUWAOS/releases) · [Code and architecture guide](docs/CODE_GUIDE.md) · [Specifications and verification](docs/history.md)

**Try WUWAOS on Windows:** [download the complete portable ZIP](https://github.com/johnson931128/WUWAOS/releases/download/portable-windows-v1/WUWAOS-portable-windows-x64.zip) (about 1.41 GiB), which includes the OS source and its build/run environment. Follow the [PowerShell setup commands](#windows-portable-release) below, then click the desktop's Terminal icon and try `help`, `demo sync`, or `workstate`.

## System Overview

| Area | Current implementation |
| --- | --- |
| Platform | QEMU `virt`, RISC-V 64-bit, one CPU core, and 128 MiB RAM; OpenSBI starts the kernel |
| Processes and scheduling | Up to 16 processes, process control blocks, fork, built-in and path-based exec, wait, exit, and timer-driven round-robin scheduling |
| Synchronization and IPC | Custom mutexes and semaphores, sleep/wakeup, per-process file descriptors, and multi-stage pipes |
| Virtual memory | Sv39 three-level page tables, demand paging, FIFO page replacement, and swap |
| File system | VirtIO block device and the custom WUFS file system, with persistent files, shared open-file offsets, and shell redirection |
| User interface | UART shell, `fan` text editor and `workstate` monitor; optional VirtIO GPU/tablet desktop with a fixed Terminal window and keyboard-operated shell |

WUWAOS focuses on fundamental operating system mechanisms and is not Linux-compatible. QEMU currently loads the kernel using `-kernel`. A restricted static RV64 ELF loader can run compatible WUWAOS programs from WUFS; general ELF/Linux compatibility, command-line arguments for external ELF programs, compiling C inside WUWAOS, networking, general window management, and a file manager are not supported. The graphical desktop provides one fixed-size Terminal window connected to the existing shell; `make run` remains the UART workflow. See the [documentation](https://johnson931128.github.io/WUWAOS/) and [Spec 21](docs/SPEC-21-native-compilation.md) for limitations and future plans.

## Demo Video

![WUWAOS QEMU demo: live terminal recording](docs/media/wuwaos-demo.gif)

The animation plays directly in the README. For playback controls and higher quality, [watch or download the MP4](docs/media/wuwaos-demo.mp4) (about 2 minutes).

The video follows a live QEMU session through the shell, processes, synchronization, pipes, WUFS, `fan`, `workstate`, and virtual memory. It was rendered from terminal output with chapter labels and has no narration; see the [recording details](docs/SPEC-24-demo-video.md).

## Boot Screenshot

![WUWAOS booting in QEMU](docs/images/wuwaos-boot.png)

*An actual QEMU session in PowerShell. The screenshot is from an earlier version; the current shell prompt shows the working directory, for example `wuwaos/>`.*

## Graphical Desktop

When QEMU has a VirtIO GPU, tablet, and keyboard, WUWAOS draws a 1280×720 desktop with a Terminal icon. Click the icon to open a fixed-size window connected to the existing WUWAOS shell. The guest kernel handles keyboard input, line editing, ANSI screen updates, and window rendering; closing the window hides the session, while `exit` ends the shell. The original UART shell remains available in `make run`.

![WUWAOS guest-rendered desktop Terminal showing shell commands](docs/images/wuwaos-terminal.png)

The current source removes the solid offset shadow along the window's right and bottom edges. The screenshot and portable v1 release show the earlier styling; acceptance checks for this source adjustment are pending.

The Terminal accepts US keyboard input including Shift, Enter, Backspace, Tab, Ctrl+D, and arrow keys. It supports a fixed 100×30 character area and a subset of ANSI cursor, erase, color, alternate-screen, and visibility controls. It is a small kernel TTY, not a POSIX PTY. It has no resize, drag, clipboard, job control, or Ctrl+C support. [Graphical Terminal design and QEMU evidence](docs/SPEC-30-graphical-terminal.md) · [workstate screenshot](docs/images/wuwaos-terminal-workstate.png).

With the Windows portable release, open the desktop from PowerShell using `./Start-WUWAOS.cmd gui`. QEMU runs natively on Windows; a VNC viewer is not needed. See the [portable guide](docs/portable-windows.md) for extraction and build instructions.

On Windows with an existing WSL1 environment, start the desktop and its native viewer with one PowerShell command. The WSL launcher builds the OS, runs QEMU in the background, and closes that QEMU session when the viewer closes:

```powershell
./tools/run-desktop.ps1
```

Install or download the official [TigerVNC Windows viewer](https://github.com/TigerVNC/tigervnc/releases) if one is not already installed or available in `build/tigervnc`. Pass `-ViewerPath 'C:\path\to\vncviewer64.exe'` to select a specific viewer. Add `-Snapshot` to work from temporary copies of the WUFS and swap images; those copies are discarded when the viewer closes. The default uses the project images and preserves guest file changes. The VNC service listens only on loopback; the launcher selects free VNC and QMP ports for each session.

```powershell
# Optional isolated session:
./tools/run-desktop.ps1 -Snapshot
```

For a Linux desktop or WSLg session with a working display, use `make run-gui GUI_DISPLAY=gtk` or `make run-gui GUI_DISPLAY=sdl`. The advanced manual WSL1 path remains `make run-gui-vnc` followed by a Windows VNC viewer connected to `127.0.0.1::5901`. GUI Make targets update the selected disk images before QEMU starts; do not edit images on the host while QEMU is using them. `make run` remains the text-mode UART workflow.

## Getting Started

### Windows portable release

The portable release bundles the WUWAOS source and a Windows x64 toolchain: MSYS2, Clang, LLD, LLVM, Make, Python, QEMU, and OpenSBI. It supports offline compilation and execution without an installer, administrator access, WSL, or changes to the system PATH.

The [Windows portable v1 release](https://github.com/johnson931128/WUWAOS/releases/tag/portable-windows-v1) is a complete source-and-environment bundle for Windows 10/11 x64. Download `WUWAOS-portable-windows-x64.zip`, rather than GitHub's automatically generated **Source code** ZIP. Allow about 8 GiB of free disk space for the download, extracted tools, and build outputs.

**Environment setup from PowerShell:** paste the following commands to download the bundle, verify its SHA-256, extract it into a new `WUWAOS-play` folder under your user profile, and start the desktop. Extraction sets up all the bundled tools; no separate compiler, Python, QEMU, or MSYS2 installation is needed. Use a writable destination with a simple path, avoiding spaces and special characters; change `$destination` if needed. If that folder already exists, choose a new destination to keep existing sessions and files separate.

```powershell
$ErrorActionPreference = 'Stop'
$release = 'https://github.com/johnson931128/WUWAOS/releases/download/portable-windows-v1'
$destination = Join-Path $env:USERPROFILE 'WUWAOS-play'
$name = 'WUWAOS-portable-windows-x64.zip'

New-Item -ItemType Directory -Path $destination | Out-Null
$archive = Join-Path $destination $name
Invoke-WebRequest -UseBasicParsing -Uri "$release/$name" -OutFile $archive
Invoke-WebRequest -UseBasicParsing -Uri "$release/$name.sha256" -OutFile "$archive.sha256"

$expected = ((Get-Content -LiteralPath "$archive.sha256" -Raw) -split '\s+')[0]
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $expected) {
    throw 'Download checksum mismatch. Download the ZIP again.'
}
tar.exe -xf $archive -C $destination
if ($LASTEXITCODE -ne 0) { throw 'Archive extraction failed.' }

Set-Location (Join-Path $destination 'WUWAOS-portable-windows-x64')
.\Start-WUWAOS.cmd gui
```

For a manual download, extract the complete ZIP and open PowerShell in the extracted `WUWAOS-portable-windows-x64` project directory. These commands use the bundled environment:

```powershell
.\Start-WUWAOS.cmd check   # Verify the bundled tools
.\Start-WUWAOS.cmd build   # Compile and prepare fresh disk images
.\Start-WUWAOS.cmd gui     # Build if needed and open the graphical desktop
.\Start-WUWAOS.cmd         # Build if needed and run the UART shell
```

Close the QEMU window to stop the graphical session. For the UART shell, press **Ctrl+A**, release the keys, and press **X**. Stop the existing session before opening another one that uses the same disk images.

The release includes a package/version manifest and SHA-256 checksum. It excludes the original computer's user profiles, cached downloads, signing keyring, and guest disk contents. The first build creates new WUFS and swap images; later sessions preserve guest files. The toolchain supports Windows x64; other hosts need their own tools. See the [portable guide](docs/portable-windows.md) for details and packaging commands.

GitHub's automatically generated **Source code** archives and a normal `git clone` contain only project files; the toolchain is supplied separately in the portable release asset.

### Existing WSL environment

The following example uses Windows PowerShell, WSL Ubuntu, and a checkout at `C:\programing\WUWAOS`. WSL needs Clang, LLD, llvm-objcopy, Make, Python 3, QEMU for RISC-V, and OpenSBI.

Run in PowerShell:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

`make run` uses Clang in WSL to cross-compile the RISC-V program and then starts QEMU. Once the `wuwaos/>` prompt appears, enter `help` to see the available commands. Try `demo sync` to observe a parent and child process using synchronization primitives.

To exit QEMU, press **Ctrl+A**, release the keys, and then press **X**. The `exit` command ends only the shell. The WUFS disk image is stored at `build/wuwaos.img`; its files persist across restarts, and `make clean` does not delete the disk image.

## Filesystem Initialization Checks

The current source preserves legacy `WUFS0001` images read-only and formats confirmed blank media as `WUFS0002`. WUFS0002 uses a bounded full-sector redo journal with flush barriers and boot-time replay. Unknown nonblank images and images with a pending or invalid journal are never rewritten by the host image tool. These changes postdate portable v1; the existing release ZIP does not include WUFS0002 or the journal and is not rebuilt by this source update. Details, new-disk commands, recovery steps, and scratch QEMU fault-injection results are in [Spec 32](docs/SPEC-32-wufs-redo-journal.md); the older initialization guard is in [Spec 31](docs/SPEC-31-filesystem-initialization-guard.md).

From a source checkout with the portable environment, run these fixed commands in PowerShell:

```powershell
.\Test-FsDisk.cmd valid
.\Test-FsDisk.cmd blank
.\Test-FsDisk.cmd damaged
.\Test-FsDisk.cmd tail
```

Each invocation automatically builds the kernel and creates fresh test disks. Confirm the expected boot message, use read-only commands such as `help` or `ls`, then press **Ctrl+A**, release, and press **X**. The tool checks disk hashes after QEMU exits. `valid` should mount without disk changes; `blank` should gain a WUFS signature; `damaged` and `tail` should refuse formatting and retain identical hashes. The reported `PASS` concerns disk contents; boot messages still need manual confirmation.

See [Spec 31](docs/SPEC-31-filesystem-initialization-guard.md) for the algorithm, exact expected messages, and remaining verification work.

## Running a WUFS ELF Program

With QEMU stopped, build the existing shell ELF and install it into the WUFS image from WSL:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make build/kernel-disk.elf && python3 tools/make_disk.py --image build/wuwaos.img --kernel build/kernel-disk.elf --install build/user.elf /bin/shell.elf'
```

Start WUWAOS with `make run`, then enter `run /bin/shell.elf`. This opens another WUWAOS shell as a child; `exit` returns to its parent. The installer refuses to overwrite existing files. Never modify the image from the host while QEMU is using it.

This first loader accepts a restricted static ELF64 RV64 layout and the WUWAOS syscall ABI; it has no arguments, dynamic linker, or Linux compatibility. See [Spec 26](docs/SPEC-26-elf-exec.md) for the supported layout and validation.

## Documentation

The documentation and course notes linked below are currently written in Traditional Chinese.

- [Getting started and topic chapters](docs/index.md): boot, processes, scheduling, memory, I/O, file systems, and user programs.
- [Windows portable environment](docs/portable-windows.md): offline builds, native QEMU, packaging, checksums, and Release uploads.
- [Synchronization demo](docs/SPEC-23-sync-demo.md): observing mutex and semaphore blocking and wakeup.
- [Code and architecture guide](docs/CODE_GUIDE.md): the responsibilities and design of each source file.
- [Specifications and verification index](docs/history.md): design decisions, verification, and limitations for each stage; older specifications retain their historical context.
- [Course notes](notes/): operating system concepts and terminology.

## License

WUWAOS's original source code is licensed under the [MIT License](LICENSE). The desktop font uses the [SIL Open Font License 1.1](licenses/Cascadia-OFL.txt). Tools bundled in portable releases retain their individual licenses; see [third-party notices](THIRD_PARTY_NOTICES.md) and the release's package manifest for details.
