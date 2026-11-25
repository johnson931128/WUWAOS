# 04 · 誰先跑：排程、狀態與多 Hart

<script setup>
import SchedulingDemo from '../.vitepress/theme/components/SchedulingDemo.vue'
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

## CPU 忙在哪裡

程式通常交替運算與等待。從上次醒來到下次阻塞之間，執行緒需要一段 CPU 時間，稱為 CPU burst。等待磁碟或別的事件時，核心可以讓其他就緒執行緒先跑。長時間運算的執行緒也可能在時間片到期時被換下。排程器負責挑下一條，派發則負責把 Hart 交給選中的執行緒，包含恢復執行進度和設定新的計時器期限。

衡量演算法時，**回應時間**關心多久開始回應，在教材的 CPU 排程題通常用第一次取得 CPU 的時間近似。**等待時間**合計在就緒佇列的時間。**周轉時間**則是從到達到完成的總時間。會阻塞的程式也把 I/O 等待計入周轉時間。

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

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 多核心（SMP）實作對照</span>

- **使用者態真並行**：每個 Hart 擁有獨立的 `current` 執行緒與排程器上下文；兩個 Hart 可以同時平行執行兩條使用者執行緒（即使屬於同一個行程）。
- **核心態大鎖保護**：進入核心後，由共享核心鎖序列化受保護的核心資料結構，簡化早期多核心同步複雜度（相較 Linux 細粒度鎖，更易於教學追蹤）。
- **實機觀察**：在 Shell 執行 `ps` 可看 `Ready TID queue`，`pcb <pid>` 可看各行程最近的 Hart、派發與搶占次數；也可以用 `spawn worker` → `ps` → `wakeup 7` → `wait <pid>` 親手走完一次阻塞與喚醒週期。
</div>

實機排程資訊可用 [workstate](../topics/monitoring.md) 查看。可調整教學時間片的模型見[計時器與輪流排程補充](../topics/scheduling.md)。

在下方單 Hart 互動模型中切換 `25 ms / 50 ms / 100 ms`，對照 Gantt 圖與 Ready Queue 如何隨時間片變化（WUWAOS 核心實際固定為 50 ms）。實機即時監測見 [workstate](../topics/monitoring.md)，延伸說明見 [計時器與輪流排程補充](../topics/scheduling.md)。

<SchedulingLab />

原始碼與驗收：[就緒佇列 (`kernel/proc/scheduler.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/scheduler.c) · [排程與切換 (`kernel/proc/process.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L1331) · [計時器排程規格](../SPEC-15-timer-rr.md) · [既有驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)
