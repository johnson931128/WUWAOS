# Spec 15：Timer interrupt 與 RR 時間片排程

本階段記錄 RR 政策與當時派發方式；現行核心派發與阻塞恢復機制已由 [Spec 16](./SPEC-16-kernel-context.md) 改成保留核心 C 呼叫堆疊，50 ms RR 政策不變。

## 目標與工作範圍

使用者要求計時器中斷、時間片與 RR（輪詢排程），可依需要重構分類；更新規格與公開文件網站，可加入 Mermaid 圖。完成適當驗證後依 AGENTS.md commit／push 並發布網站。不新增測試程式碼，不更動其他未完成工作。

## 設計

- 單 hart、QEMU virt／RV64／128 MiB。使用 OpenSBI TIME extension（韌體計時服務），不直接讀寫 M-mode 的 CLINT，也不要求 Sstc 硬體擴充。
- `clock.h` 統一 rdtime 與毫秒換算，維持目前平台 10 MHz 假設；splash／block timeout／timer 共用，不再各自重複實作。尚不解析 FDT 的 timebase-frequency。
- `sbi.c/h` 封裝 SBI BASE probe 與 TIME set_timer 的暫存器呼叫約定；`timer.c/h` 負責裝置初始化、設定絕對到期時間、清除待處理事件與計數。韌體不支援或呼叫失敗時輸出診斷並停止。
- `scheduler.c/h` 保留單向鏈結 FIFO Ready queue 作為 RR 輪轉順序，政策為每次出隊分配 50 ms。設定常數 SCHEDULER_QUANTUM_MS 與硬體計時分開。
- 開機先關閉中斷，trap_init 裝共同入口，timer_init 清除舊事件並開啟 sie.STIE（監督模式計時器中斷允許位元）。初始使用者 TrapFrame 設 sstatus.SPIE（陷阱返回後啟用中斷），SIE 仍為 0；核心保持不搶占。
- 派發行程前設定 deadline=now+50ms。一般 syscall、合法缺頁恢復後繼續同一 Running 行程時不更新 deadline，因此頻繁呼叫服務不能不斷重置時間片。yield／阻塞／退出會放棄原時間片，下一次派發取得新時間片。
- 計時器中斷的 scause 為最高位加 5。先保存真實 TrapFrame，經 SBI 清除 pending，再增加行程 preemptions，Running→Ready 排隊尾，由排程器取隊首、切頁表並還原狀態。中斷不修改 sepc，也不覆寫 a0；不是 ecall 的 sepc+4。
- 沒有其他 Ready 時，同一行程可重新獲選並取得下一個時間片；全員等待終端時停用到期事件，繼續既有 UART 輪詢。無事件來源的全員阻塞仍印診斷並 idle。
- 單核心、核心全域中斷關閉，佇列與核心資料不在修改中途被搶占。長 syscall、磁碟同步 I/O 或缺頁換入可能延遲中斷，50 ms 不是嚴格即時保證，也不是精確使用者 CPU burst 統計。
- `ps` 顯示 RR、時間片、計時器中斷數與就緒順序；`pcb` 新增 dispatches／preemptions。fork 子計數從零開始，exec 保留同一 PCB 的累計值；數值用於觀察，不新增 syscall 或示範程式。
- 網站用本機套件 Mermaid 產生 SVG，程式碼仍只提供 GitHub 連結；沒有編輯、上傳或來源切換，沒有外部 CDN／圖表服務。

## 執行步驟與驗收條件

1. 整理計時／SBI／timer 模組，接上中斷分派與 RR 派發；保留原有行程生命週期與 I/O。
2. 建置並在 QEMU 操作 fork、run、wakeup、wait、kill、pipe、fan、檔案與分頁壓力。
3. 利用 QEMU 偵錯介面，將既有 worker 保存位置指向映像中已存在的無 syscall 迴圈，觀察計時器在無 yield 下強制切換、shell 可回應且能 kill／wait。這是偵錯操作，不加入任何故障或測試程式碼。
4. 檢查至少兩個持續可執行行程的輪轉、單一行程續跑、普通 syscall 不重置時間片、阻塞時無計時器風暴，以及沒有修改中斷 sepc／返回值。
5. 更新主題文件、CODE_GUIDE、README 與歷史規格入口，網站 Mermaid 圖在瀏覽器確認後發布 GitHub Pages。

