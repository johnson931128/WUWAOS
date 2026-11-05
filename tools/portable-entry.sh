#!/usr/bin/env bash
set -euo pipefail

# Use only the bundled tools; do not load this computer's shell profiles.
export PATH=/ucrt64/bin:/usr/bin:/bin
mkdir -p /tmp
case "${1:-run}" in
  ''|run) exec make run ;;
  gui) exec make run-gui GUI_DISPLAY=gtk ;;
  build) exec make -j4 disk-image ;;
  check)
    clang --version
    ld.lld --version
    llvm-objcopy --version
    make --version
    python3 --version
    qemu-system-riscv64 --version
    test -f /ucrt64/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin
    ;;
  shell) exec bash --noprofile --norc ;;
  *) echo 'Usage: Start-WUWAOS.cmd [run|gui|build|check|shell]' >&2; exit 2 ;;
esac
