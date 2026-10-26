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

## 補充分頁表記錄什麼

`kernel/vm.c` 的 VmSpace 保存程式來源、heap 結尾與每頁 VmPage；每頁記錄實體位址、交換槽位、修改狀態、入駐次序與暫時固定次數。

| physical | slot | 內容在哪裡 |
| --- | --- | --- |
| 有 | 無／有 | 在 RAM；可能還保留交換區副本 |
| 無 | 有 | 由交換區換回 |
| 無 | 無 | 從內嵌映像或零填充重建 |

PTE 是 CPU 查找的結構；補充表是核心用來決定「缺頁時去哪找資料」的 metadata（管理資料）。兩者用途不同。

## 何時置換、如何選頁

預設全系統使用者常駐預算為 64 頁，也就是 256 KiB；這不是整台 QEMU 的 RAM 大小。頁框預算達上限，或底層沒有可用實體頁時，VM 嘗試置換。

FIFO 掃描補充表，選入駐序號最小且未固定的常駐頁。讀寫命中不改入駐次序，所以不是 LRU（最近最少使用）。最多掃描 5 份空間、每份 262 頁記錄；核心與分頁表不參與置換。

- clean（未修改）且可從映像／零頁重建：直接丟棄。
- clean 且已有 Swap 副本：保留槽位，不必重寫。
- dirty（已修改）：先完整寫入 8 個磁區，再撤銷 PTE、同步轉譯並回收頁框。

修改狀態合併軟體記錄與硬體 PTE_D（已寫入標記）。缺頁處理會暫時 pin（固定、禁止換出）當前指令可能跨越的程式碼頁，避免資源不足時指令頁與資料頁互相淘汰、永遠無法完成重試。

## 磁碟交換

專用 `build/wuwaos-swap.img` 預設 16 MiB，標頭占 4 KiB，其餘為 4095 個 4 KiB 槽位。它與 WUFS 分開；重啟後不恢復舊行程。

`kernel/swap.c` 用 bitmap（位元圖）管理槽位。槽位從 1 編號，0 代表沒有槽位。槽位 n 從磁區 `8*n` 開始，一頁逐次讀寫八個 512-byte 磁區。

原始碼位置為 `kernel/swap.c`／`kernel/swap.h`；目前仍屬本機未提交的 Spec 10 工作，不會為了發布網站而混入 OS 提交。已公開的核心版本可在 [GitHub kernel 目錄](https://github.com/johnson931128/WUWAOS/tree/main/kernel) 閱讀，待 OS 另行提交後才會出現新檔。

換出失敗時，VM 保留原本 RAM 副本；換入失敗時，不公布只讀到一部分的頁面。沒有 Swap 仍可按需配置與丟棄可重建頁，但不能無條件換出修改過的資料。

## 配置與生命週期

`sbrk` 只保留 heap 合法範圍，第一次存取才分配頁框。fork 複製私有內容，包含已換出資料；exec 成功後才回收舊空間；wait 與孤兒回收歸還頁框、槽位和私有分頁表。

目前共用限制由 `include/vm_limits.h` 定義：每頁 4096 bytes、heap 起點 0x1000000、最多 256 個 heap 頁、預設 64 個常駐頁框、最小預算 8 頁。這份新標頭同樣待 OS 提交；已公開標頭見 [GitHub include 目錄](https://github.com/johnson931128/WUWAOS/tree/main/include)。

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

非法缺頁回 -14，沒有資源回 -12，磁碟錯誤回 -5；無法補頁就結束該行程。磁碟讀寫是同步輪詢，期間不切換其他行程，沒有裝置逾時處理。

沒有 COW、mmap、共享頁與動態核心 heap。縮小 heap 只釋放不再涵蓋的整頁；部分頁縮小再擴張不保證清零。常駐頁保留交換區副本、空 heap L0 表保留至空間銷毀，都是仍有所有權的資源。

觀念參考：`notes/ch10 Virtual Momory.md` 的缺頁處理步驟與 §10.4.2 FIFO。WUWAOS 目前實作及資源不足、唯讀交換區、非法存取等既有驗收見 [Spec 10](../SPEC-10-demand-paging.md)；未實際耗盡全部 128 MiB，也未注入部分磁區讀取失敗。