## 觀念與規格來源

- `notes/ch5 CPU schedueling.md` 的 Round-Robin 理論觀念（約 467 行）與非搶占式核心（約 95 行）。教材說明時間片／輪轉，WUWAOS 具體採 50 ms 與核心不搶占。
- [RISC-V SBI TIME extension](https://github.com/riscv-non-isa/riscv-sbi-doc/blob/master/src/ext-time.adoc)：絕對時間、以 UINT64_MAX 清除 pending。
- RISC-V Supervisor 規格：stvec、scause、sie.STIE、sstatus.SIE／SPIE 與 sret。

## 驗收紀錄

2026-10-27，WSL Ubuntu／Clang 18／QEMU 8.2.2／OpenSBI 1.3，單 hart、128 MiB，獨立 build/rr-check.img 與 rr-swap.img，未使用原有使用者磁碟。沒有新增測試程式碼或斷言。

- 建置通過 -Wall/-Wextra/-Werror，kernel.elf 無未解析符號；開機成功查詢 SBI TIME，ps 顯示 RR／quantum_ms=50／中斷數。初始 shell 短工作中斷數為 0，長時間等輸入時不自行累加。
- fork 子 2 的私有 data／stack=101／101，父=1／100，wait=0。counter 3／4 於通道 9 喚醒後各自 3／2／1、wait=0；pipe 子 5／6 完整傳遞 80 bytes／EOF 並回收。檔案 write／cat 讀回 timer_rr。
- 未加入 CPU-bound 測試程式；使用 QEMU GDB remote protocol 將既有 worker 7／8 的保存 sepc 指向 app.elf 原本就存在的 0x400010 自跳迴圈。喚醒後沒有 yield 或 syscall，兩者 preemptions 皆持續增加，shell 仍可 ps／pcb，佇列顯示 7→8；一輪各觀察到 106／109 次到期。kill 7／wait=-9 後，只有 worker 8 持續執行時仍可續跑與被搶占。
- GDB 原先在 U-mode 讀核心虛擬記憶體回 E14，改用官方支援的 Qqemu.PhyMemMode:1。兩者保存 a0 分別維持 0x12345678／0x87654321，sepc 仍是 0x400010，確認中斷沒有覆寫返回值或將 sepc 加 4。
- worker 9 以相同方式改用既有 getpid wrapper：sepc=ra=0x40001a，形成 getpid→ecall→ret 的持續服務迴圈。其 preemptions 達 595、sepc 可在既有 ret（0x400026）恢復，證明普通 syscall 不重新贈送時間片。shell 能 kill／wait 8／9，回到只有 PID 1、Free PCB slots=3。
- 8 頁常駐預算下寫 12 個 heap 頁，頁 0 首／中／尾讀回 65；統計 swap_in=7、swap_out=14、invalid_faults=0、io_errors=0。mem free 後恢復預算；fan 儲存並 cat 讀回 RR preemption works，退出自動 wait=0。
- shell exit 後 monitor 確認 free_count=0x7b04（31492，開機核心映射基準）、resident=0、swap used=0、timer armed=0。monitor quit 結束 QEMU。紀錄在忽略的 build/rr-console.log，偵錯暫存器操作見本次工具紀錄。

- 最終 make clean／make 通過；四個 ELF 均無未解析符號，kernel.elf 為 516536 bytes。npm docs:build 通過，npm audit 為 0 vulnerabilities；Mermaid 動態套件仍有大型 chunk 提醒。瀏覽器確認兩張圖表、章節錨點與深淺色切換正常，重建後重啟 preview 以避免舊資產快取。

限制：未驗證真實硬體、非 10 MHz 時基、多核心、核心搶占、SBI 不支援／失敗的實際注入、計數器溢位或精密牆鐘 jitter；沒有宣稱整個 kernel I/O 期間仍能維持 50 ms 切換，也沒有新增優先權、CPU burst 統計或終端作業控制。
