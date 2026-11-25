# 04 · 誰先跑：排程、狀態與多 Hart

<script setup>
import SchedulingDemo from '../.vitepress/theme/components/SchedulingDemo.vue'
</script>

如果有三條執行緒都能工作，只有一個 [Hart](./glossary.md#hart) 時，核心要決定現在跑哪一條。WUWAOS 把能執行的 TCB 放進一條全系統共用的[就緒佇列](./glossary.md#ready-queue)，取出隊首來執行。這是以執行緒為單位的[輪流排程](./glossary.md#round-robin)。

## CPU 忙在哪裡

程式通常交替運算與等待。從上次醒來到下次阻塞之間，執行緒需要一段 CPU 時間，稱為 CPU burst。等待磁碟或別的事件時，核心可以讓其他就緒執行緒先跑。長時間運算的執行緒也可能在時間片到期時被換下。排程器負責挑下一條，派發則負責把 Hart 交給選中的執行緒，包含恢復執行進度和設定新的計時器期限。

衡量演算法時，**回應時間**關心多久開始回應，在教材的 CPU 排程題通常用第一次取得 CPU 的時間近似。**等待時間**合計在就緒佇列的時間。**周轉時間**則是從到達到完成的總時間。會阻塞的程式也把 I/O 等待計入周轉時間。

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

### 等 50 ms 到期，看 A 和 B 怎麼交接

固定範例中，A 已在 Hart 0 上跑，B、C 留在就緒佇列。50 ms 到期後，動畫先顯示 A 回到隊尾，排列變成 `B → C → A`。同一個模擬時間的下一格，B 從隊首取出，佇列剩 `C → A`。把兩個動作畫開，就能看清楚 Ready 和 Running 分別在哪一步改變。

核心的計時器中斷呼叫 `thread_preempt()`，讓目前執行緒轉回 Ready。排程器的 `scheduler_next()` 再從隊首取出下一條執行緒。若系統只剩 A 可執行，A 回隊尾後會再次被選中並取得新的時間片。 [計時器中斷](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/trap.c#L67) · [就緒佇列](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/scheduler.c#L22)

<SchedulingDemo mode="round-robin" />

### 算一次 4 ms 的輪流排程

三個工作同時到達，CPU 需求依序是 24、3、3 ms。把時間片設為 4 ms，並忽略切換成本。A 跑到第 4 ms 時排到隊尾。B 於第 7 ms 完成，C 於第 10 ms 完成。A 再次取得 CPU 後繼續輪轉，直到第 30 ms 結束。

| 工作 | CPU 需求 | 第一次取得 CPU | 完成時間 | 就緒佇列等待 |
| --- | --- | --- | --- | --- |
| A | 24 ms | 0 ms | 30 ms | 6 ms |
| B | 3 ms | 4 ms | 7 ms | 4 ms |
| C | 3 ms | 7 ms | 10 ms | 7 ms |

A 的 6 ms 是第 4 到第 10 ms 排在 B 和 C 後面。這題沒有 I/O 阻塞，等待時間也可用完成時間減去到達時間和 CPU 需求。平均等待時間為 17 ÷ 3，約 **5.67 ms**。時間片較短通常讓其他就緒工作更早輪到，也會增加派發與切換的次數。WUWAOS 的 50 ms 是本專案固定設定，4 ms 用來完成課本例題。

## 多個 Hart 怎麼分工

每個 Hart 有自己的執行中 TCB 和排程器上下文。兩個 Hart 可以同時各跑一條使用者執行緒，即使它們屬於同一個行程。進入核心後，共享核心鎖會序列化受保護的核心工作。使用者態可因計時器中斷而切換，核心態在允許的搶占檢查點才接受這種切換。等待 I/O 的呼叫者也可以在等待點讓出 CPU。

恐龍書介紹了全系統共用佇列與各處理器各有佇列兩種安排。共用佇列讓空出的 Hart 直接從同一處找工作，取隊首必須受鎖保護。WUWAOS 採用共用佇列。動畫裡 Hart 1 完成 B 後取走 C，Hart 0 上的 A 可以繼續跑。程式回到核心時，共享核心鎖負責序列化受保護的狀態更新。 [共享核心鎖](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/hart.c#L17)

<SchedulingDemo mode="multi-hart" />

`ps` 的 `Ready TID queue` 顯示實際等候派發的 TID。`pcb <pid>` 可查看代表執行緒最近的 Hart、派發次數、搶占次數與等待原因。要觀察一個完整週期，可在 Shell 用 `spawn worker` 開始，等 `ps` 顯示 `Waiting` 後輸入 `wakeup 7`，最後用 `wait <pid>` 收取結果。

現行 CLI 有 2／3 Hart QEMU 回歸。長時間 SMP 壓測留待驗證。 [就緒佇列](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/scheduler.c) · [排程與切換](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L1331) · [計時器排程規格](../SPEC-15-timer-rr.md) · [既有驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)

實機排程資訊可用 [workstate](../topics/monitoring.md) 查看。可調整教學時間片的模型見[計時器與輪流排程補充](../topics/scheduling.md)。

## 恐龍書與課堂筆記

恐龍書第 10 版 §5.1–5.5。延伸筆記收錄 FCFS、SJF／SRTF、優先權、MLQ、MLFQ、執行緒排程和多處理器等演算法與例題。

- [CPU 排程、計算題與演算法程式範例](../theory/05-cpu-scheduling.md)
