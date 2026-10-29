# 原始規格與驗收紀錄

此處按開發順序保存規格。它們記錄「當時做了什麼」，不一定代表現在的架構；第一次閱讀建議先看左側主題章節。

| 階段 | 規格與定位 |
| --- | --- |
| 01 | [使用者模式與系統呼叫](./SPEC-01-user-syscalls.md) |
| 02 | [合作式行程生命週期](./SPEC-02-process-lifecycle.md) |
| 03 | [Shell、PCB 與 Pipe](./SPEC-03-shell-pipe.md) |
| 04 | [開機 ASCII 動畫](./SPEC-04-boot-splash.md) |
| 05 | [Fork → Exec → 排程 → 回收](./SPEC.md)，靜態位址空間設計已更新 |
| 06 | [VirtIO 與 WUFS](./SPEC-06-filesystem.md) |
| 07 | [FIFO 排程器](./SPEC-07-fifo-scheduler.md) |
| 08 | [動態實體頁與三級頁表](./SPEC-08-memory.md)，預先載入使用者頁的部分由 10 取代 |
| 09 | [fan 全螢幕編輯器](./SPEC-09-fan.md) |
| 10 | [Demand paging、頁面置換與 Swap](./SPEC-10-demand-paging.md)，現行 VM 設計 |
| 11 | [文件網站](./SPEC-11-documentation.md) |
| 12 | [磁碟請求逾時與停止核心](./SPEC-12-block-timeout.md) |
| 13 | [螢光色塊開機字樣](./SPEC-13-block-splash.md) |
| 14 | [逐行命令說明](./SPEC-14-shell-help.md) |
| 15 | [計時器中斷與 RR 排程](./SPEC-15-timer-rr.md)，現行 CPU 排程政策 |
| 16 | [RV64 核心上下文切換](./SPEC-16-kernel-context.md)，獨立排程器堆疊、阻塞後恢復 C 呼叫 |
| 17 | [workstate 即時監測](./SPEC-17-workstate.md)，CPU／RAM 快照、毫秒休眠與全螢幕操作 |
| 18 | [檔案命令與 cls](./SPEC-18-file-commands.md)，刪除、複製、畫面與捲動歷史 |
| 19 | [工作目錄與 shell 命令處理](./SPEC-19-cwd-shell.md)，每行程目錄、相對路徑 |
| 20 | [資料夾刪除與確認](./SPEC-20-directory-removal.md)，子目錄與檔案回收 |
| 21 | [系統內編譯 C 程式的長期目標](./SPEC-21-native-compilation.md)，目前為規劃、尚未實作 |
| 22 | [自製 Mutex 與 Semaphore](./SPEC-22-synchronization.md)，單核心同步與驗證範圍 |
| 23 | [同步工具互動示範](./SPEC-23-sync-demo.md)，實際行程與等待狀態 |

## 如何判斷是否驗證過

請讀各規格的「驗收紀錄」及「限制與未驗證」。設計方案、原始碼靜態檢查、實際 QEMU 操作是不同種類的證據；沒有記錄的邊界不能當成已通過。

完整檔案職責與演算法見 [程式碼與架構紀錄](./CODE_GUIDE.md)。
