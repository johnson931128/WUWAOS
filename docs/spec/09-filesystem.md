---
next: false
---

# 09 · 資料怎麼存下來：WUFS 與磁碟復原

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

磁碟本身只是一排編號從 0 到 $N$ 的 512-byte 磁區。作業系統如何把這排冰冷的區塊變成有目錄樹、檔名與權限的檔案系統？更致命的是：**如果更新一個檔案需要同時修改目錄、inode 與資料區塊，寫到一半突然斷電或重啟，磁碟會陷入半殘損毀狀態**——這正是 Journaling（日誌式檔案系統）誕生的原因。
</div>

## 恐龍書核心理論：Directory / Inode 分離與 Journaling

恐龍書第 13–15 章（File-System Interface & Internals）說明現代 UNIX 檔案系統的兩大支柱：

1. **檔名與實體分離（Directory Entry → Inode）**：目錄裡只存「檔名 → [inode](./glossary.md#inode) 編號」的對照表；真正的檔案大小、類型與資料磁區指標都記錄在 inode 裡。這讓多個檔名可以指向同一個 inode（[Hard Link](./glossary.md#hard-link)），也讓「刪除檔名但已開啟的 FD 仍能繼續讀寫，直到最後一個引用關閉才回收磁區」成為可能。
2. **日誌式復原（Write-Ahead / Redo Journaling）**：把一筆多磁區更新先當成一筆**交易（Transaction）**完整寫進磁碟上的日誌區並提交（Commit），再套用到正式位置。萬一中途斷電，重開機時只要重播已提交的日誌就能恢復一致性，不必全盤掃描 `fsck`。

## WUFS 格式與檔案生命週期

WUFS 是 WUWAOS 的磁碟檔案系統，目前版本演進如下：

| 磁碟格式 | 現行支援與能力 |
| --- | --- |
| `WUFS0001` | 保留舊格式的唯讀相容。 |
| `WUFS0002` | 具備 [redo journal](./glossary.md#redo-journal)，可讀寫；主機建置工具預設格式。 |
| `WUFS0003` | 沿用 journal，新增 **hard link（`ln`）** 與 console 裝置節點；可新建或由乾淨的 `WUFS0002` 另存升級。 |

在 `WUFS0003` 中，使用 `ln` 建立硬連結後，兩個目錄項目共享同一個 inode 與資料區塊；移除其中一個檔名不會刪除資料，直到最後一個檔名被移除**且**最後一個開啟該 inode 的 FD 關閉後，核心才透過 `fs_reap_orphans()` 正式回收 inode 與資料磁區。

## 寫入到一半重啟怎麼辦：WUFS Redo Journal 四步協議

當 `fan` 編輯器安全存檔（`fs_replace()`）或核心執行檔案寫入時，WUFS0002／0003 嚴格遵守四步協議：

1. **寫入日誌內容並 Flush**：先將目標磁區表與完整新資料寫入 journal 區域，執行 [flush](./glossary.md#flush) 等待磁碟裝置確認落盤。
2. **寫入並確認 Commit Header**：寫下提交標頭並再次 flush。**這一刻起交易正式生效**——即便立刻斷電，日誌裡已有足夠資訊重播。
3. **套用到正式位置並清除 Header**：把資料寫回正式 inode／目錄／資料磁區，確認落盤後才清除 commit header。
4. **掛載重播（Recovery Replay）**：完整核心開機掛載時，若偵測到未清除的 commit header，會先驗證目標磁區範圍與校驗值，再依 journal 重寫正式位置，確保檔案系統回到一致狀態。

<div class="tb-box wuwaos">
<span class="tb-label">故障安全邊界：絕不猜測、絕不誤格式化</span>

- **不確定狀態（`ERR_UNCERTAIN`）**：開始寫入 commit header 後若發生磁碟 I/O 錯誤，交易可能已在硬體端提交也可能沒有；核心回報 `ERR_UNCERTAIN` 並把磁碟標記為不可用，避免後續寫入擴大損毀。
- **開機保護**：Flash 載入器屬於唯讀精簡環境，會拒絕直接從帶有待重播交易的系統磁碟開機（須由直接開機的完整核心完成重播復原）。對無法辨識的非空白磁碟，工具與核心皆拒絕自動格式化以保護既有資料。
</div>

## 動手觀察

- [WUFS Journal 互動實驗室](../topics/journal.md)：逐步觀看一筆交易從寫入 journal、flush、commit 到套用落盤，並實際測試在各個階段斷電重啟後的復原結果。
- 延伸補充：[磁碟格式與 Shell 操作補充](../topics/filesystem.md)

原始碼與驗收：[WUFS 核心 (`kernel/fs/fs.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/fs/fs.c) · [API 介面 (`kernel/fs/fs.h`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/fs/fs.h) · [主機磁碟工具 (`tools/make_disk.py`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/tools/make_disk.py) · [磁碟保護](../SPEC-31-filesystem-initialization-guard.md) · [Journal 詳細規格](../SPEC-32-wufs-redo-journal.md) · [驗收程式](https://github.com/johnson931128/WUWAOS/blob/3a3b273/tools/test_kernel_cli.py)
