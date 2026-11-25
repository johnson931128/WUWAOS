# 06 · 同步工具怎麼做

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

既然知道不能讓多條執行緒同時踩進臨界區，我們該如何打造「互斥鎖（Mutex）」與「號誌（Semaphore）」？更棘手的是：如果搶不到鎖就一直在 `while (locked);` 裡空轉（Spinlock），在單核心或長 I/O 情境下會白白燒光整個時間片。作業系統必須把同步工具和排程器結合，讓搶不到資源的執行緒**主動讓出 CPU 進入等待佇列**。
</div>

## 恐龍書核心理論：Mutex Lock vs. Semaphore

恐龍書第 6 章把高階同步原語分成兩大主力：

- **Mutex Lock（互斥鎖）**：強調**所有權（Ownership）**。同一時間只能有一條執行緒持有鎖，且**只能由原本上鎖的那條執行緒解鎖**；專門保護臨界區資料。
- **Semaphore（號誌）**：強調**資源計數與訊號同步**。內部維護一個整數計數器 `S`，透過兩個原子操作 `wait()`（P 操作，減一，不足則等）與 `signal()` / `post()`（V 操作，加一並喚醒等待者）。不需要「誰 wait 就誰 post」，因此非常適合 Producer–Consumer 與事件通知。

## 使用者程式的 Mutex 與 Semaphore（WUWAOS 實作）

| 工具 | 核心維護的狀態 | 等待、喚醒與錯誤保護 |
| --- | --- | --- |
| [Mutex](./glossary.md#mutex) | 目前擁有者的執行緒指標／TID | 已被鎖住時加入 FIFO 等待佇列；僅擁有者可解鎖並喚醒隊首。同一執行緒重複鎖住自己回傳錯誤；若持有者意外退出，核心標記鎖已受損並讓等待者收到 `SYNC_ERR_OWNER_DIED`。 |
| [Semaphore](./glossary.md#semaphore) | 剩餘可用資源份數 | `wait()` 在份數 > 0 時扣 1 立即返回，否則阻塞等待。`post()` 加 1 並喚醒隊首；即使當下無人等待，資源份數仍會被保留。 |

在 WUWAOS 中，兩種使用者同步物件共用 **8 格核心表**，使用者程式透過整數 `handle` 操作；尚有持有者或等待者時禁止銷毀物件。被喚醒的執行緒回到 Ready Queue 後會重新嘗試取得資源，因此喚醒順序雖是 FIFO，實際拿鎖順序仍受排程派發影響。

## 核心的 Gate 與事件：保護「會睡覺的長操作」

在作業系統核心內部，保護磁碟讀寫、檔案系統交易或虛擬記憶體換頁時，不能一直握著短暫的自旋鎖去睡覺（否則會死鎖或卡死所有其他 Hart）。WUWAOS 為此實作了可重入的 [KernelGate](./glossary.md#kernel-gate)（`kernel/proc/gate.h`）：

```c
typedef struct KernelGate {
    struct ThreadControlBlock *owner; // 目前持有該 Gate 的執行緒
    long owner_tid;                   // 配對 TID 辨識所有權
    unsigned depth;                   // 支援同一擁有者重入（每次 enter 需配對 leave）
    KernelEventSource event;          // Gate 釋放時通知等待者
    unsigned initialized : 1;
} KernelGate;
```

- **可睡眠路徑（`kernel_gate_enter`）**：遇到已被占用的 Gate 時，透過內嵌的 `KernelEventSource` 讓出 CPU，等原擁有者呼叫 `kernel_gate_leave()` 時再被喚醒。
- **不可睡眠路徑（`kernel_gate_try_enter`）**：中斷處理器或排程器輪詢等不能阻塞的路徑，遇到忙碌 Gate 直接回傳 `ERR_BUSY`，絕不睡眠。

<div class="tb-box wuwaos">
<span class="tb-label">兩層核心鎖的分工對照</span>

- **共享核心鎖（Spin / Big Kernel Lock）**：序列化極短暫的核心記憶體結構更新（如修改 Ready Queue、檢查事件序號），持有期間絕不睡眠。
- **KernelGate**：保護跨越磁碟 I/O、Swap 或檔案交易等「可能睡眠」的長操作邊界，持有期間允許執行緒讓出 CPU。
- **親手試試**：在 WUWAOS Shell 執行 `demo sync`，可以直接觀察使用者態 Mutex 與 Semaphore 的實際交錯與等待行為。
</div>

原始碼與驗收：[同步實作 (`kernel/ipc/sync.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/ipc/sync.c) · [Gate (`kernel/proc/gate.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/gate.c) · [Event (`kernel/proc/event.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/event.c) · [同步規格](../SPEC-22-synchronization.md) · [非同步 I/O 驗收](../merge-2026-11-10.md)
