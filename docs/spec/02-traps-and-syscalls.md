# 02 · 從使用者程式進入核心

使用者程式要讀檔時，會呼叫 `read()`，把檔案描述符、緩衝區位址和長度交給核心。它在 [U-mode](./glossary.md#privilege-mode) 執行 `ecall` 後，CPU 轉入核心使用的 S-mode。這次受控的交接稱為[系統呼叫](./glossary.md#syscall)。

## 系統呼叫如何往返

1. 使用者函式庫依約定放入參數。前三個參數用 `a0` 至 `a2`，呼叫編號用 `a7`。
2. `ecall` 觸發[陷阱](./glossary.md#trap)。CPU 進入由 `stvec` 指定的共同入口，核心組合語言切換堆疊，將暫存器保存到 [TrapFrame](./glossary.md#trap-frame)。
3. `trap_handler()` 依 `scause` 判斷原因。系統呼叫交給 `syscall_dispatch()`，一般回傳值寫回 `a0`。需讀寫使用者資料的路徑會檢查位址和權限。
4. 若需要等磁碟或 Pipe，呼叫者可停在原本的核心函式內，由[排程器](./04-scheduling.md)換另一條 Thread。原 Thread 回來後接續未完成的呼叫。
5. 正常完成時，核心還原使用者狀態，以 `sret` 返回。

## 進入核心後從哪裡繼續

| 事件 | 核心做什麼 | 返回位置 |
| --- | --- | --- |
| 使用者 `ecall` | 呼叫 `syscall_dispatch()`。 | 一般前進 4 bytes 越過 `ecall`。成功的 `exec()` 改從新程式入口開始，`exit()` 結束行程。 |
| 取指、讀取或寫入缺頁 | `vm_handle_fault()` 用 `stval` 找到相關位址，檢查原始權限並補頁。 | 成功時以原 `sepc` 重試。非法存取或無法補頁時會結束行程。 |
| 計時器或外部中斷 | 時間片到期可換 Thread，外部中斷交給裝置處理。 | 原 Thread 恢復時從保存的 `sepc` 繼續。 |

`scause` 表示為什麼進核心，`sepc` 表示返回或重試的位置，`stval` 在缺頁時提供相關虛擬位址。傳入的 user buffer 只是一個待驗證的位址。對應到頁框後，寫入還可能觸發[寫入時複製](./08-virtual-memory.md#寫入時複製)。

原始碼與既有驗收見 [陷阱入口](https://github.com/johnson931128/WUWAOS/blob/3a3b273/arch/riscv/trap.S) · [Trap handler](https://github.com/johnson931128/WUWAOS/blob/3a3b273/arch/riscv/trap.c) · [Syscall dispatcher](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/syscall/syscall.c) · [陷阱章節](../topics/traps.md)。

接著讀[行程與執行緒](./03-processes-and-threads.md)。
