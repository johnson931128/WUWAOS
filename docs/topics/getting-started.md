# 01 · 開始使用

本章的目標是從 PowerShell 啟動 WUWAOS，分清楚主機、模擬器與客體作業系統。

## 三個執行環境

| 環境 | 負責什麼 |
| --- | --- |
| Windows／PowerShell | 你輸入啟動指令的主機 |
| WSL Ubuntu | 用 Clang、LLD、Make 建置 RISC-V 程式，執行 QEMU |
| QEMU 裡的 WUWAOS | 核心管理虛擬硬體，使用者 shell 接受 OS 指令 |

QEMU 的 virt 機型提供的是指定的虛擬硬體。WUWAOS 仍須有對應驅動與核心機制才能使用裝置。

## 建置與啟動

WSL Ubuntu 需備有 Clang、LLD、llvm-objcopy、Make、qemu-system-riscv64 及 OpenSBI。既有 OS 驗收環境為 Clang 18、QEMU 8.2.2、OpenSBI 1.3。

在專案根目錄 `C:\programing\WUWAOS` 的 PowerShell 執行：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

Makefile 先建立核心、三份使用者映像及磁碟，再啟動 QEMU。WUFS 磁碟為 `build/wuwaos.img`，交換區為 `build/wuwaos-swap.img`。`make clean` 保留磁碟；不要為重新建置而手動刪除它們，裡面可能有你的檔案。

## 進入系統後

看到開機動畫和 `wuwaos>` 後，以下指令才是在 WUWAOS 裡輸入：

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
