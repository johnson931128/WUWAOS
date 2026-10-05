# WUWAOS

以 RISC-V 64-bit 為目標的小型作業系統，從 QEMU 啟動開始逐步實作。

## 本次 spec：最小啟動

- 平台：QEMU `virt`，單核心、128 MiB RAM。
- 啟動：OpenSBI（M-mode）→ `_start`（S-mode）→ `kernel_main`。
- 核心位址：`0x80200000`；stack：16 KiB。
- 初始化：關閉 supervisor interrupts、設定 stack、清除 BSS。
- 輸出：輪詢 UART（`0x10000000`），印出 `WUWAOS booting...` 後等待。
- 驗收：實際 QEMU console 出現核心訊息，OpenSBI 顯示 Next Mode 為 S-mode。
- 本次不加入 process、User Mode、syscall、排程或測試程式碼。

## 建置與執行

目前使用 Windows 的 WSL Ubuntu。需要 `clang`（支援 RISC-V target）、`ld.lld`、`make`、`qemu-system-riscv64` 與 QEMU 可找到的 OpenSBI firmware。
本次已使用 Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3 完成實際啟動驗證。

在專案根目錄的 PowerShell 執行：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

Linux／WSL shell 可直接執行 `make`、`make run`。
OpenSBI 的啟動資訊之後，應看到：

```text
WUWAOS booting...
```

核心此時會保持等待，不會返回主機 shell。按 **Ctrl+A，再按 X** 離開 QEMU。
`make clean` 移除建置產物；`build/` 不納入 Git。

## 程式閱讀順序

1. `kernel/linker.ld`：決定入口、各區段位置與 stack 空間。
2. `kernel/entry.S`：在呼叫 C 函式前準備執行環境。
3. `kernel/main.c`：用 volatile 存取 UART 暫存器並逐字輸出。

可手動修改啟動文字，重新建置並觀察 QEMU 輸出。

## 目前限制與下一步

目前只完成 S-mode 啟動與 UART 輸出。尚無 trap handler、分頁、process、User Mode、syscall 或 scheduler。
硬體位址固定為 QEMU `virt`，尚未解析 device tree；只支援 `-smp 1`。

下一個小步驟是建立 trap 入口，保存暫存器並顯示例外原因，讓後續錯誤可診斷。
長期方向依序為頁配置與 Sv39、User Mode 與 syscall、process 與合作式排程、timer 搶占，再加入 `spawn`。
