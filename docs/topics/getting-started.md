# 01 · 開始使用

本章的目標是從 PowerShell 啟動 WUWAOS，分清楚主機、模擬器與客體作業系統。

## 三個執行環境

| 環境 | 負責什麼 |
| --- | --- |
| Windows／PowerShell | 你輸入啟動指令的主機 |
| Windows portable 環境或 WSL Ubuntu | 用 Clang、LLD、Make 建置 RISC-V 程式，執行 QEMU |
| QEMU 裡的 WUWAOS | 核心管理虛擬硬體，使用者 shell 接受 OS 指令 |

QEMU 的 virt 機型提供的是指定的虛擬硬體。WUWAOS 仍須有對應驅動與核心機制才能使用裝置。

## 建置與啟動

### 目前原始碼的 CLI 磁碟開機

WSL Ubuntu 需備有 Clang、LLD、llvm-objcopy、Make、Python 3、QEMU RISC-V 與 OpenSBI。從 PowerShell 進入你的專案目錄，再啟動 QEMU。

```powershell
cd C:\programing\WUWAOS
wsl -d Ubuntu -- make run-disk
```

這條指令在 WSL 交叉編譯，將核心安裝到 `build/wuwaos-cli.img`。QEMU 啟動 OpenSBI 後，flash 裡的載入器從磁碟讀取 `/boot/kernel.elf`，同一個 PowerShell 視窗最後會顯示 `wuwaos/>`。實際交接順序見[開機流程](./boot.md)。

此路徑使用一顆 WUFS0002 磁碟，停用 swap，並要求 journal 無待重播紀錄。`make clean` 保留磁碟與裡面的檔案。需要用 QEMU 直接載入核心並附加獨立 swap 磁碟時，可執行 `wsl -d Ubuntu -- make run`，它使用 `build/wuwaos.img` 與 `build/wuwaos-swap.img`。若要重播待處理的 journal，請使用跳過主機映像更新的[復原流程](../SPEC-32-wufs-redo-journal.md#新磁碟與-pending-recovery-操作)。

### Windows 免安裝版本

下載 [Windows portable v1 Release](https://github.com/johnson931128/WUWAOS/releases/tag/portable-windows-v1) 的完整 ZIP 並解壓，環境就已準備完成。下載與校驗指令見 [Windows portable 指南](../portable-windows.md)。在專案根目錄的 PowerShell 執行：

```powershell
.\Start-WUWAOS.cmd       # 自動編譯並進入 CLI
.\Start-WUWAOS.cmd gui   # 自動編譯並開啟圖形桌面
.\Start-WUWAOS.cmd build # 編譯並準備磁碟映像，不啟動 QEMU
```

需要操作 Makefile 時，可先用 `.\Start-WUWAOS.cmd shell` 進入隨附環境，再輸入 `make`、`make run` 或 `make run-gui GUI_DISPLAY=gtk`。一般啟動不必手動設定 Bash 路徑或環境變數。

**目前原始碼搭配 portable 工具環境**也支援磁碟開機。在 PowerShell 執行：

```powershell
.\Start-WUWAOS.cmd shell
```

進入 Bash 後輸入：

```bash
make run-disk
```

這會使用 `build/boot-flash.img` 的載入器，從 `build/wuwaos-cli.img` 讀取 `/boot/kernel.elf`；完整 ASCII 圖解見[磁碟開機原理](./boot.md#磁碟開機-主機先準備什麼)。原始 portable v1 ZIP 使用該版本內的直接開機流程，須有目前原始碼才有 `run-disk` target。

### 最新原始碼的磁碟保護與檢查

目前原始碼在 kernel 與主機映像工具都加入非空白磁碟保護：magic 不符時，只有確認全部資料為零才允許初始化。一般啟動前會維護磁碟映像；要驗證 kernel 的空白／損毀處理，請使用固定測試入口：

```powershell
.\Test-FsDisk.cmd valid
.\Test-FsDisk.cmd blank
.\Test-FsDisk.cmd damaged
.\Test-FsDisk.cmd tail
```

每次都會自動編譯並建立獨立測試磁碟。按 Ctrl+A，放開後按 X 結束 QEMU，工具才會檢查前後雜湊。預期訊息與結果見 [Spec 31](../SPEC-31-filesystem-initialization-guard.md)。這些是 portable v1 之後的原始碼更新；v1 ZIP 沒有上述測試入口，也不包含本次保護修正。使用者已確認磁碟保護驗證完成，證據範圍與未驗證項目記錄於該規格。

## 進入系統後

看到開機動畫和 `wuwaos/>` 後，以下指令才是在 WUWAOS 裡輸入：

```text
help
ps
vmstat
ls /
```

應能看到可用命令、行程資訊、分頁統計與根目錄。使用 `fan /home/user/note.txt` 可以進入編輯器；詳見[使用者程式](./applications.md)。

按 `Ctrl+A`，放開後按 `X` 離開 QEMU。`exit` 只結束 shell，並不關閉虛擬電腦。

## 對應程式碼

`Makefile` 負責交叉編譯、連結與 QEMU 參數；`tools/make_disk.py` 維護 WUFS 映像；`tools/make_swap.py` 建立專用交換區。它們是在主機執行的工具，不是 WUWAOS 的 syscall（系統呼叫）。

## 閱讀與驗證邊界

本機文件網站是另一個主機程式，不在 QEMU 裡執行。其啟動方式見[維護文件網站](./writing.md)。OS 既有操作與結果見[規格紀錄](../history.md)，本次文件建置不會重新驗證核心功能。

觀念參考：`notes/ch1 Introduction.md`、`notes/ch2 Operating-System Structures.md`。本章指令則依 WUWAOS 的 Makefile 與操作介面整理。
