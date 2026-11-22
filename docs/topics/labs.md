# 互動教學

依[現行規格](../spec/index.md)的章節順序選擇教材。這些圖解用可重現的步驟展示機制，實際運行結果可查各章的 QEMU 驗收紀錄。

| 章節 | 可觀察的操作 |
| --- | --- |
| [00 · 專案簡介與系統架構](../spec/00-overview.md) | [系統架構圖](../index.md#atlas-title)可選擇元件，對照各層的責任。 |
| [01 · 如何執行：啟動、建置與驗證](../spec/01-running.md) | [開機時間線](../spec/01-running.md#從韌體交接到-shell)逐步顯示韌體到 Shell 的交接。 |
| [02 · 從使用者程式進入核心](../spec/02-traps-and-syscalls.md) | [陷阱流程](../spec/02-traps-and-syscalls.md#逐步看-cpu-如何返回)比較三種事由及其返回方式。 |
| [03 · 行程與執行緒](../spec/03-processes-and-threads.md) | [執行緒狀態](../spec/03-processes-and-threads.md#觀察一條執行緒的狀態)比較時間片、I/O 等待與 `join`。 |
| [04 · 誰先跑：排程、狀態與多 Hart](../spec/04-scheduling.md) | [輪流排程](../spec/04-scheduling.md#比較不同時間片)比較時間片怎麼改變執行順序。 |
| [07 · 資料怎麼進出：I/O 與磁碟](../spec/07-io.md) | [FD／OFD](./file-descriptors.md)觀察共用讀寫位置，[磁碟非同步 I/O](./block-async.md)追蹤請求送出與完成。 |
| [08 · 頁面、虛擬記憶體與 COW](../spec/08-virtual-memory.md) | [Sv39](./memory.md)走訪頁表，[FIFO 與 Swap](./paging.md)比較置換與回滾，[COW 圖解](../spec/08-virtual-memory.md#寫入時複製)說明首次寫入。 |
| [09 · 資料怎麼存下來：WUFS 與磁碟復原](../spec/09-filesystem.md) | [WUFS Journal](./journal.md)比較交易成功與不同階段的故障。 |
