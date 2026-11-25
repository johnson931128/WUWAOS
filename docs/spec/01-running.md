# 01 · 如何執行：啟動、建置與驗證

<script setup>
import BootTimeline from '../.vitepress/theme/components/BootTimeline.vue'
</script>

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

剛按下電源鍵時，RAM 裡一片空白，作業系統核心還躺在磁碟裡。既然沒有作業系統可以幫忙讀磁碟，電腦要怎麼把自己「拉起來」（Bootstrap）？這就是開機鏈（Boot Chain）設計要解決的雞生蛋問題。
</div>

## 恐龍書核心理論：Bootstrap Program 與多階段開機

恐龍書第 1 章指出，電腦剛通電時，CPU 會從固定的非揮發性記憶體（ROM / Flash）執行一小段 **Bootstrap Program（引導程式）**。它的工作非常單純：

1. **硬體初始與探測**：初始化 CPU 狀態與記憶體控制器，並準備硬體描述資訊（在 RISC-V 平台通常是 Device Tree / FDT）。
2. **載入作業系統核心**：從儲存裝置把作業系統核心映像（例如 ELF 檔）讀進 RAM。
3. **交接控制權**：把 CPU 的程式計數器跳轉到核心的進入點（Entry Point），由核心接管整台機器並啟動第一個使用者行程。

在 RISC-V 架構下，這條交接鏈通常跨越兩個特權等級：**M-mode 韌體（OpenSBI）** → **S-mode 載入器／核心** → **U-mode Shell**。

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 的兩條開機實例</span>

WUWAOS 實作了兩種開機路徑，方便對照真實磁碟開機與快速開發除錯：

- **完整磁碟開機（`make run-disk`）**：OpenSBI 交給位在唯讀 Flash（`0x20000000`）的自製 [Flash 載入器](./glossary.md#flash-loader)，載入器透過 VirtIO 讀取 WUFS 磁碟上的 `/boot/kernel.elf` 並載入 RAM，再跳到核心入口 `_start`。
- **直接核心開機（`make run` / `make run-gui`）**：由 QEMU 的 `-kernel` 選項預先把 `build/kernel.elf` 放進 RAM（`0x80200000`），OpenSBI 直接交接給核心，略過 Flash 載入器。
</div>

## 從原始碼啟動

你可以直接使用預編譯的 Windows CLI Release，或在 Windows 的 [WSL](./glossary.md#wsl) Ubuntu 內建置目前原始碼（需安裝 Clang、LLD、LLVM 工具、Make、Python 3 與 RISC-V QEMU）。從專案根目錄的 PowerShell 執行：

```powershell
wsl -d Ubuntu -- make run-disk
```

| 指令入口 | 實際開機路徑與掛載裝置 |
| --- | --- |
| `make run-disk` | OpenSBI → Flash 載入器 → 從 `build/wuwaos-cli.img` 載入 `/boot/kernel.elf`。預設單磁碟，載入器要求 [journal](./glossary.md#redo-journal) 無待重播交易。 |
| `make run` | QEMU 直接載入 `build/kernel.elf`，同時掛載 `build/wuwaos.img` 與 `build/wuwaos-swap.img`，適合測試 Swap 與 Journal 重播。 |
| `make run-gui` | 直接載入核心並接上 VirtIO GPU 與鍵盤／滑鼠裝置，啟動原生圖形桌面。 |

若要在 CLI 測試多核心（多 [Hart](./glossary.md#hart)），可加上參數：

```powershell
wsl -d Ubuntu -- make run-disk CLI_HARTS=2
```

新磁碟通常由主機工具建立為 `WUFS0002`（載入器同時識別 `WUFS0003`）。若磁碟存在未重播交易，請依[復原流程](../SPEC-32-wufs-redo-journal.md#新磁碟與-pending-recovery-操作)處理。

## 用現有 Release 啟動

解壓 Windows CLI Release、依發行說明安裝 QEMU，在解壓目錄用 CMD 執行 `WUWAOS` 即可啟動預編譯核心。[Windows CLI Release](https://github.com/johnson931128/WUWAOS/releases/tag/cli-windows-v1)

## 確認已進入系統

終端機出現 `wuwaos/>` 提示字元後，可輸入 `help`、`ps` 或 `ls /` 驗證系統運作。離開 CLI 模式的 QEMU 請按 `Ctrl+A` 再按 `X`（在 Shell 裡輸入 `exit` 只會結束該 Shell 行程）。

自動化回歸測試可於 WSL 執行 `python3 -B tools/test_kernel_cli.py`，涵蓋直接開機、Flash 開機、2／3 Hart、管線與 WUFS 重播。[Makefile](https://github.com/johnson931128/WUWAOS/blob/3a3b273/Makefile#L143) · [磁碟載入器](https://github.com/johnson931128/WUWAOS/blob/3a3b273/bootloader/boot.c) · [CLI 測試](https://github.com/johnson931128/WUWAOS/blob/3a3b273/tools/test_kernel_cli.py) · [既有驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)

## 從韌體交接到 Shell

下方互動時間線把課本的 Bootstrap 理論對應到 WUWAOS 的每一個記憶體位址與程式碼入口；可切換「磁碟開機」與「直接開機」比較差異。[開機流程補充](../topics/boot.md)

<BootTimeline />
