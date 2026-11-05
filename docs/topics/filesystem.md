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

目前目錄記在各行程的 PCB。相對路徑先接上目前目錄，核心再逐段查找，包含 `.` 和 `..`；`cd` 只有在目標查找成功且是目錄時才更新位置。`pwd` 可以查看，詳細命令及流程見 [Shell 章節](./applications.md)。

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

## 格式辨識與空白磁碟保護

WUFS 在第一個磁區開頭使用 8-byte 格式標記（magic）`WUFS0001`。它用來辨識格式，不是密碼，也不能單獨證明檔案系統完整。magic 相符後，kernel 仍會檢查既有 superblock 欄位。

目前原始碼對 magic 不符的磁碟採保留資料的處理方式：

| 判斷 | 處理 |
| --- | --- |
| 整顆磁碟每個 byte 都是零 | 才允許進入自動格式化流程 |
| 任何位置存在非零資料 | 拒絕格式化，保留原有磁碟內容 |
| 讀取失敗，無法確認是否空白 | 返回錯誤，不進入格式化流程 |
| 容量回報達到 32-bit 飽和值 | 無法保證完整掃描，拒絕自動格式化 |

`disk_is_blank` 只讀取資料，逐一檢查所有 512-byte 磁區；找到非零 byte 或讀取錯誤即停止。只有全部讀取成功且皆為零才認定為空白。不能只看第一個磁區，因為檔頭可能已清零，但後面的 inode 或檔案內容仍存在。全零磁碟的最壞掃描成本為 O(磁碟 bytes)，kernel 額外使用一個 512-byte 緩衝區；正常 WUFS 掛載不走這條掃描路徑。

主機上的 `tools/make_disk.py` 也會先檢查既有映像：WUFS 映像依原有流程更新，未知且非空白的映像則報錯退出，不寫回檔案。這一層不可省略，因為一般 `make run` 在 kernel 啟動前就會執行該工具。

完整設計與四種驗證案例見 [Spec 31](../SPEC-31-filesystem-initialization-guard.md)。本次機制已實作，編譯與 QEMU 驗收結果尚未記錄；portable v1 發行包不包含這次後續修正。它也不提供 fsck、資料復原或掉電交易保護。

## 路徑查找與讀寫

`kernel/fs.c` 從根目錄開始，逐段查找名稱與 inode，最後依檔案內 offset（位移）找直接或間接區塊。建立檔案要配置 inode，擴展內容還要配置資料區塊與必要的間接區塊。`open` 建立的 OFD 保存 inode 和共享 offset，`read`／`write` 逐段存取；`O_APPEND` 寫入時從當下檔尾開始。

由於 inode 為 80 bytes、磁區為 512 bytes，有些 inode 會跨磁區。`inode_read`／`inode_write` 依每個磁區剩餘長度分段搬移；寫入先讀出原磁區再修改，保護旁邊 inode 的內容。

核心介面的宣告見 [kernel/fs.h（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs.h)：初始化、列目錄、建立目錄／檔案、讀寫、刪除與複製。

`rm /absolute/file` 移除一般檔案：從父目錄移掉名稱，再回收檔案的資料區塊與 inode（檔案資訊）。為保持目錄項目連續，核心會把最後一項移到刪除位置；目錄本身的既有資料區塊保留，之後的項目可重用。`rm` 遇到資料夾會顯示路徑和警告，輸入 Y/y 再按 Enter 才刪除該資料夾與所有內容；N/n 或其他輸入取消。根目錄、`.`、`..` 不能刪除，若任何活躍行程正使用該目錄樹中的工作目錄則回傳 -16。詳見 [Spec 20](../SPEC-20-directory-removal.md)。

`cp /absolute/source /absolute/destination` 用 512-byte 緩衝區複製一般檔案；目的檔存在時，先寫完整的新 inode，成功連入目錄後才回收舊目的檔。複製到同一 inode 會被拒絕，避免清空來源。要同時容納新舊目的檔，因此剩餘空間不足可能回報錯誤。磁碟異常或斷電時仍可能留下未回收區塊或部分目錄更新，WUFS 尚無 journal（交易日誌）；見 [Spec 18](../SPEC-18-file-commands.md)。

Shell 同時保留 path-based（直接以路徑呼叫）的 `fs_readfile`／整檔 `fs_writefile`，並新增通用 open／close／read／write 描述號介面。`dup`、`dup2` 與 fork 共用同一 OFD offset；不提供 seek，描述號採固定容量。一般整檔覆寫、truncate、rm、rm tree 和 cp 覆寫若目標 inode 正被開啟，回 busy（-16）；本版不支援 unlink 後延遲回收。

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

`echo hello > /home/user/out.txt` 會建立或截斷目的檔，`echo again >> /home/user/out.txt` 會附加；若 stage 執行前的開啟成功而後續 fork 失敗，已建立或截斷的目的檔狀態不回復。完整管線與 I/O 說明見 [Spec 27](../SPEC-27-file-descriptors-and-shell-pipelines.md)。

## 持久性與限制

`tools/make_disk.py` 在主機維護 16 MiB 原始磁碟映像，已有 WUFS 時保留使用者檔案並更新 `/boot/kernel.elf` 副本。不要用刪除映像檔的方式重新建置。

make disk-image 先產生 kernel-disk.elf，移除除錯資料後才存入 WUFS，以免除錯段超過單檔容量；QEMU 與 GDB 仍使用保留除錯資料的 kernel.elf。這不是從磁碟開機，也沒有改變 WUFS 格式。

尚無重新命名、權限、連結、掛載其他格式、journal（日誌）或掉電交易保護。資料夾刪除若中途發生磁碟錯誤，可能只刪除一部分，無法自動復原。資料持久保存不等於每一次寫入都具備原子性。

觀念參考：`notes/Chapter 13 file system Interface.md`、`notes/chapter 14 File-System Implementation.md`、`notes/chapter 15  File System  Internals.md`。WUFS 格式與既有持久性驗收見 [Spec 06](../SPEC-06-filesystem.md)。
