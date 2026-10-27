# 04 · 行程管理

程式是一份指令與資料；Process（行程）是這份程式正在執行的一次實例。兩個行程可以執行同一份程式，卻擁有不同 PID、暫存器與私有資料。

## PCB 保存什麼

原始碼：[kernel/process.h（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/process.h)。

`ProcessControlBlock` 就是 PCB（行程控制區塊），`processes[4]` 是目前行程表。`page_table` 找到私有位址空間，`frame` 找到暫存器保存狀態；等待與 I/O 欄位記錄阻塞後還沒完成的事。

`ready_next` 是指向下一個 Ready PCB 的指標，不是另一份行程副本。核心堆疊每欄固定 16 KiB；使用者頁面則由 VM 按需要配置。

## 狀態怎麼走

```text
Unused → New → Ready → Running
                 ↑        ├─ yield → Ready
                 │        ├─ sleep／wait／阻塞 I/O → Waiting
                 └─ 喚醒 ─┘
Running／Ready／Waiting → exit／kill → Zombie → wait → Unused
```

Zombie（已退出、待回收）仍保留結束結果與資源，讓父行程能透過 wait 取得結果。它已不再執行，也不在 Ready queue（就緒佇列）裡。

## 建立、複製與替換

`create_process` 取得空 PCB，準備頁表與初始 TrapFrame，成功後才公布 Ready。建立中途失敗就回收不完整空間。

`process_fork` 複製父行程的使用者內容與已越過 ecall 的暫存器狀態。父 a0 得到子 PID，子 a0 得到 0。核心堆疊不直接複製；常駐頁與已交換資料都取得獨立內容，目前沒有 COW（寫入時複製）。

`process_exec` 先準備新空間與必要參數，成功後才切換並銷毀舊空間。PID 和父子關係保留，下一次返回使用者模式時從新程式入口開始。

## 在 QEMU 觀察

從新開機開始，逐條輸入：

```text
fork
ps
wait 2
run worker
ps
pcb 3
wakeup 7
wait 3
ps
```

父子應顯示不同的私有 data／stack 值。worker 等待事件通道 7，喚醒後可回收。若先前已建立過行程，請以實際輸出的 PID 取代 2／3；`sleep(7)` 是等待通道 7，不是睡 7 秒。

## 回收與限制

exit 關閉端點並留下 Zombie；wait 取得結果後釋放使用者頁、Swap slots（交換區槽位）、私有頁表及 PCB。孤兒行程另有接管與回收流程。狀態結果寫入父使用者空間失敗時，不會先丟棄 Zombie。

最多 4 個行程，exec 仍只選擇核心內嵌的程式 ID，不是載入任意磁碟執行檔。沒有執行緒與完整作業控制。

觀念參考：`notes/ch3 Process.md`。實作脈絡見 [Spec 02](../SPEC-02-process-lifecycle.md)、[Spec 05](../SPEC.md)，目前私有記憶體複製見 [Spec 10](../SPEC-10-demand-paging.md)。
