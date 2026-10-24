# Spec 06：WUWAOS 簡易檔案系統與磁碟 I/O

## 目標

在 QEMU 上接入可持久保存的虛擬磁碟，並建立 WUWAOS 自有的簡易檔案系統。Linux 風格的 `/bin`、`/boot`、`/dev`、`/home/user` 只代表目錄命名慣例，不要求 Linux 格式相容。

## 範圍

- 實作 QEMU `virt` 平台上的 VirtIO-MMIO 區塊裝置讀寫，從 OpenSBI 傳入的 FDT（硬體裝置樹）尋找裝置，不硬編碼裝置位址。
- 實作固定 512-byte 區塊的 WUFS 格式：superblock、inode bitmap、block bitmap、固定 inode 表、直接與單層索引區塊指標、目錄項目。檔案上限約 516 KiB；使用小型、固定上限，先不做動態掛載、多磁碟、權限位元、連結、日誌或快取。
- 建立根目錄與 `/bin`、`/boot`、`/dev`、`/home`、`/home/user`。首次初始化空白磁碟時建立；辨識到既有 WUFS 後保留使用者資料。
- 提供路徑查找、列目錄、建立目錄／檔案、讀取與覆寫一般檔案。shell 至少可用 `ls`、`mkdir`、`touch`、`cat`、`write` 操作絕對路徑。第一版採專用 path-based（以路徑為參數）系統呼叫，尚未實作 POSIX 檔案描述號與 `open/close`。
- 把目前建置的 `kernel.elf` 作為一般檔案 `/boot/kernel.elf` 放入虛擬磁碟。此階段依然由 QEMU `-kernel` 載入正在執行的核心；磁碟中的核心副本不負責開機。
- block I/O 採單核心同步輪詢；尚無 block cache、DMA IOMMU、多核心鎖或磁碟中斷處理。
- 不新增測試程式碼。使用一般 shell 命令操作，並在重新啟動 QEMU 後確認文字檔保留。

## 分層

```text
user shell command
  → user syscall wrapper
  → kernel syscall / VFS-style path interface
  → WUFS inode and directory operations
  → block read/write API
  → VirtIO-MMIO block driver
  → QEMU raw disk image
```

UART console 與 pipe 保持為其他 I/O 端點，不經過 WUFS。檔案操作、終端操作、pipe 各自使用明確系統呼叫／介面；目前 `kernel/io.c` 管理 console 與 pipe，`kernel/block.c` 提供的 VirtIO 區塊 I/O 則只供磁碟檔案系統使用。兩者都屬於核心的輸入輸出功能，但尚未整合成通用裝置或檔案描述號架構。

## 驗收條件

- 核心可識別 QEMU VirtIO block device，讀寫區塊成功；裝置未出現或回報錯誤時輸出診斷並繼續以 shell 啟動，檔案功能回報不可用。
- 第一次使用新磁碟建立目錄樹與 `/boot/kernel.elf`；再次啟動時保留磁碟內容而不重格式化。
- `ls /` 可見預設目錄；可建立 `/home/user/note.txt`、寫入、讀回內容，關閉 QEMU 後重開仍讀到相同內容。
- 建置工具更新 `/boot/kernel.elf` 時保留其他 inode 與檔案內容；QEMU 實際執行中的核心仍由 `-kernel` 載入。
- 既有 QEMU 開機、shell、行程生命週期與固定 pipe 行為不回歸。
- 文件說明 WUFS 格式限制、IO 路徑及 kernel file 與目前直接開機方式的差異。

## 實作與驗收紀錄

2026-10-19：Clang 18／QEMU 8.2.2／OpenSBI 1.3，單核心 `virt`。核心由 OpenSBI 傳入 FDT（硬體裝置樹）尋找 VirtIO-MMIO block，確認 VERSION_1 後設定單一同步輪詢佇列；QEMU 使用 `-global virtio-mmio.force-legacy=false`。核心映射發現的裝置頁面後，WUFS 才讀寫磁碟。

- `make run` 建立或更新 16 MiB raw disk；更新 `/boot/kernel.elf` 時原有 WUFS 會保留其他檔案。WUFS 共 128 inode，固定目錄項目每個名稱 24 bytes，支援 8 個直接區塊與 8 個單層索引表，每檔最多 1032 個 512-byte 區塊。
- QEMU 顯示 `VirtIO block ready, sectors=32768`、`Mounted existing WUFS volume`。`ls /` 列出 bin、boot、dev、home，`ls /boot` 列出 kernel.elf。
- 建立 `/home/user/notes.txt`，寫入 `WUWAOS-filesystem`，退出 QEMU 後再次 `make run`，讀回相同 17 bytes；再建立 `/home/user/notes/today.txt`，寫入並讀回 `filesystem-works`。重複建立既有 notes 目錄回 -17。
- `make` 通過 `-Wall -Wextra -Werror`；實際 QEMU 操作不是測試程式碼，未新增測試檔。

限制：第一版 shell 的 `write` 只接受一個不含空白的文字 token，`cat` 顯示上限 2048 bytes；若顯示內容非空且未以 LF（換行字元）結尾，shell 會在輸出後補一個換行，不改動檔案內容。syscall 每次最多處理單一使用者資料頁，因此目前互動寫入最多 4 KiB。沒有刪除／改名／追加、相對路徑、cwd、檔案描述號、開機載入、掉電交易保護或壞磁碟復原。block I/O 同步輪詢，失效裝置可能讓核心等待；目前沒有 block cache。
