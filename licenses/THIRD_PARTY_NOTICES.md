# Third-party notices

WUWAOS's original source code is licensed under the [MIT License](../LICENSE).
That license does not replace the licenses of third-party components shipped
in the repository or in a portable release.

## Guest desktop font

`kernel/font_terminal.h` contains **WUWAOS Terminal**, a renamed rasterized
ASCII subset derived from Microsoft Cascadia Mono Regular v2407.24. This
font data is licensed under the [SIL Open Font License 1.1](./Cascadia-OFL.txt).
The upstream copyright and reserved font name notice are retained there.
See [font generation details](./README.md).

`kernel/font8x8_basic.h` contains the previous public-domain bitmap font.

## Windows portable environment

Portable release ZIPs also contain independently distributed MSYS2 packages,
including Clang, LLD, LLVM, GNU Make, Python, QEMU, OpenSBI, and their runtime
dependencies. These programs run on the host computer and are not part of the
WUWAOS guest kernel. Their licenses include GPL, LGPL, Apache, BSD, MIT,
Python, and other licenses; the complete package list is version-specific.

Each portable release contains:

- `PORTABLE-MANIFEST.json`: exact installed package versions, declared license
  identifiers, upstream projects, package recipes, and versioned source-archive
  links. Source availability at those external links depends on the distributor.
- `build/portable/msys64/usr/share/licenses` and
  `build/portable/msys64/ucrt64/share/licenses`: original license and copyright
  files supplied by the installed packages.
- `build/portable/msys64/var/lib/pacman/local`: installed package metadata,
  including license declarations and package file lists.

Source package indexes:

- [MSYS2 environment sources](https://repo.msys2.org/msys/sources/)
- [MinGW/UCRT tool sources](https://repo.msys2.org/mingw/sources/)
- [MSYS2 package build recipes](https://github.com/msys2/MSYS2-packages)
- [MinGW package build recipes](https://github.com/msys2/MINGW-packages)

Redistribution of the bundled tools is governed by their individual licenses,
including applicable corresponding-source requirements. Preserve their notices
and license texts when sharing the environment. The manifest and external source
links are an inventory, not a replacement for those license terms or for source
material required by a particular license.
