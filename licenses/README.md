# Desktop font

`kernel/font_terminal.h` contains **WUWAOS Terminal**, a renamed rasterized
ASCII subset derived from Microsoft's Cascadia Mono Regular, release v2407.24.
The font data is covered by the SIL Open Font License 1.1 in
`Cascadia-OFL.txt`; it is not covered by the repository's MIT license.

Upstream: https://github.com/microsoft/cascadia-code/tree/v2407.24

To regenerate on Windows, download and extract the official
`CascadiaCode-2407.24.zip` release, then run:

```powershell
./tools/make_desktop_font.ps1 -FontPath 'path/to/ttf/static/CascadiaMono-Regular.ttf'
```

The generator uses Windows GDI+ grayscale antialiasing at 13 pixels and stores
printable ASCII in 8×16 cells. Rasterization can differ between Windows versions.
The OS builds directly from the checked-in header; no font installation, runtime
TrueType parser, or Windows dependency is required in the guest.

`kernel/font8x8_basic.h` is the previous public-domain font and is no longer used
by the desktop renderer.
