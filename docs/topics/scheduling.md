# 05 · FIFO 排程

有了 Ready（可以執行）的行程，還要有人決定下一個用 CPU 的對象。WUWAOS 將這個選擇獨立放在 `kernel/scheduler.c`。

## 就緒佇列

FIFO（先進先出）把新就緒的 PCB 放到隊尾，從隊首取出下一個。佇列以 PCB 裡的 `ready_next` 串接，不複製 PCB。

原始碼：[kernel/scheduler.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/scheduler.c)。

有 head（隊首）和 tail（隊尾）指標，因此入列與取隊首是 O(1)。移除特定行程要從隊首尋找前一節點，最壞 O(N)。`ready_queued` 避免重複入列。

## 什麼時候會換人

`process.c` 的 `transition` 維護狀態與佇列。進入 Ready 排到隊尾，離開 Ready 就移除；Running、Waiting、Zombie 不應留在佇列中。

`process_resume` 先檢查原行程是否仍是 Running。如果是，就繼續原行程；否則才從 FIFO 取下一個。普通 syscall 不會自動讓出 CPU，只有 yield、阻塞、退出等情況觸發選擇。

因此這是 cooperative scheduling（合作式排程）。不是 RR（輪詢時間片排程）：目前沒有計時器在時間片結束時強制切換。

## 切換時 CPU 改變什麼

核心選中 PCB 後切換 satp（分頁設定暫存器）指向它的頁表，並把它的 TrapFrame 交給 `trap.S`。暫存器與 sepc 恢復後，CPU 從該行程保存的位置繼續執行。

這裡不保存可長期暫停的 C 核心呼叫堆疊；待完成操作改記錄在 PCB。I/O 補完後設定返回結果，再讓行程回到 Ready。

## 在 QEMU 觀察

```text
run counter
run counter
ps
wakeup 9
```

兩個 counter 等待通道 9。喚醒後各自從 3 倒數，每印一次就 yield，所以可看到交替；依輸出的 PID 分別 `wait` 回收。`ps` 額外列出 Ready queue，但行程可能在下一次輸入前已完成。

## 與記憶體 FIFO 的區別

這一章 FIFO 決定誰取得 CPU；[頁面置換 FIFO](./paging.md) 決定誰讓出實體頁框。兩者管理不同資源，資料結構也不相同。

觀念參考：`notes/ch5 CPU schedueling.md` 的 FCFS 與非搶占排程。WUWAOS 保留主動 yield，實作與既有驗收見 [Spec 07](../SPEC-07-fifo-scheduler.md)。
