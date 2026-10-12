# WUWAOS

在 QEMU 上執行的小型 RISC-V 64-bit 作業系統。

## 已完成

- OpenSBI（機器模式 firmware）→ S-mode（監督模式）核心啟動。
- UART（序列埠）文字輸出、trap（中斷／例外）共同入口與診斷。
- Sv39（三級分頁），區分核心與使用者記憶體權限。
- 一個固定建立的 process（行程），從核心進入 U-mode（使用者模式）。
- write、getpid、exit 三個 syscall（系統呼叫），含返回值及使用者 buffer（資料緩衝區）檢查。
- 獨立編譯並嵌入核心的使用者示範程式。

本次規格與驗收紀錄在 [SPEC.md](docs/SPEC.md)，所有程式碼的功能與硬體關係在 [CODE_GUIDE.md](docs/CODE_GUIDE.md)。

## 建置與執行

使用 Windows 的 WSL Ubuntu，需要 Clang、LLD、llvm-objcopy、Make、qemu-system-riscv64，以及 QEMU 可找到的 OpenSBI firmware。
目前已使用 Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3 完成實際功能驗證。

在專案根目錄的 PowerShell 執行：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

Linux / WSL shell 可以直接使用 `make`、`make run`。更改編譯選項後請 `make clean` 再建置。
平台固定為 QEMU virt、單核心、128 MiB RAM；不要直接修改為多核心。

OpenSBI 資訊後預期出現：

```text
WUWAOS booting...
[trap] Direct entry installed
[vm] Sv39: user code RX; data/stack RW; kernel/UART supervisor-only
[process] PID=1 Ready
[process] PID=1 Running; entering U-mode
[syscall] from U-mode number=0x0000000000000002
[syscall] return=0x0000000000000001
[syscall] from U-mode number=0x0000000000000001
[user] Hello from PID 1
[syscall] return=0x0000000000000018
[syscall] from U-mode number=0x0000000000000003
[process] PID=1 Exited status=0x0000000000000000
```

十六進位 `0x18` 是 24，代表 write 回傳輸出文字的 byte 數。
行程結束後核心保持等待；不會關閉 QEMU。按 **Ctrl+A，再按 X** 離開。
`build/` 裡的 ELF（執行映像）、binary（原始機器碼）、map（布局紀錄）與其他產物不納入 Git。

## 閱讀與練習

先讀 `kernel/entry.S` → `kernel/main.c`，再依 CODE_GUIDE 追蹤 `process_start` → `user_enter` → `user/demo.c` → `trap_entry` → `syscall_dispatch` → `sret`（陷阱返回指令）。
可以手動推演 getpid 的 a7（呼叫編號暫存器）與 a0（回傳值暫存器）如何變化，再修改示範文字並觀察 write 回傳的 byte 數。

## 限制與下一步

目前是單一固定行程，沒有動態建立行程的 syscall、排程器、timer（計時器）中斷、通用頁配置器、ELF loader（執行檔載入器）、檔案系統或 shell。
使用者 code（程式碼）、data（資料）、stack（堆疊）各限制一頁；不支援浮點／向量程式，硬體位址固定且尚未解析 device tree（硬體描述資料）。
沒有加入測試框架或負面測試程式；使用者示範程式是目前正常工作負載。

下一個小步驟可加入第二個固定行程及 yield（主動讓出 CPU）的合作式排程，之後才加入 timer 搶占與 spawn（建立行程）。
