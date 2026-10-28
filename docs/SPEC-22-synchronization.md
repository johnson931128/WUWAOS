# Spec 22：自製 Mutex 與 Semaphore

## 目標與範圍

不依賴 POSIX 程式庫，提供 WUWAOS 自己的 Mutex（互斥鎖）與 Semaphore（計數號誌），可供多個行程持共同的核心物件編號協作。新增使用者 API、系統呼叫、shell 命令及文件；不引入共享記憶體或多核心。同步物件是記憶體中的核心資源，重啟後不保留。

## 設計

單核心且核心內不搶占：核心中檢查物件狀態、登記 FIFO 等待佇列、設定等待狀態及切換行程的過程不被其他行程插入，避免錯失喚醒。這是目前架構的前提，不能直接推廣到多核心或核心可搶占的情況。

全域最多 8 個物件，以遞增 handle（物件編號）識別；編號過期不能指向新物件。所有知道編號的行程都可操作，沒有使用權限隔離，fork／exec 不複製物件。使用者 API 在 `user/api.h`，封裝 WUWAOS 專用 syscall，而非 POSIX API。物件不因創建者退出自動銷毀，需明確 destroy，否則保留直到重啟。

Mutex 的 owner（持有者）一次最多一個；未持有可取得，其他行程進入等待佇列，持有者解鎖後喚醒一個。不可重入；非持有者不能解鎖。持有者終止、被 kill 或成功 exec 時，標記 mutex 為 poisoned（受保護資料可能不一致），喚醒等待者，後續 lock 回 -130；不能把它當成資料已恢復，需自行處理後銷毀並重新建立。

Semaphore 儲存非負 permit count（可用名額）：wait 在數量大於 0 時減一，否則等待；post 加一，並喚醒佇列中一人。數量達 LONG_MAX 時 post 回 -75。此 semaphore 不追蹤誰使用名額；持有名額的行程結束時不自動歸還，應由應用協定管理。

排隊採 FIFO 唤醒，醒來後重檢條件；其他 Ready 行程可能先取得，因此不保證嚴格 FIFO 取得或無飢餓。被 kill 的等待行程從佇列移除，已喚醒但未重試者被 kill 時，若資源仍可用會喚醒下一人；避免遺失喚醒。物件有持有者、等待者或已喚醒尚未返回的等待呼叫時 destroy 回 -16。handle 沒有類型權限或安全隔離。

## 操作

shell 提供 `sync mutex create`、`sync mutex lock <id>`、`sync mutex unlock <id>`、`sync sem create <n>`、`sync sem wait <id>`、`sync sem post <id>`、`sync destroy <id>`。建立成功回傳正數編號，其餘成功回 0。負數表示錯誤；-35 是重複取得、-1 是非持有者解鎖、-16 是物件忙碌、-22 是錯誤編號或操作、-130 是持有者離開造成狀態不確定。

若從 shell 對沒有可用資源的物件執行阻塞操作，而沒有其他行程能釋放／post，shell 會等待，無前景 Ctrl+C 取消；操作前應確認有其他行程能喚醒。shell 的 `syncbg sem wait|post <id>` 透過 fork 在子行程操作號誌；`syncbg mutex hold <id>` 在子行程取得鎖，等待通道 7，收到 `wakeup 7` 後釋放。以 `ps` 觀察、`wait <pid>` 回收；`wakeup 7` 也會喚醒其他等待此通道的行程。無一般外部程式載入器。

## 驗收紀錄與未驗證

WSL Clang 建置成功。QEMU 獨立磁碟單行程互動確認建立、取得、重複上鎖拒絕、持鎖時不可銷毀、解鎖、銷毀與過期編號拒絕；Semaphore 初始值 1 的 wait、post、destroy 成功。以 shell 的 syncbg 實際 fork 多個行程：確認 semaphore 等待與 post 喚醒、兩位等待者依佇列順序喚醒、等待中 kill 後下一等待者仍能進行；mutex 競爭、非持有者解鎖拒絕、持有者正常釋放及 kill 持有者後等待者收到 -130。忙碌物件拒絕銷毀。尚未驗證已喚醒但尚未執行時被 kill、持鎖成功 exec、所有資源耗盡與多核心；不保證無飢餓。

參考 `notes/ch6 Synchronization Tools.md` 的互斥、計數號誌和等待佇列；實際語意以本文件與 [核心實作](https://github.com/johnson931128/WUWAOS/blob/main/kernel/sync.c) 為準。
