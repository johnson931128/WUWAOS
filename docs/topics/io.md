# 08 · 輸入輸出與 Pipe

檔案系統處理名稱、目錄和資料放置；I/O system（輸入輸出系統）處理如何跟裝置交換資料。WUWAOS 先提供 UART 終端、行程間管道與 VirtIO 區塊磁碟這幾條路徑。

## UART 終端

`kernel/console.c` 以 volatile 指標讀寫 `0x10000000` 的 UART（序列埠）裝置暫存器。輸出前輪詢是否可寫；讀取時檢查是否收到資料。volatile 保留硬體存取，不等於鎖或完整記憶體同步。

一般 shell 使用行輸入：收集字元、處理 Backspace，等 Enter 才完成 read。fan 使用 raw console（逐 byte、不回顯輸入），因此按方向鍵不必先按 Enter。

raw console 同時只能有一個持有者；行程退出、被終止或成功 exec 時釋放。它是自訂介面，不是完整 POSIX termios。

## 等待時資料放在哪裡

`kernel/io.c` 不能在核心函式裡一直等到某個行程有資料，否則其他 Ready 行程沒有機會前進。尚未完成的操作會記在 PCB 的 `io_operation`、`io_buffer`、`io_length`、`io_count`，行程改成 Waiting。

`process_resume` 輪詢可完成的 I/O，完成後填入返回值並喚醒行程。使用者資料不一定常駐 RAM，搬移時由安全複製介面逐頁載入，錯誤明確回報。

這裡的「阻塞行程」不等於「裝置中斷」：目前 UART 仍以輪詢觀察，不是中斷驅動。

## 真實的位元組管道

固定 `writer | reader` 使用 16-byte ring buffer（環形緩衝區）。`read_at`、`write_at` 是讀寫索引，`count` 記錄目前資料量；索引前進至尾端後繞回。

writer 要傳 80 bytes，所以無法一次塞完：管道滿時寫入者等待；reader 讀走資料後喚醒寫入者。空管道且仍有寫入者時，reader 等待；所有寫端關閉且資料讀完才得到 EOF（資料結束）。沒有讀取者時，寫入回報錯誤。

每 byte 入／出緩衝區 O(1)，搬移 N bytes 為 O(N)，另外可能包含多次阻塞與恢復。端點參照計數在 fork 繼承、exit／kill 關閉時更新。

## 區塊裝置的另一條路

```text
檔案操作 → fs.c → block.c → VirtIO queue → QEMU 磁碟
換頁操作 → swap.c → block.c → VirtIO queue → 獨立 Swap 磁碟
```

`kernel/block.c` 從 FDT 探索現代 VirtIO-MMIO 裝置，每顆磁碟有自己的請求與佇列狀態。以磁碟標頭辨識 WUFS 與 Swap，不依賴裝置順序。目前磁碟請求同步輪詢完成；尚無磁碟排程、非同步請求或快取。

## 在 QEMU 觀察

```text
writer | reader
```

應讀到完整 80 bytes，最後 EOF 並回收兩個子行程。這是固定的內建管線，不代表 shell 已支援任意多段管線或一般檔案重導向。

觀念參考：`notes/ch12  I_O Systems.md` 的裝置分層、輪詢與阻塞。管道既有驗收見 [Spec 03](../SPEC-03-shell-pipe.md)，磁碟格式識別更新見 [Spec 10](../SPEC-10-demand-paging.md)。
