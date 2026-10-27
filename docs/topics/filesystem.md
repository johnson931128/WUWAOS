# 09 · WUFS 檔案系統

磁碟只提供編號磁區，不知道 `/home/user/note.txt` 是什麼。WUFS 是 WUWAOS 自訂的檔案系統，將路徑轉成 inode，再找到存放內容的磁區。

## 從名稱到硬體

```text
shell 的 cat／write／fan
  → 使用者 API → 檔案系統 syscall
  → fs.c：路徑與 inode → block.c：512-byte 磁區
  → VirtIO-MMIO → QEMU 的 build/wuwaos.img
```

Linux 風格的 `/bin`、`/boot`、`/dev`、`/home/user` 是目錄命名慣例，並不表示 WUFS 可被 Linux 直接掛載，也不表示這些目錄已具備 Linux 的全部語意。

## 磁碟資料結構

| 結構 | 負責什麼 |
| --- | --- |
| Superblock（檔案系統總表） | 記錄格式標記與布局 |
| Inode bitmap（索引節點位元圖） | 哪些 inode 已配置 |
| Block bitmap（資料區塊位元圖） | 哪些磁區已配置 |
| Inode table（索引節點表） | 128 個 inode，每個 80 bytes |
| Directory entries（目錄項目） | 將名稱對應到 inode |
| Data blocks（資料區塊） | 一般檔案內容或目錄資料 |

inode 有 8 個直接指標與 8 個單層間接指標。每個間接區塊可放 128 個 4-byte 指標，因此最大內容是 `(8 + 8 * 128) * 512 = 528384 bytes`，約 516 KiB；不是無上限大檔案。

## 路徑查找與讀寫

`kernel/fs.c` 從根目錄開始，逐段查找名稱與 inode，最後依檔案內 offset（位移）找直接或間接區塊。建立檔案要配置 inode，擴展內容還要配置資料區塊與必要的間接區塊。

由於 inode 為 80 bytes、磁區為 512 bytes，有些 inode 會跨磁區。`inode_read`／`inode_write` 依每個磁區剩餘長度分段搬移；寫入先讀出原磁區再修改，保護旁邊 inode 的內容。

核心介面的宣告見 [kernel/fs.h（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs.h)：初始化、列目錄、建立目錄／檔案、讀取與整檔覆寫。

這是 path-based（直接以路徑呼叫）介面，還沒有完整 open／close／seek 與通用檔案描述號表。`fs_write` 是整檔覆寫，沒有 append（附加寫入）。

## 在 QEMU 操作

```text
ls /
mkdir /home/user/notes
touch /home/user/notes/today.txt
write /home/user/notes/today.txt hello_WUWAOS
cat /home/user/notes/today.txt
```

若目錄已存在，mkdir 可能回 -17；不用再建立一次。write 的文字參數目前只能是一個不含空白的 token（解析單位）；有空白或多行內容請用 fan。

cat 會在非空內容末端缺少換行時補一個「顯示用換行」，防止提示字元黏在文字後；不會因此修改檔案內容。退出並重新啟動 QEMU 後，再 cat 應仍可讀回。

## 持久性與限制

`tools/make_disk.py` 在主機維護 16 MiB 原始磁碟映像，已有 WUFS 時保留使用者檔案並更新 `/boot/kernel.elf` 副本。不要用刪除映像檔的方式重新建置。

make disk-image 先產生 kernel-disk.elf，移除除錯資料後才存入 WUFS，以免除錯段超過單檔容量；QEMU 與 GDB 仍使用保留除錯資料的 kernel.elf。這不是從磁碟開機，也沒有改變 WUFS 格式。

沒有刪除、重新命名、權限、連結、掛載其他格式、journal（日誌）或掉電交易保護；資料持久保存不等於每一次寫入都具備原子性。

觀念參考：`notes/Chapter 13 file system Interface.md`、`notes/chapter 14 File-System Implementation.md`、`notes/chapter 15  File System  Internals.md`。WUFS 格式與既有持久性驗收見 [Spec 06](../SPEC-06-filesystem.md)。
