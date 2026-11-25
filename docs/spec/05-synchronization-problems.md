# 05 · 為什麼要同步

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

假設兩條執行緒都想把共享變數 `count` 加一。在 C 語言裡 `count++` 看起來只有一行，編譯成機器碼卻是三步：**讀取（Load）→ 加一（Add）→ 寫回（Store）**。如果兩條執行緒同時讀到 `0`，各自算出 `1` 再依序寫回，最後結果竟然是 `1` 而不是 `2`——其中一次更新被無聲蓋掉了。這就是[競爭條件](./glossary.md#race-condition)（Race Condition），即使只有一顆單核心 CPU，只要在「讀」與「寫」之間發生計時器切換，同樣會出事。
</div>

## 恐龍書核心理論：臨界區問題的三大條件

恐龍書第 6 章把這類問題抽象為 **Critical-Section Problem（臨界區問題）**。任何正確的同步解法，都必須同時滿足三個條件：

1. **Mutual Exclusion（互斥）**：同一時間最多只能有一條執行緒進入[臨界區](./glossary.md#critical-section)存取共享資料。
2. **Progress（進展）**：如果目前沒有人在臨界區內，且有執行緒想進入，不能讓不想進入的人擋路，必須在有限時間內選出下一位進入者。
3. **Bounded Waiting（有限等待）**：一條執行緒發出進入請求後，其他執行緒搶先進入的次數必須有上限，避免發生飢餓（Starvation）。

## 作業系統中最常見的三類同步問題

| 同步問題 | 經典情境 | WUWAOS 的處理方式 |
| --- | --- | --- |
| **共享資料同時改動** | 兩條執行緒同時更新計數器或鏈結串列。 | 用 [Mutex](./glossary.md#mutex) 保護臨界區，讓「讀—改—寫」由單一持有者原子完成。 |
| **等待資源或生產者** | 消費者要等生產者把資料放進緩衝區（Producer–Consumer）。 | 用 [Semaphore](./glossary.md#semaphore) 記下可用資源份數，或由核心以事件等待讓出 CPU，不浪費算力忙等（Busy Waiting）。 |
| **遺失喚醒（Lost Wakeup）** | 讀者檢查 Pipe 為空、正要睡覺前，寫者搶先寫入資料並發出通知——讀者沒收到通知卻安心睡死了。 | 核心在睡覺前記錄事件序號（`sequence`），真正登記等待時再核對；序號已變就直接重檢條件，不睡下去。 |

## WUWAOS 實例：如何靠 `generation` + `sequence` 防堵遺失喚醒

在真實核心裡，單靠簡單的 `sleep(channel)` / `wakeup(channel)` 無法解決「先通知、後睡眠」以及「事件物件被回收重用」的問題。WUWAOS 在 `kernel/proc/event.h` 設計了 `KernelEventSource` 與 `KernelEventWait`：

```c
typedef struct KernelEventSource {
    uint32_t generation;   // 物件世代號：避免回收重用後誤收舊事件
    uint64_t sequence;     // 每次 notify 遞增：用來偵測「檢查後到入睡前」是否已發生通知
    unsigned active : 1;
    unsigned retired : 1;
} KernelEventSource;
```

- 等待者在檢查條件失敗前，先呼叫 `event_source_snapshot()` 拍下當下的 `(generation, sequence)`。
- 真正呼叫 `thread_wait_event()` 準備睡眠時，核心核對快照序號；若序號已推進，表示通知早已發生，立刻返回重檢條件而不進入睡眠。

## Channel 喚醒與事件等待的適用邊界

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 設計區分</span>

- **`sleep(channel)` / `wakeup(channel)`**：適合已約定好順序的教學 Shell Demo；`wakeup()` 只喚醒呼叫當下已在該號碼上等候的執行緒。
- **Semaphore**：若通知必須被「記住」而不是錯過就消失，改用帶計數的 Semaphore。
- **喚醒後一律重檢條件**：像 Pipe 讀取被喚醒回到 Ready Queue 後，執行時仍會在核心迴圈內重新檢查緩衝區資料量與寫端是否存在，才決定讀取、繼續等，或回傳 EOF（0）。
</div>

程式對照與驗收：[等待與喚醒 (`kernel/proc/process.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L771) · [事件等待 (`kernel/proc/process.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L1096) · [同步操作示範](../SPEC-23-sync-demo.md) · [Thread 驗收](../thread-stage-b.md)
