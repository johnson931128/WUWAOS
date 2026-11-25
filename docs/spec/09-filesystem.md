---
next: false
---

# 09 · 資料怎麼存下來：WUFS 與磁碟復原

WUFS 是 WUWAOS 的磁碟檔案系統。使用者用 `/home/user/note.txt` 這樣的路徑找檔案。核心逐層查目錄，每筆目錄項目把名字對應到 [inode](./glossary.md#inode)。inode 記錄檔案類型、大小和資料存放的磁區。這一層決定資料放在哪裡，[第 07 章](./07-io.md)的驅動負責把資料送到磁碟。

## 檔名如何變成磁碟位置

檔案系統提供兩層關係。目錄讓核心把名稱查到 inode，inode 再提供檔案長度、類型與資料配置。以 `/home/user/note.txt` 為例，核心依序在根目錄查找 `home`、在該目錄查找 `user`、再查 `note.txt`。找到 inode 後，讀取才會依檔案偏移定位需要的磁區。目錄名稱、開啟狀態與資料位置有各自的生命週期。 [路徑處理](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/path.c) · [WUFS 實作](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c)

## 格式與檔案生命週期

| 磁碟格式 | 現行支援 |
| --- | --- |
| WUFS0001 | 保留舊格式的唯讀相容。 |
| WUFS0002 | 有 redo journal，可讀寫。主機工具預設建立此版本。 |
| WUFS0003 | 沿用 journal，支援 hard link 與 console 裝置節點。可建立新映像，或由乾淨的 WUFS0002 另存升級。 |

在 WUFS0003，下 `ln` 可讓兩個目錄名稱指向同一個 inode。[Hard link](./glossary.md#hard-link) 讓它們共用資料。移除名稱後，已開啟的 FD 仍能存取檔案。最後一個名稱移除、最後一份開啟引用解除後，核心才可回收 inode 和資料磁區。

## 寫入到一半重啟怎麼辦

更新檔案時，核心可能同時更動目錄、inode 和資料磁區。若其中一部分先落盤，重啟後可能找不到檔案或無法判斷哪些磁區已使用。WUFS0002／0003 用 [redo journal](./glossary.md#redo-journal) 留下這一批要寫入的完整磁區內容：

1. 先寫 journal 的目標磁區表和資料，執行 [flush](./glossary.md#flush) 等待裝置確認。
2. 寫入並確認 commit header。它表示 journal 有一筆待重播的交易。
3. 把磁區套用到正式位置，確認落盤後清除 header。
4. 完整核心掛載時，若發現待重播交易，先驗證目標範圍和校驗資料，再依 journal 重寫正式位置。

開始寫入 commit header 後若發生 I/O 錯誤，操作可能已提交，核心回 `ERR_UNCERTAIN` 並把磁碟標為不可用。Flash 載入器會拒絕待重播的系統磁碟，復原入口見原本的 [WUFS 復原流程](../SPEC-32-wufs-redo-journal.md#新磁碟與-pending-recovery-操作)。對無法辨識的非空白磁碟，主機和核心都會保留原資料並拒絕自動格式化。

現行 CLI 回歸驗證了 WUFS0003 另存升級、hard link、最後一個引用回收、重啟持久性和 redo 重播。各種損毀、逾時與中途失敗的邊界分別留在詳細紀錄。 [WUFS 核心](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/fs/fs.c) · [主機磁碟工具](https://github.com/johnson931128/WUWAOS/blob/3a3b273/tools/make_disk.py) · [磁碟保護](../SPEC-31-filesystem-initialization-guard.md) · [Journal 詳細規格](../SPEC-32-wufs-redo-journal.md) · [驗收程式](https://github.com/johnson931128/WUWAOS/blob/3a3b273/tools/test_kernel_cli.py)

### 用 fan 儲存看提交與重播

`fan` 透過安全替換路徑保存內容。核心先準備新內容與相關更新，再透過 redo journal 把需要一起完成的磁區寫入記錄。交易提交後，正式磁區可能仍在更新中。若此時重新掛載，有效的已提交紀錄可在核心或主機工具支援的復原流程中重播。重播前會先驗證記錄，避免套用無效的目標或資料。

這也解釋了為什麼「已提交」與「全部寫入正式位置」分成兩個時刻。測試映像可以刻意保留已提交交易，確認下一次啟動能讀到更新後的檔案。現有紀錄另有滿盤時保留原檔、釋放空間後重試成功的案例。 [儲存與復原驗收](https://github.com/johnson931128/WUWAOS/blob/main/planning/cli-readiness.md) · [交易提交程式碼](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L153)

## 動手觀察

[Journal 與安全替換](../topics/journal.md)可逐步查看一筆交易落盤，並比較不同階段中斷後是否能重播。 [磁碟格式與 Shell 操作補充](../topics/filesystem.md)

## 恐龍書與課堂筆記

恐龍書第 10 版第 13–15 章，journal 復原見 §14.7。VFS、NFS 與 Linux 物件模型提供其他檔案系統的設計例子。

- [檔案介面、目錄與保護](../theory/13-file-interface.md)
- [磁碟配置、目錄實作、快取與復原](../theory/14-file-implementation.md)
- [VFS、Linux 檔案物件與 NFS](../theory/15-file-internals.md)
- [期末名詞表與複習例子](../theory/final-glossary.md)
