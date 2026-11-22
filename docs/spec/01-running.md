# 01 · 如何執行：啟動、建置與驗證

<script setup>
import BootTimeline from '../.vitepress/theme/components/BootTimeline.vue'
</script>

你可以直接使用已有的 Windows CLI Release，或在 Windows 的 [WSL](./glossary.md#wsl) Ubuntu 內建置目前原始碼。前者功能以該版本的發行說明為準，後者會編譯工作目錄中的原始碼。

## 從原始碼啟動

WSL Ubuntu 需有 Clang、LLD、LLVM 工具、Make、Python 3 和 RISC-V QEMU。從專案根目錄的 PowerShell 執行：

```powershell
wsl -d Ubuntu -- make run-disk
```

| 入口 | 實際開機路徑 |
| --- | --- |
| `make run-disk` | OpenSBI 交給 [Flash 載入器](./glossary.md#flash-loader)，再由載入器從 `build/wuwaos-cli.img` 取出 `/boot/kernel.elf`。預設只掛這一顆 WUFS 磁碟，載入器要求 [journal](./glossary.md#redo-journal) 無待重播交易。 |
| `make run` | QEMU 直接載入 `build/kernel.elf`，同時掛上 `build/wuwaos.img` 與 `build/wuwaos-swap.img`。可用於 Swap 測試或依復原紀錄操作。 |
| `make run-gui` | 直接載入核心並接上 VirtIO 圖形與輸入裝置，進入圖形桌面。 |

要在 CLI 指定兩個 [Hart](./glossary.md#hart)，可執行 `wsl -d Ubuntu -- make run-disk CLI_HARTS=2`。預設是一個 Hart。新磁碟通常由主機工具建立成 WUFS0002，磁碟載入器也識別 WUFS0003。若已有磁碟出現未重播交易，請依[復原流程](../SPEC-32-wufs-redo-journal.md#新磁碟與-pending-recovery-操作)處理。 [Makefile](https://github.com/johnson931128/WUWAOS/blob/3a3b273/Makefile#L143) · [磁碟載入器](https://github.com/johnson931128/WUWAOS/blob/3a3b273/bootloader/boot.c)

## 用現有 Release 啟動

解壓 Windows CLI Release，按發行說明安裝 QEMU，然後在解壓目錄用 CMD 執行 `WUWAOS`。這條路徑執行的是發行版的預編譯核心。 [Windows CLI Release](https://github.com/johnson931128/WUWAOS/releases/tag/cli-windows-v1)

## 確認已進入系統

看到 `wuwaos/>` 後，在 QEMU 裡輸入 `help`、`ps` 或 `ls /`。終止 CLI 的 QEMU 可按 `Ctrl+A` 後按 `X`。Shell 的 `exit` 則只結束 Shell。

既有自動化驗收從 WSL 執行 `python3 -B tools/test_kernel_cli.py`。工具會另建測試磁碟，涵蓋直接開機、Flash 開機、2／3 Hart、管線和 WUFS 重播。 [CLI 測試](https://github.com/johnson931128/WUWAOS/blob/3a3b273/tools/test_kernel_cli.py) · [既有驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)

## 從韌體交接到 Shell

沿著開機順序逐步查看 OpenSBI、載入器、核心和 Shell 如何交接。 [開機流程補充](../topics/boot.md)

<BootTimeline />
