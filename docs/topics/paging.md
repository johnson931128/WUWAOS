# 07 · 按需分頁、置換與 Swap

程式保留了一段合法虛擬位址，不代表每一頁現在都占用 RAM。Demand paging（按需分頁）在真正需要資料時才準備頁面；RAM 頁框不足時，再透過 Page replacement（頁面置換）與 Swap（記憶體與磁碟交換）騰出空間。

## 一次合法缺頁

1. CPU 取指令、讀取或寫入，發現 PTE 不存在，觸發缺頁。
2. `trap_handler` 讀 scause（陷阱原因）與 stval（相關位址），交給 `vm_handle_fault`。
3. VM 檢查是否位於合法區域，且允許這種存取。
4. `resident_page` 取得頁框；從原始程式映像、零頁或 Swap 完整載入內容。
5. 成功後才公布有效 PTE，同步轉譯，返回使用者模式。
6. sepc（例外指令位置）維持原值，CPU 重試同一指令。

原因 12 是取指令缺頁，13 是讀取缺頁，15 是寫入缺頁。非法位址和寫入唯讀頁不會因為「發生缺頁」就獲准配置。

## 互動：挑選 victim 與換出

下方固定資料互動展示 FIFO 候選與 dirty page 換出成功或 I/O 失敗。這是固定資料教學示意，不會呼叫核心或磁碟；QEMU runtime 與瀏覽器操作驗收均為 NOT_RUN。

<script setup>
import PagingLab from '../.vitepress/theme/components/PagingLab.vue'
</script>

<PagingLab />

示意假設 victim 為 dirty、沒有既有 Swap 副本，開始交易時配置新 slot 12；失敗情境模擬第 3 個 sector 出錯，實際故障可能在任一 sector。clean 且可由映像／零頁重建的頁可直接丟棄；已有有效 Swap 副本則可重用，不需另配 slot。

核心的 `vm_handle_fault` 取得 VM Gate 後會跨越可能睡眠的 `swap_write`。原 fault caller 等待磁碟時，另一個 process 若也發生 page fault，會等待同一 Gate；scheduler/UI poll 是 no-wait context，若配置需要置換，會在修改 PTE 或送 I/O 前回 `ERR_BUSY`。互動中的失敗情境假設沒有其他 eligible victim；實際 `evict_one` 在某頁寫失敗並 rollback 後，還會繼續掃描下一個候選，僅當沒有其他候選可用時才回 `ERR_IO`。換出成功或回復完成後，Gate 仍由外層 fault scope 持有，直到外層處理完需求頁或錯誤才 leave。模型只表現這些同步關係，不代表 process 可任意操作 VM metadata。

## 補充分頁表記錄什麼

`kernel/mm/vm.c` 的 VmSpace 保存程式來源、heap 結尾與每頁 VmPage；每頁記錄實體位址、交換槽位、修改狀態、入駐次序與暫時固定次數。

VM 的可重入 scope 序列化頁表與 resident metadata 的建立、複製、fault、置換及查詢；scheduler/UI poll 是 no-wait context，若配置必須置換會在修改 PTE 或送出磁碟 I/O 前回 BUSY。Dirty page 換出前先保存並撤銷舊 PTE、執行 `sfence.vma`，再進行可能睡眠的 Swap I/O；其他行程碰到該頁會 fault 並等候 VM Gate。失敗時恢復原 PTE，保留 dirty RAM 作有效副本。

| physical | slot | 內容在哪裡 |
| --- | --- | --- |
| 有 | 無／有 | 在 RAM；可能還保留交換區副本 |
| 無 | 有 | 由交換區換回 |
| 無 | 無 | 從內嵌映像或零填充重建 |

PTE 是 CPU 查找的結構；補充表是核心用來決定「缺頁時去哪找資料」的 metadata（管理資料）。兩者用途不同。

## 何時置換、如何選頁

預設全系統使用者常駐預算為 64 頁，也就是 256 KiB；這不是整台 QEMU 的 RAM 大小。頁框預算達上限，或底層沒有可用實體頁時，VM 嘗試置換。

