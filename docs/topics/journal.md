# WUFS Journal 與安全替換

一次更新 inode、allocation bitmap、檔案資料與目錄項目時，若只完成其中一部分，檔案系統可能留下彼此不一致的結構。WUFS 將 transaction 的目標 sectors 與完整 sector payload 先保存到 journal，之後才寫 home sectors；重啟時可用有效的 committed journal redo 尚未完成的更新。

<script setup>
import JournalLab from '../.vitepress/theme/components/JournalLab.vue'
</script>

<JournalLab />

## 課程概念與 WUFS 的選擇

使用者提供的《File-System Implementation》在 Transaction 章節指出：**「committed 不代表真正的 file system structure 已經全部更新完。」**同章的 Crash Recovery 段落說明，重啟時可以 redo 已提交但尚未全部套用的交易，忽略未提交的交易。原始概念與上下文見 [`notes/chapter 14 File-System Implementation.md`](https://github.com/johnson931128/WUWAOS/blob/main/notes/chapter%2014%20File-System%20Implementation.md#L1945-L1968) 的「Transaction 和 committed 是什麼」及「Crash 發生時怎麼 recovery？」。

這是課程中的 journaling 概念；WUFS 的具體格式與錯誤規則由目前 source 定義。WUFS 使用**固定容量、redo 型、full-sector payload journal**：target table 指定要更新哪些 home sectors，payload 保存這些 sector 的完整新內容，payload 也可能包含檔案 data。它不是只記 metadata 的 journal，也不是 log-structured file system；checkpoint 後仍寫回原本的 home sectors。

## transaction 的實際順序

外層 transaction 以 FS scope 序列化檔案系統操作。巢狀的 FS API 共用同一個 transaction depth；只有最外層成功結束時才 commit。`write_sector` 不直接改 home sector，而是把資料放進核心 RAM `transaction_payload`，並在 target table 中記錄 sector。這個 staging 階段若操作或容量檢查失敗，transaction 不進入 commit。

`transaction_commit` 的持久化次序如下：

1. 寫完整 target-sector table，再寫每個 sector 的 payload。
2. `block_flush()`，確保 table 與 payload 先到穩定儲存。
3. 開始寫 commit header，再 flush。**開始呼叫 header sector write 的時刻**先設定 `header_attempted`；自此之後任何 commit I/O 錯誤都保守回 `ERR_UNCERTAIN`。
4. 把 payload 寫到真正的 home sectors，再 flush。
5. 清除 journal header，再 flush。完成後交易結果才確定回傳。

header write 加 flush 成功代表交易已 committed，意即核心已承諾要完成這組更新。它不代表 home sectors 已經全部更新。header 開始寫入後，I/O 錯誤可能發生在 header 是否完整持久、部分 home write 或 flush 等不同位置；核心一律將 FS 設成 offline，回 `ERR_UNCERTAIN`（-200），不以錯誤回傳推論磁碟必定維持舊版，也不宣稱新版必定已全部完成。header write 呼叫前的 I/O 失敗則回 `ERR_IO`（-5），同樣 offline；由於 home writes 尚未開始，該交易的 home sectors 保持原內容。

上面的互動將 home card 當作磁碟 sector 的逐步快照。runtime transaction 持有 FS Gate；其他 FS caller 不會同時穿插讀取這些中間 home 狀態。故障後 FS offline，亦不允許 caller 繼續讀寫該卷。

## 開機 recovery 與不確定結果

掛載 WUFS0002 時，核心先讀 journal header。全零表示沒有待回復交易；非零 header 必須通過格式、版本、committed flag、count、reserved bytes、target 範圍、target 不重複、未用 table 項目與 CRC 檢查。核心先驗證**所有**紀錄，再寫任何 home sector；驗證通過後將每筆完整 payload redo 到 target sectors、flush，再清除 header 並 flush。

因此，header 前錯誤例子中的零 header 表示 journal payload 不會套用，home 保持舊資料。若 header write 已開始但失敗，磁碟 header 可能完整、未完整或無效；ERR_UNCERTAIN 本身不能保證 recovery 一定會重播。只有有效的 committed header、table、payload 與 CRC 全數通過時才會 redo。撕裂或無效 journal 會讓 WUFS 拒絕掛載並將卷標為 offline，而不猜測或套用部分內容。

## `fs_replace` 的原子路徑

`fs_replace(path, buffer, length)` 將一份新檔案內容與其 metadata 加入**同一筆 redo transaction**。替換現有檔案時先建立新 inode 和 data sectors，將新內容完整 stage；之後才 stage directory entry 指向新 inode、parent metadata，以及舊 inode／data 的釋放。最後 transaction commit 將這些 target sectors 一起 checkpoint。路徑不存在時可建立新檔；目標是 directory 或仍由 OFD 開啟時拒絕替換（open 目標回 `ERR_BUSY`，directory 回錯）。

空間不足、sector target 超出 1152-sector journal 容量，或新舊資料並存時無足夠磁碟空間，會在 header 發布前中止 staging，舊 directory entry 保持不變。替換需要先保留舊版並配置新版所需空間，並受 WUFS inode/data 與單檔容量限制；這不是任意大小檔案的通用快照功能。

`SYS_FS_REPLACE` 的 Fan 呼叫會先將使用者 buffer 複製到核心 staging，再執行 `fs_replace`。成功時回傳新內容長度。Fan 若收到一般儲存錯誤，保留編輯 buffer 並顯示失敗；若收到 `ERR_UNCERTAIN`，同樣保留 buffer，但禁止直接重試，要求重新開啟檔案確認磁碟上是哪一版，避免在結果不明時重複執行寫入。

## 欄位、參數與回傳值

| 名稱 | 作用 |
| --- | --- |
| `transaction_depth` | 巢狀 FS API 共用外層交易；只有深度歸零時才進入 commit。 |
| `transaction_count` / `transaction_targets[]` | 目前交易的不同 target sectors；重寫同一 sector 時沿用同一 payload slot。 |
| `transaction_payload` | 核心 RAM 中每個 target sector 的完整 512-byte 新內容。 |
| `FS_JOURNAL_CAPACITY` | 最多 1152 個 sector payload target；table 與 header 使用額外 journal sectors。 |
| journal header | 含 `WUJNL002` magic、版本、target count、committed flag 與 CRC；全零表示沒有 pending journal。 |
| `path` | `fs_replace` 的 WUFS 路徑；SYS API 接收使用者指標並先複製到核心 buffer。 |
| `buffer` | 新檔案 bytes；`length > 0` 時必須是有效來源。SYS 進入核心後複製資料，不把 user pointer 留在 transaction 中。 |
| `length` | 新內容 byte 數，不能超過 syscall staging 與 FS 單檔限制。 |
| `ERR_IO` | -5；commit header write 尚未開始前 I/O 失敗，交易未提交，FS offline。 |
| `ERR_UNCERTAIN` | -200；header write 呼叫已開始後遇到 I/O 失敗，磁碟結果不確定，FS offline。 |

```c
static int transaction_begin(void);
static int transaction_commit(void);
static int transaction_end(long operation_result);
static int journal_recover(uint32_t sectors);
long fs_replace(const char *path, const void *buffer, size_t length);
long fs_replacefile(const char *path, const char *buffer, unsigned long length);
```

使用者 `fs_replacefile` 包裝 `SYS_FS_REPLACE`（WUWAOS syscall 編號 41），傳入 `a0=path`、`a1=buffer`、`a2=length`。核心 syscall 先驗證路徑、長度與使用者 buffer，複製至 `fs_staging` 後呼叫 FS API。成功回傳實際替換的 byte 數（空檔為 0）；失敗回傳負錯誤碼。syscall ABI 非 Linux syscall ABI。

| 函式 | 成功結果 | 重要錯誤／限制 |
| --- | --- | --- |
| `transaction_begin()` | `0`，開始或加入巢狀交易 | FS 未掛載／offline 或 staging 不可用回 `-5`；唯讀卷回 `-30`。 |
| `transaction_end(result)` | 僅外層成功交易會執行 commit；回傳 commit 結果 | 操作失敗或 staging error 時略過 commit，回傳相應錯誤。 |
| `transaction_commit()` | 全部步驟成功回 `0` | header attempt 前 I/O error 回 `-5`；attempt 後 I/O error 回 `ERR_UNCERTAIN=-200`，FS offline。 |
| `journal_recover(sectors)` | 無待處理 header 回 `0`；有效 journal replay 完成回 `1` | header、範圍或 CRC 無效、I/O／flush 失敗回負值；掛載拒絕。 |
| `fs_replace(path, buffer, length)` | 回傳替換 byte 數 | open 目標 `ERR_BUSY`；空間／journal 容量不足時 abort；commit I/O 錯誤依 header attempt 邊界回 -5 或 -200。 |

## Source 對照

| 路徑 | 實作位置 |
| --- | --- |
| transaction staging、commit 次序、uncertain 邊界、recovery 驗證與 replay | [`kernel/fs/fs.c`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L24-L28)、[`transaction_commit` / `transaction_end` / `journal_recover`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L109-L214) |
| sector write 的 RAM staging 與容量檢查 | [`write_sector`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L246-L266) |
| boot mount 時的 journal recovery | [`fs_init`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L790-L847) |
| 新 inode/data、directory switch、open-file busy 檢查 | [`fs_replace_impl`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L1106-L1176) |
| FS public API 與巢狀 transaction 包裝 | [`fs_replace`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.c#L1392-L1400)、[`kernel/fs/fs.h`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fs/fs.h#L18-L19) |
| syscall staging、user copy 與 result | [`SYS_FS_REPLACE`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/syscall/syscall.c#L264-L282)、[`user/lib/api.c`](https://github.com/johnson931128/WUWAOS/blob/main/user/lib/api.c#L98-L100) |
| syscall number 與錯誤碼 | [`include/syscall_numbers.h`](https://github.com/johnson931128/WUWAOS/blob/main/include/syscall_numbers.h#L45-L75) |
| Fan 保留 dirty buffer 並停止 uncertain retry | [`user/apps/fan/main.c`](https://github.com/johnson931128/WUWAOS/blob/main/user/apps/fan/main.c#L173-L188)、[`uncertain save handling`](https://github.com/johnson931128/WUWAOS/blob/main/user/apps/fan/main.c#L238-L250) |

互動圖是依目前 source 建立的固定教學模型，並非故障注入或磁碟 trace。它展示的 reboot replay 情境明確假設 committed header、target table、payload 與 CRC 都有效；失敗的 header 發布可能留下無效紀錄，WUFS 對此拒絕掛載，不會把所有 uncertain 結果演成必定成功。
