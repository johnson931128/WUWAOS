# 06 · 同步工具怎麼做

WUWAOS 提供使用者程式可請求的 Mutex 和 Semaphore。核心另外用 Gate 保護檔案系統、虛擬記憶體和 Block 裝置的長操作，並以事件通知等待者。它們都能讓等待者暫停，但保護的對象和喚醒後的動作不同。

## 使用者程式的 Mutex 與 Semaphore

| 工具 | 核心維護的資料 | 等待與喚醒 |
| --- | --- | --- |
| [Mutex](./glossary.md#mutex) | 目前擁有者是一條執行緒。 | 有擁有者時加入等待佇列。解鎖只能由擁有者發出，解鎖後喚醒隊首。原擁有者退出時標記鎖已受損，等待者收到 `SYNC_ERR_OWNER_DIED`。 |
| [Semaphore](./glossary.md#semaphore) | 還可以取用幾份資源。 | `wait()` 在數量大於零時取走一份，否則等候。`post()` 加回一份並喚醒隊首，即使當下無人等待，這份資源也會保留。 |

兩種物件共用 8 格核心表，程式以 handle（物件編號）操作。等待者被喚醒後會重新嘗試取得資源，因此喚醒順序是 FIFO，取得資源的順序仍受後續派發影響。尚有持有者或等待者時不能銷毀物件。同一執行緒重複鎖住自己的使用者 Mutex 會回傳錯誤。

## 核心的 Gate 與事件

[KernelGate](./glossary.md#kernel-gate) 以 Thread 和 TID 辨識擁有者。同一擁有者可以再次進入，每次進入需配對離開。其他執行緒遇到已占用的 Gate，可以等擁有者釋放。中斷處理與排程器輪詢等不能睡眠的路徑，遇到忙碌 Gate 時回傳 `ERR_BUSY`。

[事件來源](./glossary.md#kernel-event) 用 generation 辨識物件的這一次使用，用 sequence 記下自上次查閱後是否發生通知。等待者可同時設定 deadline（最後等到的時間）。通知、逾時或取消會把結果留下，讓執行緒回來後知道等待為何結束。共享核心鎖序列化短暫的核心狀態更新，Gate 則保護可能跨越執行緒睡眠的操作邊界。

可以用 Shell `demo sync` 觀察 Mutex 和 Semaphore。Core 測試覆蓋 Gate 所有權與 TID 的行為，QEMU 既有驗收覆蓋實際 I/O 事件等待和喚醒。 [同步實作](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/ipc/sync.c) · [Gate](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/gate.c) · [Event](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/event.c) · [同步規格](../SPEC-22-synchronization.md) · [非同步 I/O 驗收](../merge-2026-11-10.md)