FIFO 掃描補充表，選入駐序號最小且未固定的常駐頁。讀寫命中不改入駐次序，所以不是 LRU（最近最少使用）。最多掃描 17 份空間（16 個行程加一份 exec 備用）、每份 262 頁記錄；核心與分頁表不參與置換。

- clean（未修改）且可從映像／零頁重建：直接丟棄。
- clean 且已有 Swap 副本：保留槽位，不必重寫。
- dirty（已修改）：先 pin 並撤銷 PTE、同步轉譯，再完整寫入 8 個磁區；成功才公布 swap metadata 並回收頁框，失敗則恢復映射與 dirty RAM。撤銷到還原／提交之間延後 kill，避免半途丟棄狀態。

修改狀態合併軟體記錄與硬體 PTE_D（已寫入標記）。缺頁處理會暫時 pin（固定、禁止換出）當前指令可能跨越的程式碼頁，避免資源不足時指令頁與資料頁互相淘汰、永遠無法完成重試。

## 磁碟交換

專用 `build/wuwaos-swap.img` 預設 16 MiB，標頭占 4 KiB，其餘為 4095 個 4 KiB 槽位。它與 WUFS 分開；重啟後不恢復舊行程。

`kernel/mm/swap.c` 用 bitmap（位元圖）管理槽位。槽位從 1 編號，0 代表沒有槽位。槽位 n 從磁區 `8*n` 開始，一頁逐次讀寫八個 512-byte 磁區。

原始碼：[kernel/mm/swap.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/swap.c)／[kernel/mm/swap.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/swap.h)，由 GitHub main 管理，本站不內嵌來源檔。

## 欄位、限制與主要函式

