# Thread 第一階段：PCB／TCB 與排程單位拆分

2026-11-10，依 [Spec 34](./SPEC-34-threads.md) 完成階段 A：核心已拆出 Thread 執行狀態，排程與等待使用 TCB。**每個 Process 仍只有一條初始 Thread；使用者 thread_create／join／exit／gettid 尚未提供。** 單 hart 與核心內不強制搶占的政策保留。

## 現在的結構

```text
Process table (16 slots)              Thread table (32 slots)
+---------------------------+        +--------------------------+
| PID / parent / lifecycle  |        | TID / execution state    |
| page table / program      |<-------| process pointer          |
| FD table / cwd / session  |        | frame / kernel context   |
| initial thread pointer ---|------->| guarded kernel stack     |
+---------------------------+        | event / sync / cancel    |
                                     +--------------------------+
                                                 |
                                                 v
                                    Ready / sync queues use TCBs
```

`kernel/proc/thread.h` 定義 TCB，`thread.c` 管理獨立 table、單調遞增 TID、目前 Thread 指標，以及 32 組各有 guard 的靜態核心堆疊。階段 A 的 allocator 拒絕替已有初始 TCB 的 PCB 再配置 Thread。TCB slot 可重用，TID 不重用；PCB slot 與 TCB slot 不要求索引相同。

PCB 的狀態為 Unused／New／Live／Dying／Zombie，描述資源生命週期。Ready／Running／Waiting 改屬 TCB。`process_current()` 透過 `thread_current()->process` 找到 PCB；boot／scheduler 沒有目前 Thread，不能沿用剛才執行者的身分。

## 變更的執行路徑

- Ready queue、RR 派發、核心 context switch、user timer 搶占與 TrapFrame 都使用 Thread。核心 stack 在 TCB 回收後才可再次配置，不能回收目前正在執行的 TCB。
- Channel／child／event 等待、timeout、wake、取消與 cancel-defer 記錄移至 TCB；event 的 generation／sequence 規則保留。`wait(pid)` 的等待者是 Thread，回收目標仍是子 Process。
- KernelGate owner 改為 Thread 指標＋TID；同 PID 不同 TID 不被當作遞迴持有者。User mutex owner、semaphore／mutex FIFO 等待者與 owner-died 清理也改為 Thread。
- TTY raw owner 改用 TID，暫時的 console route 存 TCB；FD、cwd、page table 與 TTY session 仍由 Process 持有。
- `ps` 保留每 Process 一列，STATE／WAIT 來自其初始 TCB，另列 TID、Free TCB slots 與 Ready TID queue。`pcb` 額外顯示 Process lifecycle 與 TID。workstate 由 TCB 計 CPU，記憶體仍每 Process 計一次，使用者 snapshot ABI 不變。
- 階段 A 的 runtime／等待與 scheduler glue 仍在 `process.c`，由 `thread_*` API 與型別區分責任；Process 建立／fork／exec／退出／回收仍集中管理共用資源。後續多 Thread 生命週期要依 Spec 34 再擴充。

每 Process 一條 Thread 的限制，使目前 FD table 操作、fork／exec 與 Process exit 能保留既有語意。這不代表多 Thread 所需的 FD pin、join claim、額外 user stacks 與群組退出已實作。`kernel/proc/kmutex.*` 獨立原型及其主機測試也已改用 Thread 型別，仍未接入 FS／VM runtime。

## 重跑驗證

PowerShell 在有 portable 工具的目前原始碼目錄執行：

```powershell
& .\build\portable\msys64\ucrt64\bin\python.exe -B .\tools\test_thread_stage_a.py --gui
```

Linux 可用 `python3 tools/test_thread_stage_a.py --gui`，需已有建置工具、QEMU 與 OpenSBI。`--gui` 使用無顯示視窗的 VirtIO GPU／tablet／keyboard 與 QMP 輸入；省略時只測 UART 與 flash 磁碟開機。Serial TCP 與 QMP 僅監聽 loopback。

工具會建置核心、flash loader、主機測試與專用 ELF probe，並建立全新 `build/thread-stage-a-*` 磁碟／swap。Probe 只安裝到該測試磁碟，不改正常 `build/wuwaos.img` 或 CLI 磁碟；測試結束保留 image 與 console logs。

也可在 `.\Start-WUWAOS.cmd shell` 的 Bash 單獨執行 `make test-thread-core`、`make test-kmutex`。前者測實際 TCB allocator、Ready queue、KernelGate、sync 與 event source，僅以 stand-in 取代 CPU／裝置／實際切換。

## 本次驗證結果

環境：portable Clang／LLD 23.1.4、QEMU 11.1.3、OpenSBI 1.8.1；RV64 核心以 `-Wall -Wextra -Werror` 建置。

| 範圍 | 結果與證據 |
| --- | --- |
| 核心、flash loader、ELF probe 與原有 mutex 主機測試 | 通過，見 `build/thread-stage-a-build.log` 與 regression log |
| TCB 主機測試 | 通過：Ready queue FIFO／重複入列／移除、guard、slot 重用／新 TID、同 PCB 不同 TID 的 gate／mutex 身分、sync 取消與 owner death |
| 舊啟動與磁碟保護 | `build/merge-smoke-20261110-151633/`：直接載入、交換磁碟順序、flash 磁碟開機、檔案讀寫、pipe、demo sync、heap 讀寫；有效／空白／損壞／尾端非零映像的初始化與雜湊檢查通過 |
| 階段 A QEMU 回歸 | `build/thread-stage-a-2vfhcik6/` 與 `build/thread-stage-a-regression.log`：probe 20 次 fork／私有資料與 stack／wait、exec quick、spawn worker、channel wake、阻塞 pipe／EOF／取消、sync waiter 取消與 owner-died 全部通過 |
| 計時器與 VM | 兩條不同 Process 的 CPU-bound 執行路徑不 yield，`pcb` 觀察非零 preemptions；workstate snapshot、8-frame 預算下 16 heap pages 寫入／讀回、user fault 與清理通過 |
| 容量回收 | 16 PCB 用滿後回 `ERR_NO_SLOT`；子行程回收後剩 shell＋probe，再退出 probe 後 Free PCB=15、Free TCB=31 |
| GUI 基本路徑 | Headless QMP 點 Terminal 建立第二個 PCB／TCB，VirtIO keyboard Ctrl+D 結束 shell，orphan cleanup 後回到 Free TCB=31 |

GUI 這裡是裝置／TTY 生命週期 smoke test，不包含視覺排版、拖曳、縮放或多視窗壓力驗收。未加入新的硬體故障注入、完整 journal crash campaign、emergency trap stack 或效能比較。原型 mutex 主機測試不能替代 runtime KernelGate／RV64 的全部驗證；多 Thread 同 Process 的生產路徑仍待階段 B。
