# Spec 07：FIFO 就緒佇列與非搶占式排程器

## 目標與工作範圍

使用者希望先建立保存待執行 Process（行程）的資料結構，採用 FIFO（先進先出）排程，完成後直接閱讀程式碼。
保留既有 PCB、四個行程欄位、fork/exec、sleep/wakeup、exit/wait 與 console/pipe；不新增測試程式碼。
本次不含計時器搶占、優先權、動態行程表或其他排程政策。

## 參考與設計

- 參考 `notes/ch5 CPU schedueling.md` 的「FCFS 理論觀念與數學剖析」：Ready 行程從隊尾加入，從隊首取出。
- `processes[PROCESS_LIMIT]` 保存所有狀態的 PCB；獨立 `kernel/scheduler.c` 管理只含 Ready 行程的單向鏈結 FIFO。
- PCB 增加 `ready_next` 與 `ready_queued`；佇列保存 PCB 指標而非複製 PCB，不使用 heap（動態配置區）。
- ReadyQueue 保存 head（隊首）、tail（隊尾）、count（長度）。入列／取出為 O(1)，移除任意節點為 O(P)。重複入列不改變順序。
- `transition()` 統一管理入列與離列；建立完成、fork 完成、事件喚醒、I/O 完成、等待子行程結束均排到隊尾。
- Running 行程不在 ready queue 中。原行程仍 Running 時，普通 syscall 後繼續原行程，不因有其他 Ready 行程就搶占。
- Running 行程主動 yield 後變 Ready 並加入隊尾；阻塞與結束則離開可執行集合。保留 yield 是本專案的合作式介面，非計時器時間片。
- kill／reap 移除可能仍在佇列中的 PCB，避免回收後殘留指標；重新使用欄位時清空鏈結欄位。
- 同一批 wakeup／I/O 完成的處理順序維持 PCB 掃描順序，以實際入列順序打破同批事件的先後；不按 PID 大小或 PCB 欄位輪流選取。
- 單核心、核心中斷關閉時修改佇列；未加入多核心鎖。原有 I/O 輪詢與 orphan 回收仍掃描 PCB，僅候選行程的取出是 O(1)。

## 執行步驟

1. 新增 scheduler.c/h、PCB 鏈結欄位與初始化，納入 Makefile。
2. 將狀態變更接到 FIFO，process_resume 改取隊首，保留頁表切換與 TrapFrame 還原流程。
3. ps 顯示就緒佇列順序，pcb 顯示是否入列；更新 README 與 CODE_GUIDE。
4. 建置並用一般 QEMU shell 命令驗證建立、喚醒、yield、結束回收、PCB 欄位重用及固定 pipe。

## 驗收條件

- 編譯警告視為錯誤的建置通過，ELF 無未解析符號。
- Ready PCB 恰有一個佇列節點；Running／Waiting／Zombie／Unused 不留在佇列。
- 依入列先後選擇行程；普通 syscall 不搶占，yield 重新排到隊尾。
- fork、exec、sleep/wakeup、wait 回收與固定 pipe 正常；回收後可重用欄位且無重複排程／殘留節點。
- ps／pcb 可觀察實際佇列資料；文件說明 FIFO 與保留的合作式 yield，提供程式碼閱讀位置。

## 驗收紀錄

2026-10-24，WSL Ubuntu／Clang 18／QEMU 8.2.2，單核心 virt。

- `make` 通過 `-Wall -Wextra -Werror`，kernel.elf／user.elf／app.elf 的 `llvm-nm -u` 沒有未解析符號。
- PID 2／3 由 run counter 建立並 exec，於通道 9 等待；wakeup 後依 2:3、3:3、2:2、3:2、2:1、3:1 輸出，trace 顯示每次 yield 後 Ready，再從隊首選回；wait 均得 0。
- 三個 counter PID 4／5／6 喚醒後，在下一條 shell 命令 kill 5 執行時三者仍 Ready；trace 顯示 PID 5 Ready→Zombie，PID 4／6 繼續執行並返回 0，wait 5 得 -9。確認中間節點移除不破壞其餘順序。
- 重用 PCB 欄位後，兩組 counter 分別 kill 隊首 PID 10 與隊尾 PID 13，均在 Ready 狀態終止，另一個行程正常退出；wait 回收後可再次建立 PID 14／15。
- PID 14／15 喚醒後接著輸入 ps，實際列出兩者 Ready，且顯示 `Ready queue (FIFO), count=2: 14 -> 15`；兩者退出後正常回收。Running shell 的 pcb 顯示 ready_queued=0。
- 固定管線 PID 7／8 經多次 pipe write／read 阻塞與 I/O ready 喚醒，完整收到 80 bytes 與 EOF，雙方返回 0 並回收；fork PID 9 的父子私有資料仍分離，子返回 0 並可 wait。
- 本輪使用一般 shell 與既有內建程式進行互動驗證，沒有新增測試程式碼。尚未以額外核心呼叫注入重複 enqueue 或損毀鏈結，也沒有實測多核心、計時器搶占或無限 CPU 迴圈。
