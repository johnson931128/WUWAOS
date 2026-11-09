# 09 · WUFS 檔案系統

磁碟只提供編號磁區，不知道 `/home/user/note.txt` 是什麼。WUFS 是 WUWAOS 自訂的檔案系統，將路徑轉成 inode，再找到存放內容的磁區。

[Journal 與安全替換互動章節](./journal.md) 將 staging、commit header、checkpoint、uncertain 與重播放在同一條可單步操作的時間線；本章說明磁碟布局與檔案介面。

## 從名稱到硬體

```text
shell 的 cat／write／fan
  → 使用者 API → 檔案系統 syscall
  → fs.c：路徑與 inode → block.c：512-byte 磁區
  → VirtIO-MMIO → QEMU 選取的 DISK_IMAGE
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

WUFS0002 在 superblock 後預留固定 redo journal：LBA 1 是 header，LBA 2–10 是最多 1152 個目標 LBA，LBA 11–1162 是各 512-byte 的完整 sector images；inode bitmap 從 1163 開始，16 MiB volume 的 block bitmap 為 1164–1171、inode table 為 1172–1191、data 從 1192 開始。WUFS0001 保留舊布局，kernel 僅在驗證後唯讀掛載，不做破壞性升級。

inode 有 8 個直接指標與 8 個單層間接指標。每個間接區塊可放 128 個 4-byte 指標，因此最大內容是 `(8 + 8 * 128) * 512 = 528384 bytes`，約 516 KiB；不是無上限大檔案。

## 格式辨識與空白磁碟保護

WUFS 在第一個磁區開頭使用 8-byte 格式標記（magic）：舊版 `WUFS0001` 或目前新格式 `WUFS0002`。標記只能辨識格式，不能單獨證明檔案系統完整。kernel 會先精確驗證 superblock layout；V2 會先檢查並重播 journal，再驗證 inode、bitmap、pointer 與 directory tree。

目前原始碼對 magic 不符的磁碟採保留資料的處理方式：

| 判斷 | 處理 |
| --- | --- |
| 整顆磁碟每個 byte 都是零 | 才允許進入自動格式化流程 |
| 任何位置存在非零資料 | 拒絕格式化，保留原有磁碟內容 |
| 讀取失敗，無法確認是否空白 | 返回錯誤，不進入格式化流程 |
| 容量超過 WUFS 支援範圍或不足以放 V2 metadata | 拒絕自動格式化 |

`disk_is_blank` 只讀取資料，逐一檢查所有 512-byte 磁區；找到非零 byte 或讀取錯誤即停止。只有全部讀取成功且皆為零才認定為空白。不能只看第一個磁區，因為檔頭可能已清零，但後面的 inode 或檔案內容仍存在。全零磁碟的最壞掃描成本為 O(磁碟 bytes)，kernel 額外使用一個 512-byte 緩衝區；正常 WUFS 掛載不走這條掃描路徑。

主機上的 `tools/make_disk.py` 僅更新通過 V2 layout、journal-header 與 metadata 驗證的映像。它不處理 pending journal，也不覆寫壞格式。修改先在 RAM 完成，再以同目錄暫存檔、fsync 和 replace 更新目標。WUFS0001 不更新；一般 `make run` 若選到 V1 仍能啟動唯讀核心。

未知非空白磁碟、無效非零 journal header、錯誤 layout 或壞 inode pointer 都拒絕掛載，不會自動清除或修復。全零 disk 才會建立 V2。初始化保護見 [Spec 31](../SPEC-31-filesystem-initialization-guard.md)，transactions、journal/replay 與實際 QEMU fault 驗收見 [Spec 32](../SPEC-32-wufs-redo-journal.md)。WUFS0002 與 journal 後於 portable v1；既有 ZIP 不包含這些改動。

## 路徑查找與讀寫

`kernel/fs/fs.c` 從根目錄開始，逐段查找名稱與 inode，最後依檔案內 offset（位移）找直接或間接區塊。建立檔案要配置 inode，擴展內容還要配置資料區塊與必要的間接區塊。`open` 建立的 OFD 保存 inode 和共享 offset，`read`／`write` 逐段存取；`O_APPEND` 寫入時從當下檔尾開始。

runtime FS 操作由可重入 filesystem scope 保護共享 metadata 與 redo transaction staging；SYS_FS_READ／WRITE／REPLACE 的 scope 也涵蓋共用 `fs_staging` 和 usercopy，`elf_load` 則跨檔案大小查詢與全部 segment 讀取持有同一 scope。`fs_init` 是 boot-only 的一次性 mount/format/replay，並非 runtime remount API。Gate 順序為 OFD/Pipe → FS → VM → Block；磁碟完成等待會讓出 CPU，但 FS scope 保留至原呼叫完成，避免其他行程混用 transaction。FS 的 inode-open 檢查不等待 OFD Gate。

由於 inode 為 80 bytes、磁區為 512 bytes，有些 inode 會跨磁區。`inode_read`／`inode_write` 依每個磁區剩餘長度分段搬移；transaction 會在 RAM staging 中以 read-your-writes 覆蓋完整 sector，保護旁邊 inode 並讓掛載後的修改可回復。

核心介面的宣告見 [kernel/fs/fs.h（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.h)：初始化、列目錄、建立目錄／檔案、讀寫、刪除與複製。

`rm /absolute/file` 移除一般檔案：從父目錄移掉名稱，再回收檔案的資料區塊與 inode（檔案資訊）。為保持目錄項目連續，核心會把最後一項移到刪除位置；目錄本身的既有資料區塊保留，之後的項目可重用。`rm` 遇到資料夾會顯示路徑和警告，輸入 Y/y 再按 Enter 才刪除該資料夾與所有內容；N/n 或其他輸入取消。根目錄、`.`、`..` 不能刪除，若任何活躍行程正使用該目錄樹中的工作目錄則回傳 -16。詳見 [Spec 20](../SPEC-20-directory-removal.md)。

`cp /absolute/source /absolute/destination` 用 512-byte 緩衝區在同一 transaction 內建立新 inode、複製、發佈目錄項並回收舊目的檔。交易提交前的空間不足會回 `-28` 並保留舊目的檔內容；同一 inode 複製仍被拒絕。每筆最多觸及 1152 個不同磁區，超出上限整筆拒絕。`remove_tree` 也以單一 bounded transaction 提交；若提交途中 I/O 失敗，volume 會 offline，重啟後由有效 journal replay 完成整筆交易，回錯不等於操作一定未發生。

Shell 同時保留 path-based 的 `fs_readfile`／整檔 `fs_writefile` 與 open／close／read／write 描述號介面。`dup`、`dup2` 與 fork 共用同一 OFD offset；不提供 seek。一般覆寫、truncate、移除或替換若目標 inode 正被開啟，回 busy（-16）；尚不支援 unlink 後延遲回收。FD 每次 write 最多 512 bytes，各自獨立提交；`O_TRUNC` 與後續 FD write 不是一筆交易，因此 shell `>` 不會在後續失敗時還原舊內容。

## 安全替換與不確定結果

`fs_replace`／使用者 API `fs_replacefile`（syscall 41）在單一交易中配置新 inode、寫完新資料，再切換目錄項並回收舊 inode，不先清空舊檔。缺檔時建立，資料夾或開啟中的目標拒絕替換；新舊資料同時存在於 staging，所需磁碟空間與 journal 容量可能高於一般覆寫。容量不足時整筆 abort，不能宣稱支援任意大小的原子替換。`fan` 儲存改用此介面。

交易提交依序為 payload → flush → commit header → flush → home sectors → flush → clear header → flush。整個 commit 延後取消，避免 kill 中斷持久性協定。header 開始嘗試寫入前的 I/O 失敗回 `ERR_IO`（-5）；從 header 寫入嘗試開始，失敗一律保守回 `ERR_UNCERTAIN`（-200）並令 FS offline。此時可能已提交，需重啟讓 journal replay 後重新讀取確認，不能直接重試或宣稱一定保存失敗。

Fan 收到不確定結果時保留未儲存 buffer，提示重新開啟確認並阻止再次存檔。Block／FS／GPU 關鍵故障使用統一 UART 診斷格式，包含 subsystem、operation、error、uncertain 與 detail，不經可能失效的 GUI TTY。這些 2026-11-08 新路徑未執行 runtime 或故障注入測試（NOT_RUN）。

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

`tools/make_disk.py` 在主機維護 16 MiB 原始磁碟映像，通過 V2 驗證後保留使用者檔案並更新 `/boot/kernel.elf`。`make run-disk` 的載入器會從這個檔案開機，預設使用 `build/wuwaos-cli.img`。直接開機的 `make run` 由 QEMU 載入核心，可搭配既有 WUFS0001 唯讀映像，也可用新的 `DISK_IMAGE` 建立可寫的 WUFS0002 磁碟。 [開機流程](./boot.md)

make disk-image 先產生 kernel-disk.elf，移除符號與除錯資料後才存入 WUFS，以免 ELF 超過單檔容量；QEMU 與 GDB 仍使用保留除錯資料的 kernel.elf。這不是從磁碟開機。

V2 journal 以 VirtIO FLUSH 作為持久性屏障。若裝置沒有 FLUSH，乾淨 V2 僅唯讀掛載；pending header 會拒絕掛載。資料內容沒有獨立 checksum，非零損毀 header 不自動修復；真實斷電與所有硬體 flush 行為未在此驗收。尚無重新命名、權限、連結或掛載其他格式。

若預設磁碟仍是 V1，可以使用新檔名開機，例如 `make run-disk BOOT_DISK_IMAGE=build/wuwaos-cli-v2.img`。若映像有待重播的 journal，使用 QEMU 直接載入核心並附加該磁碟，讓核心重播 journal。 [Spec 32 的復原指令](../SPEC-32-wufs-redo-journal.md#新磁碟與-pending-recovery-操作)

觀念參考：`notes/Chapter 13 file system Interface.md`、`notes/chapter 14 File-System Implementation.md`、`notes/chapter 15  File System  Internals.md`。WUFS 格式與既有持久性驗收見 [Spec 06](../SPEC-06-filesystem.md)。
