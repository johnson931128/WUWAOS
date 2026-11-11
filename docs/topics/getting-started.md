---
description: 選擇 Windows 免安裝版或 WSL 原始碼，啟動 WUWAOS 並執行第一條 shell 指令。
---

# 01 · 開始使用

從 Windows 啟動 WUWAOS。選一種方式，看到 `wuwaos/>` 後就能在系統裡輸入第一條指令。

<nav class="start-routes" aria-label="選擇啟動方式">
  <a href="#windows-portable"><strong>Windows 免安裝版</strong><span>下載 ZIP，直接體驗 CLI 或圖形桌面。</span></a>
  <a href="#wsl-source"><strong>從原始碼啟動（WSL）</strong><span>用 Ubuntu 建置，從 WUFS 磁碟開機。</span></a>
</nav>

## 建置與啟動

### Windows 免安裝版 {#windows-portable}

下載 [Windows portable v1 Release](https://github.com/johnson931128/WUWAOS/releases/tag/portable-windows-v1) 的完整 ZIP 並解壓。下載與校驗步驟見 [Windows portable 指南](../portable-windows.md)。

**在 PowerShell 輸入**，先切換到解壓後的專案根目錄。

```powershell
.\Start-WUWAOS.cmd
```

腳本會準備建置環境、編譯並開啟 QEMU。看到 **`wuwaos/>`** 就可以往下執行 shell 指令。想直接開啟圖形桌面，可用 **PowerShell** 執行 `.\Start-WUWAOS.cmd gui`。

### 從原始碼啟動（WSL） {#wsl-source}

WSL Ubuntu 需備有 Clang、LLD、llvm-objcopy、Make、Python 3、QEMU RISC-V 與 OpenSBI。請將以下路徑換成你的專案目錄。

**在 PowerShell 輸入**

```powershell
cd C:\programing\WUWAOS
wsl -d Ubuntu -- make run-disk
```

指令在 WSL 交叉編譯並準備 WUFS 映像，接著啟動 QEMU。看到 **`wuwaos/>`** 就表示 WUWAOS shell 已就緒。

## 進入系統後

<ol class="start-flow" aria-label="從主機啟動到 WUWAOS shell">
  <li><strong>Windows 主機</strong><span>在 PowerShell 啟動</span></li>
  <li><strong>QEMU virt</strong><span>模擬 RISC-V 硬體</span></li>
  <li><strong>WUWAOS shell</strong><span>看到提示字元後輸入</span></li>
</ol>

看到提示字元後，在同一個視窗輸入 `help` 並按 Enter。這時，指令已經交給 QEMU 裡的 WUWAOS shell。

```text
wuwaos/> help
```

`help` 會列出可用命令。接著可用 `ps` 查看行程、`vmstat` 查看分頁統計、`ls /` 查看根目錄。用 `fan /home/user/note.txt` 可以進入編輯器，操作見 [使用者程式](./applications.md)。

**離開 CLI 的 QEMU**，按 Ctrl+A，放開後按 X。Shell 裡的 `exit` 會結束目前 shell，QEMU 仍會繼續執行。

## 進階啟動與磁碟

### 磁碟開機與直接開機

`make run-disk` 將核心安裝到 `build/wuwaos-cli.img`。QEMU 啟動 OpenSBI 後，flash 載入器會從磁碟讀取 `/boot/kernel.elf`。這條路徑使用一顆 WUFS0002 磁碟，停用 swap，並要求 journal 無待重播紀錄。`make clean` 保留磁碟與其中的檔案。

若需要由 QEMU 直接載入核心並附加 swap 磁碟，在 **PowerShell** 執行 `wsl -d Ubuntu -- make run`。此路徑使用 `build/wuwaos.img` 與 `build/wuwaos-swap.img`。若 journal 有待重播紀錄，請走會跳過主機映像更新的[復原流程](../SPEC-32-wufs-redo-journal.md#新磁碟與-pending-recovery-操作)。

### 搭配 portable 工具環境

在 **PowerShell** 執行 `.\Start-WUWAOS.cmd build` 可以只編譯並準備磁碟映像。需要直接使用 Makefile 時，執行 `.\Start-WUWAOS.cmd shell` 進入隨附的 **Bash**，再執行 `make`、`make run` 或 `make run-gui GUI_DISPLAY=gtk`。

若搭配目前原始碼，也可在該 **Bash** 執行 `make run-disk`。原始 portable v1 ZIP 內含該版本的直接開機流程，`run-disk` 需要目前原始碼。

### 磁碟保護與驗證

目前原始碼在核心與主機映像工具都會保護非空白磁碟。若 magic 不符，只有確認全部資料為零才允許初始化。要核對空白、損毀與尾端資料的處理，請從目前原始碼的專案根目錄，在 **PowerShell** 使用既有入口。

```powershell
.\Test-FsDisk.cmd valid
.\Test-FsDisk.cmd blank
.\Test-FsDisk.cmd damaged
.\Test-FsDisk.cmd tail
```

每次執行都會建立獨立測試磁碟。依畫面提示退出 QEMU 後，工具會檢查前後雜湊。這些入口與保護修正於 portable v1 之後加入。預期訊息、既有結果與驗證範圍見 [Spec 31](../SPEC-31-filesystem-initialization-guard.md)。

## 對應程式碼

`Makefile` 負責交叉編譯、連結與 QEMU 參數。`tools/make_disk.py` 維護 WUFS 映像，`tools/make_swap.py` 建立 swap 映像。這些工具都在主機執行。

觀念參考見 `notes/ch1 Introduction.md` 與 `notes/ch2 Operating-System Structures.md`。既有 OS 操作結果見[規格紀錄](../history.md)，文件站的啟動方式見[維護文件網站](./writing.md)。

**接著讀 [〈開機流程〉](./boot.md)**，看 OpenSBI、flash 載入器和 WUWAOS 核心如何交接。
