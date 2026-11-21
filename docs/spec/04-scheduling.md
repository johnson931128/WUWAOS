# 04 · 誰先跑：排程、狀態與多 Hart

如果有三條執行緒都能工作，只有一個 [Hart](./glossary.md#hart) 時，核心要決定現在跑哪一條。WUWAOS 把能執行的 TCB 放進一條全系統共用的[就緒佇列](./glossary.md#ready-queue)，取出隊首來執行。這是以執行緒為單位的[輪流排程](./glossary.md#round-robin)。

## 一條執行緒怎麼換狀態

| 狀態轉換 | 原因和下一步 |
| --- | --- |
| `New → Ready` | 資源和初始執行位置已準備完成，加入就緒佇列。 |
| `Ready → Running` | 某個 Hart 取走隊首，切換頁表、恢復執行進度並設定 50 ms 時間片。 |
| `Running → Ready` | 時間片到期，或執行緒主動 `yield()`，回到隊尾。 |
| `Running → Waiting` | 呼叫者需要等 join、子行程、同步物件、I/O 或指定期限，停止占用 CPU。 |
| `Waiting → Ready` | 條件滿足、等到期限或等待被取消，加入隊尾。 |
| `Running → Zombie` | 執行緒已退出，等待 join 或隨行程清理。 |

切換時保存的是[核心上下文](./glossary.md#kernel-context)。例如磁碟讀取停在核心函式中，醒來後仍從原本的核心堆疊和呼叫位置繼續。等到核心要返回使用者模式時，才使用 TrapFrame 還原使用者狀態。

## 多個 Hart 怎麼分工

每個 Hart 有自己的執行中 TCB 和排程器上下文。兩個 Hart 可以同時各跑一條使用者執行緒，即使它們屬於同一個行程。進入核心後，共享核心鎖會序列化受保護的核心工作。使用者態可因計時器中斷而切換，核心態在允許的搶占檢查點才接受這種切換。等待 I/O 的呼叫者也可以在等待點讓出 CPU。

`ps` 的 `Ready TID queue` 顯示實際等候派發的 TID。`pcb <pid>` 可查看代表執行緒最近的 Hart、派發次數、搶占次數與等待原因。要觀察一個完整週期，可在 Shell 用 `spawn worker` 開始，等 `ps` 顯示 `Waiting` 後輸入 `wakeup 7`，最後用 `wait <pid>` 收取結果。

現行 CLI 有 2／3 Hart QEMU 回歸。長時間 SMP 壓測留待驗證。 [就緒佇列](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/scheduler.c) · [排程與切換](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L1331) · [計時器排程規格](../SPEC-15-timer-rr.md) · [既有驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)

接著讀[為什麼要同步](./05-synchronization-problems.md)。
