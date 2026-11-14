@echo off
setlocal
set "QEMU=qemu-system-riscv64.exe"
where /q "%QEMU%"
if not errorlevel 1 for /f "delims=" %%I in ('where "%QEMU%"') do (
  set "QEMU=%%I"
  goto launch
)
set "QEMU=%ProgramFiles%\qemu\qemu-system-riscv64.exe"
if exist "%QEMU%" goto launch
echo WUWAOS: Install QEMU with RISC-V 64-bit and OpenSBI, then add its directory to PATH. 1>&2
exit /b 1

:launch
pushd "%~dp0" || exit /b 1
if not exist boot-flash.img goto missing_images
if not exist wuwaos-cli.img goto missing_images

"%QEMU%" -machine virt -smp 1 -m 128M -nographic -bios default ^
  -drive if=pflash,unit=0,format=raw,readonly=on,file=boot-flash.img ^
  -global virtio-mmio.force-legacy=false ^
  -drive file=wuwaos-cli.img,if=none,format=raw,id=wuwaos,cache=writeback ^
  -device virtio-blk-device,drive=wuwaos
set "WUWAOS_EXIT=%ERRORLEVEL%"
popd
exit /b %WUWAOS_EXIT%

:missing_images
echo WUWAOS: Extract the full CLI release ZIP before launching. 1>&2
popd
exit /b 1
