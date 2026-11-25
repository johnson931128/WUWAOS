# 03 · 行程與執行緒

<script setup>
import ThreadFlow from '../.vitepress/theme/components/ThreadFlow.vue'
</script>

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

磁碟上的執行檔只是靜態的位元組；當我們想同時跑 Shell、文字編輯器與背景工作時，作業系統需要一個能追蹤「誰擁有哪塊記憶體、開了哪些檔案、現在跑到哪一行」的動態單位。更進一步，如果同一個程式裡想同時處理多個任務，每次都複製整份位址空間太昂貴了——於是現代 OS 把**資源擁有者（Process）**與**CPU 執行單位（Thread）**拆開設計。
</div>

## 恐龍書核心理論：Process 與 Thread 的分工

在恐龍書第 3–4 章中，**行程（Process）**是「執行中的程式」，而**執行緒（Thread）**是 CPU 排程的基本單位。兩者各自持有的資料非常清楚：

| 歸屬 | 核心結構 | 保存的內容 | 同一行程內是否共用？ |
| --- | --- | --- | --- |
| [行程](./glossary.md#process)（資源容器） | [PCB](./glossary.md#pcb) | PID、父 PID、頁表（位址空間）、檔案描述符表、工作目錄（CWD）、生命週期與退出狀態。 | **共用**（同一行程所有執行緒看到同一份頁表與 FD） |
| [執行緒](./glossary.md#thread)（執行現場） | [TCB](./glossary.md#tcb) | TID、執行狀態（Ready / Running / Waiting / Zombie）、使用者堆疊、核心堆疊、暫存器與等待原因。 | **各自獨立**（每條執行緒都有獨立堆疊與程式計數器） |

### 為什麼需要 `fork()` + `exec()`，還要有 `wait()`？

- **`fork()`**：複製目前行程建立子行程（父收子 PID、子收 0）。在現代 OS 與 WUWAOS 中，為了避免無謂複製大量記憶體，父子會先透過[寫入時複製（COW）](./08-virtual-memory.md#寫入時複製)共享實體頁框。
- **`exec()`**：把目前行程的位址空間換成另一支新程式，PID 保持不變。UNIX Shell 就是靠 `fork()` ＋ `exec()` 組合出各種外部命令與管線。
- **`exit()` 與 `wait()`**：子行程結束後不會立刻蒸發，而是先進入 **Zombie** 狀態保留退出碼（Exit Status），等父行程呼叫 `wait()` 收取結果後才正式回收 PCB。

## WUWAOS 真實實作與程式碼實例

### 程式碼實例：TCB 的真實定義（`kernel/proc/thread.h`）

在 WUWAOS 裡，行程與執行緒的關係直接反映在 `ThreadControlBlock` 結構上：每條 `Thread` 指向自己所屬的 `ProcessControlBlock`，同時各自擁有獨立的 `kernel_stack_top`、`user_stack_top`、`TrapFrame` 與 `KernelContext`：

```c
typedef enum {
    THREAD_UNUSED, THREAD_NEW, THREAD_READY,
    THREAD_RUNNING, THREAD_WAITING, THREAD_ZOMBIE
} ThreadState;

typedef struct ThreadControlBlock {
    long tid;
    struct ProcessControlBlock *process;   // 指向所屬行程（共享頁表與 FD）
    ThreadState state;
    struct ThreadControlBlock *ready_next; // 串入全系統 Ready Queue
    uintptr_t kernel_stack_top, user_stack_top;
    TrapFrame *frame;                      // U-mode 進入核心時保存的現場
    KernelContext context;                 // 核心態切換用的 callee-saved 暫存器
    WaitReason wait_reason;                // WAIT_NONE / WAIT_EVENT / WAIT_JOIN ...
    KernelEventWait event_wait;
    long exit_status;
} Thread;
```

### 同一個程式可以跑出什麼

磁碟上的 ELF 是可載入的程式映像。啟動兩次時，各自建立行程和位址空間，即使兩份記憶體的虛擬位址看起來相同，也能映射到不同頁框。在同一行程建立第二條執行緒時，兩條執行緒則共用行程資源。共享可寫資料需要同步，暫存器與堆疊仍分別保存。

以兩條執行緒共同操作一個行程內的緩衝區為例，程式只需傳遞緩衝區指標。若工作分到兩個不同的行程，可使用 Pipe 等行程間通訊機制，把資料交給另一邊處理。傳遞資源的方式和 CPU 如何逐條派發是兩個獨立的問題。 [行程結構](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.h) · [執行緒結構](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/thread.h)

## 行程如何建立、換程式和退出

| 操作 | WUWAOS 行為細節 |
| --- | --- |
| `spawn()` | 建立執行內建程式的新行程，附帶一條初始執行緒。 |
| `fork()` | 建立子行程、獨立頁表與核心堆疊；父子先共享常駐頁框（COW），繼承的 FD 指向相同的 OFD。 |
| `exec()`／`exec_argv()` | 先準備新程式映像與堆疊，成功後才替換原位址空間；沿用原 PID／TID，失敗則安全回到原程式。 |
| `exit()`／`wait()` | `exit()` 啟動整個行程退出流程；父行程以 `wait(pid, &status)` 或 `wait(-1, &status)` 收取結果。孤兒行程由排程器協助回收。 |

### 一次 Shell 命令怎麼把三個操作串起來

Shell 啟動外部程式時，`fork()` 讓子行程繼承準備好的資源，子行程可以先調整標準輸入、標準輸出，接著用 `exec` 換成目標程式。父行程視需要呼叫 `wait` 收回子行程的退出狀態。管線就是相同做法的延伸：Shell 先安排端點，再啟動各段，讓每個程式從自己的標準輸入與輸出讀寫。 [Shell 執行路徑](https://github.com/johnson931128/WUWAOS/blob/main/user/shell/shell_exec.c)

## 執行緒如何建立和回收

- `thread_create(entry, arg)`：在同一行程內建立新執行緒並回傳 TID，配發獨立使用者堆疊與[防護頁](./glossary.md#guard-page)（避免堆疊溢位靜默破壞相鄰記憶體）。
- `thread_exit(status)`：結束呼叫者；入口函式直接 `return` 時由使用者端 trampoline 代為呼叫。最後一條執行緒退出時會連帶結束行程。
- `thread_join(tid, &status)`：阻塞等待同行程指定目標退出，收取退出值並回收其 TCB 與額外堆疊。

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 設計取捨與容量邊界</span>

- 系統採靜態上限以保持記憶體可預測：最多 **16 個行程**、**32 格 TCB**，單一行程最多占 **8 格 TCB**。
- `fork()` 與 `exec()` 要求目前行程只占用 1 格 TCB（含尚未 `join` 的 Zombie 執行緒），避免多執行緒行程在 `fork` 後陷入鎖狀態不一致的經典難題。
- `kill(pid)` 僅允許終止直接子行程，且 PID 1 受保護。
</div>

## 觀察一條執行緒的狀態

切換「時間片輪轉」、「I/O 等待」與「退出與回收」，觀察單條執行緒如何在 `Ready`、`Running`、`Waiting`、`Zombie` 之間移動。[執行緒與硬體補充](../topics/threads.md)

<ThreadFlow />

原始碼與驗收：[行程與執行緒生命週期 (`kernel/proc/process.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c) · [TCB 定義 (`kernel/proc/thread.h`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/thread.h) · [Thread 規格](../SPEC-34-threads.md) · [ELF 規格](../SPEC-26-elf-exec.md) · [Thread Stage A 驗收](../thread-stage-a.md) · [COW 驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)