課程以 FIFO queue 解釋「最早進入 frame 的頁面先離開」；WUWAOS 沒有每行程的實體 FIFO queue，而是掃描補充記錄中的 `arrival`，取最小且未 pin 的常駐頁。
觀念參考：[`notes/ch10 Virtual Momory.md` · 缺頁處理、dirty bit、§10.4.2 FIFO](https://github.com/johnson931128/WUWAOS/blob/main/notes/ch10%20Virtual%20Momory.md)。

| WUWAOS 欄位 | 用途與本實作語意 |
| --- | --- |
| `VmPage.physical` | 非 0 表示目前有 RAM frame；為 0 時可能在 Swap，也可能由映像／零頁重建。 |
| `VmPage.slot` | 1-based Swap slot；0 表示沒有已指派 slot。新 slot 先配置，I/O 成功後才寫進 page metadata。 |
| `VmPage.arrival` | 頁面進入 RAM 時遞增；FIFO 命中不更新。跨所有 `VmSpace` 比較。 |
| `VmPage.pins` | 非 0 時不可置換。fault handler 也會 pin 可能跨頁的指令 code page。 |
| `VmPage.dirty`、PTE `D` | 軟體修改狀態與硬體 dirty bit 合併判定；dirty 頁不能直接丟棄。 |
| `saved_entry` | 暫存 victim leaf PTE；I/O 失敗時還原並執行 `sfence.vma`。 |
| `resident`、`frame_limit` | 全系統常駐頁數與預算。預設 64 頁，最小預算 8 頁。 |
| Swap bitmap | `swap.c` 的槽位配置圖；slot 由 1 起算，0 不代表磁碟位置。 |

主要函式（精確型別見標頭檔）：

| 函式 | 參數與回傳 |
| --- | --- |
| `int vm_handle_fault(uintptr_t root, uintptr_t address, unsigned long cause, uintptr_t pc)` | 處理 user page fault；cause 12/13/15 是取指令／讀／寫。回 0 表示頁面已備妥，負值為錯誤碼；返回 trap 後 CPU 重試原指令。 |
| `int vm_scope_enter(void)` / `int vm_scope_leave(void)` | 進入／離開 VM Gate；enter 可能等待另一個 VM 操作，no-wait context 不能等待時回錯。 |
| `long vm_sbrk(uintptr_t root, long increment)` | 調整 heap 合法範圍，回傳舊 break 或負錯誤；頁框在首次存取時才配置。 |
| `long vm_control(long limit)` | 查詢或設定全系統 resident budget；0 查詢，非 0 值受最小值與可用資源限制。 |
| `int vm_get_usage(uintptr_t root, VmUsage *usage)` | 寫入該空間的 resident/swapped page 數及 heap bytes；錯誤時回負值。 |
| `unsigned int swap_alloc(void)` / `void swap_free(unsigned int slot)` | 配置 1-based slot；無可用槽回 0。 |
| `int swap_write(unsigned int slot, uintptr_t page)` / `int swap_read(unsigned int slot, uintptr_t page)` | 在已配置 slot 與對齊的 4 KiB 頁間傳輸 8 個 sector；成功回 0，參數錯誤或 I/O 失敗回負值。 |

來源：[vm.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c) · [vm.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.h) · [swap.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/swap.c) · [swap.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/swap.h) · [vm_limits.h](https://github.com/johnson931128/WUWAOS/blob/main/include/vm_limits.h)

換出失敗時，VM 保留原本 RAM 副本；換入失敗時，不公布只讀到一部分的頁面。沒有 Swap 仍可按需配置與丟棄可重建頁，但不能無條件換出修改過的資料。

## 配置與生命週期

`sbrk` 只保留 heap 合法範圍，第一次存取才分配頁框。fork 複製私有內容，包含已換出資料；exec 成功後才回收舊空間；wait 與孤兒回收歸還頁框、槽位和私有分頁表。

目前共用限制由 [include/vm_limits.h（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/include/vm_limits.h) 定義：每頁 4096 bytes、heap 起點 0x1000000、最多 256 個 heap 頁、預設 64 個常駐頁框、最小預算 8 頁。

## 在 QEMU 觀察

逐條輸入；這是讓你觀察記憶體的互動操作，不是自動測試程式：

```text
vmstat limit 8
mem alloc 12
mem read 0
mem write 0 65
mem write 1 66
mem write 2 67
mem write 3 68
mem write 4 69
mem write 5 70
mem write 6 71
mem write 7 72
mem write 8 73
mem write 9 74
mem write 10 75
mem write 11 76
vmstat
mem read 0
mem free
vmstat limit 64
```

第一個 read 應讀到零；填寫頁數超過預算後，vmstat 應顯示換出／換入或 clean drop（直接丟棄乾淨頁）。再次讀第 0 頁，首／中／尾內容應保持 65。實際計數也包含 shell 自己的程式碼、資料與堆疊，不能只以 heap 頁數推算。

## 錯誤與限制

非法缺頁回 -14，沒有資源回 -12，Swap I/O 失敗回 -5；無法補頁就結束該行程。runtime 磁碟等待會讓出 CPU；逾時後先要求 reset，確認安全停止才隔離裝置並回錯，不能確認則停機保留 DMA storage。Swap-out 失敗會恢復 victim 的 RAM 映射，但裝置 offline 後，已經只存在 Swap 的其他頁仍可能無法換入。2026-11-08 的非同步換頁與故障路徑未執行 runtime 驗證（NOT_RUN）；詳見[實作計畫](https://github.com/johnson931128/WUWAOS/blob/main/plan.md)。

沒有 COW、mmap、共享頁與動態核心 heap。縮小 heap 只釋放不再涵蓋的整頁；部分頁縮小再擴張不保證清零。常駐頁保留交換區副本、空 heap L0 表保留至空間銷毀，都是仍有所有權的資源。

觀念參考：[`notes/ch10 Virtual Momory.md` · 缺頁處理步驟與 §10.4.2 FIFO](https://github.com/johnson931128/WUWAOS/blob/main/notes/ch10%20Virtual%20Momory.md)。WUWAOS 目前實作及資源不足、唯讀交換區、非法存取等既有驗收見 [Spec 10](../SPEC-10-demand-paging.md)；未實際耗盡全部 128 MiB，也未注入部分磁區讀取失敗。
