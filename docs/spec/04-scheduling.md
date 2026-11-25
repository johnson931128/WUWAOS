# 04 · 誰先跑：排程、狀態與多 Hart

<script setup>
import SchedulingLab from '../.vitepress/theme/components/SchedulingLab.vue'
</script>

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

如果系統裡有 3 條執行緒同時準備好要算資料，卻只有 1 顆 CPU（[Hart](./glossary.md#hart)），該讓誰先跑？如果先跑的那條進入無窮迴圈，其他人豈不是永遠卡死（Starvation）？如果為了公平把切換頻率調得太快，CPU 又會把時間全花在 Context Switch 上——這就是 CPU 排程器的核心權衡。
</div>

## 恐龍書核心理論：搶占式排程與 Round-Robin

恐龍書第 5 章比較了多種排程演算法（FCFS、SJF、Priority、Round-Robin、Multilevel Queue）。對互動式分時系統（Time-Sharing System）而言，最經典的解法是 **Round-Robin（輪流排程，RR）**：

- 所有可執行的執行緒排成一條 FIFO [就緒佇列](./glossary.md#ready-queue)。
- 排程器從隊首取出一位執行緒，並設定一個硬體計時器中斷作為**時間片（Time Quantum, $q$）**。
- 若執行緒在 $q$ 毫秒內主動阻塞（例如等待磁碟）或結束，CPU 立刻讓給下一位；若時間片到期還沒跑完，硬體計時器強制**搶占（Preempt）**，把它排回隊尾。

<div class="tb-box theory">
<span class="tb-label">課本關鍵權衡：時間片 $q$ 該設多大？</span>

- **$q$ 太大**：RR 會退化成先進先出（FCFS），互動式程式（如終端機打字）反應變遲鈍。
- **$q$ 太小**：Context Switch 次數暴增，CPU 有效產出被切換開銷吃掉。
- **經驗法則**：讓時間片遠大於單次 Context Switch 的微秒級耗時，同時小於人類可感知的互動延遲（WUWAOS 採用 **50 ms**）。
</div>

## 一條執行緒怎麼換狀態

| 狀態轉換 | 觸發原因與核心動作 |
| --- | --- |
| `New → Ready` | TCB 與堆疊準備完成，加入全系統就緒佇列隊尾。 |
| `Ready → Running` | 某個 Hart 取出隊首，切換 Sv39 頁表、恢復上下文並設定 50 ms 計時器。 |
| `Running → Ready` | 時間片到期（計時器中斷）或主動呼叫 `yield()`，回到隊尾。 |
| `Running → Waiting` | 等待 `join`、子行程、同步鎖、I/O 事件或 `sleep`，移出就緒佇列不占 CPU。 |
| `Waiting → Ready` | 事件完成、逾時或取消，重新排入隊尾。 |
| `Running → Zombie` | 執行緒已退出，暫留退出碼等待 `join` 或隨行程回收。 |

### 程式碼實例：O(1) 就緒佇列（`kernel/proc/scheduler.c`）

WUWAOS 的排程佇列不依賴動態記憶體配置，直接透過 TCB 內的 `ready_next` 指標串成單向鏈結串列，確保入隊（`scheduler_enqueue`）與取隊首（`scheduler_next`）都是常數時間 `O(1)`：

```c
void scheduler_enqueue(Thread *thread) {
    if (thread->ready_queued) return;
    thread->ready_next = 0;
    if (ready_queue.tail != 0) ready_queue.tail->ready_next = thread;
    else ready_queue.head = thread;
    ready_queue.tail = thread;
    thread->ready_queued = 1;
    ++ready_queue.count;
}

Thread *scheduler_next(void) {
    Thread *next = ready_queue.head;
    if (next == 0) return 0;
    ready_queue.head = next->ready_next;
    if (ready_queue.head == 0) ready_queue.tail = 0;
    next->ready_next = 0;
    next->ready_queued = 0;
    --ready_queue.count;
    return next;
}
```

切換時保存的是[核心上下文](./glossary.md#kernel-context)（`arch/riscv/swtch.S` 保存 callee-saved 暫存器與 `ra`、`sp`）。因此即使磁碟讀取停在核心函式深處，醒來後仍從原本的核心堆疊繼續執行；直到離開核心回到 U-mode 時，才用 `TrapFrame` 還原使用者現場。

## 多個 Hart 怎麼分工

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 多核心（SMP）實作對照</span>

- **使用者態真並行**：每個 Hart 擁有獨立的 `current` 執行緒與排程器上下文；兩個 Hart 可以同時平行執行兩條使用者執行緒（即使屬於同一個行程）。
- **核心態大鎖保護**：進入核心後，由共享核心鎖序列化受保護的核心資料結構，簡化早期多核心同步複雜度（相較 Linux 細粒度鎖，更易於教學追蹤）。
- **實機觀察**：在 Shell 執行 `ps` 可看 `Ready TID queue`，`pcb <pid>` 可看各行程最近的 Hart、派發與搶占次數；也可以用 `spawn worker` → `ps` → `wakeup 7` → `wait <pid>` 親手走完一次阻塞與喚醒週期。
</div>

## 比較不同時間片

在下方單 Hart 互動模型中切換 `25 ms / 50 ms / 100 ms`，對照 Gantt 圖與 Ready Queue 如何隨時間片變化（WUWAOS 核心實際固定為 50 ms）。實機即時監測見 [workstate](../topics/monitoring.md)，延伸說明見 [計時器與輪流排程補充](../topics/scheduling.md)。

<SchedulingLab />

原始碼與驗收：[就緒佇列 (`kernel/proc/scheduler.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/scheduler.c) · [排程與切換 (`kernel/proc/process.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L1331) · [計時器排程規格](../SPEC-15-timer-rr.md) · [既有驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)
