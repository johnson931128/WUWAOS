# 05 · 為什麼要同步

假設兩條執行緒都想替一個計數器加一。它們各自先讀到 `0`，各自算出 `1`，再依序寫回去。最後數字是 `1`，其中一次更新被蓋掉了。這就是[競爭條件](./glossary.md#race-condition)。即使只有一個 Hart，也可能在「讀」與「寫」之間被切換出去。

## 三個常見問題

| 問題 | 例子 | WUWAOS 的處理方式 |
| --- | --- | --- |
| 共用資料被同時改動 | 兩條執行緒更新同一份計數或佇列。 | 用 [Mutex](./glossary.md#mutex) 保護[臨界區](./glossary.md#critical-section)，讓一整段讀取、修改和寫回由單一持有者完成。 |
| 要等資源或資料準備好 | 接收端要等另一條執行緒放入一筆工作。 | 用 [Semaphore](./glossary.md#semaphore) 記下可用份數，或由核心用事件和等待條件記錄 I/O 是否完成。等待者可以先讓出 CPU。 |
| 檢查完才睡，通知剛好錯過 | 讀者看到 Pipe 是空的，正要睡覺，寫者這時已放入資料並通知。 | 核心在等待前記下事件序列，登記等待時再核對。序列已變就重新檢查條件，避免在已發生通知後才睡下。 |

## Channel 喚醒和事件等待

`sleep(channel)` 和 `wakeup(channel)` 適合已約定好順序的 Shell demo。`wakeup()` 喚醒呼叫當下已在該號碼上等候的執行緒。若工作需要保留已到達的通知，可以用有計數的 Semaphore。核心 I/O 則需要另外辨識事件來源與其使用世代，因為裝置請求或物件欄位可能已被回收再使用。

等待因通知而結束時，執行緒先回到 Ready Queue。像 Pipe 讀取這類操作，喚醒後仍要在核心重新檢查資料量和寫端是否存在，才能決定讀取、繼續等待，或回傳結束。

程式對照見 [等待與喚醒](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L771) · [事件等待](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c#L1096) · [同步操作示範](../SPEC-23-sync-demo.md)。本章的加一例子用來解釋交錯順序，實際 Pipe、join 和事件喚醒驗收見[Thread 驗收](../thread-stage-b.md)。

接著讀[同步工具怎麼做](./06-synchronization-tools.md)。
