# 03 · 行程與執行緒

<script setup>
import ThreadFlow from '../.vitepress/theme/components/ThreadFlow.vue'
</script>

把程式跑起來時，核心建立一個[行程](./glossary.md#process)，用 PID 辨認。行程持有頁表、檔案描述符、工作目錄等資源。實際在 CPU 上前進的是[執行緒](./glossary.md#thread)，用 TID 辨認。每條執行緒有自己的堆疊、暫存器和執行位置。同一行程內的執行緒透過同一份頁表看到共用資料。

| 核心記錄 | 保存什麼 |
| --- | --- |
| [PCB](./glossary.md#pcb) | 行程的 PID、父 PID、頁表、檔案描述符、工作目錄、生命週期與退出狀態。 |
| [TCB](./glossary.md#tcb) | 執行緒的 TID、所屬行程、狀態、等待原因、使用者／核心堆疊和保存的執行進度。 |

### 同一個程式可以跑出什麼

磁碟上的 ELF 是可載入的程式映像。啟動兩次時，各自建立行程和位址空間，即使兩份記憶體的虛擬位址看起來相同，也能映射到不同頁框。在同一行程建立第二條執行緒時，兩條執行緒則共用行程資源。共享可寫資料需要同步，暫存器與堆疊仍分別保存。

以兩條執行緒共同操作一個行程內的緩衝區為例，程式只需傳遞緩衝區指標。若工作分到兩個不同的行程，可使用 Pipe 等行程間通訊機制，把資料交給另一邊處理。傳遞資源的方式和 CPU 如何逐條派發是兩個獨立的問題。 [行程結構](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.h) · [執行緒結構](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/thread.h)

## 行程如何建立、換程式和退出

| 操作 | 呼叫後會發生什麼 |
| --- | --- |
| `spawn()` | 建立執行內建程式的新行程，附帶一條初始執行緒。 |
| `fork()` | 建立子行程，準備獨立頁表與核心堆疊，複製返回使用者程式所需的暫存器。父收到子 PID，子收到 0。父子先共享常駐頁框，需要寫入時才[拆分](./08-virtual-memory.md#寫入時複製)。繼承的 FD 指向相同的開啟檔案記錄，所以可能共用讀寫位置。 |
| `exec()`／`exec_argv()` | 先準備新程式映像和堆疊，成功後才換掉原位址空間。沿用 PID 與 TID，從新程式入口繼續；準備失敗會回到原程式。 |
| `exit()`／`wait()` | `exit()` 開始整個行程的退出流程。核心清理其執行緒和資源，父行程可用 `wait(pid, &status)` 收取結果，或用 `wait(-1, &status)` 等任一子行程。沒有父行程的子行程由排程器安排回收。 |

`kill(pid)` 只接受符合條件的直接子行程，PID 1 受保護。`fork()` 和 `exec()` 要求目前行程只占用一格 TCB，尚未 `join` 的 Zombie 執行緒也算一格。使用外部 ELF 時，核心支援受限的靜態 RV64 映像與 `exec_argv()` 傳遞的引數。

### 一次 Shell 命令怎麼把三個操作串起來

Shell 啟動外部程式時，`fork()` 讓子行程繼承準備好的資源，子行程可以先調整標準輸入、標準輸出，接著用 `exec` 換成目標程式。父行程視需要呼叫 `wait` 收回子行程的退出狀態。管線就是相同做法的延伸：Shell 先安排端點，再啟動各段，讓每個程式從自己的標準輸入與輸出讀寫。 [Shell 執行路徑](https://github.com/johnson931128/WUWAOS/blob/main/user/shell/shell_exec.c)

## 執行緒如何建立和回收

- `thread_create(entry, arg)` 在同一行程內建立新的執行緒，成功回傳 TID。額外執行緒有自己的使用者堆疊與[防護頁](./glossary.md#guard-page)。
- `thread_exit(status)` 結束呼叫者。入口函式直接回傳時，由啟動程式代為呼叫。最後一條執行緒退出時，行程也會進入退出流程。
- `thread_join(tid, &status)` 等同一行程內的指定目標退出，收取結果並回收其 TCB 與額外堆疊。

系統上限為 16 個行程、32 格 TCB，每個行程最多占 8 格。既有 QEMU 案例涵蓋建立、阻塞 join、群組退出與 COW 隔離。 [行程與執行緒生命週期](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/proc/process.c) · [Thread 規格](../SPEC-34-threads.md) · [ELF 規格](../SPEC-26-elf-exec.md) · [Thread Stage A 驗收](../thread-stage-a.md) · [COW 驗收](../SPEC-35-copy-on-write.md#_2026-11-22-qemu-驗證)

## 觀察一條執行緒的狀態

比較時間片到期、等待 I/O 和等待 `join`。圖解以單 Hart 示意執行緒的狀態轉換。 [執行緒與硬體補充](../topics/threads.md)

<ThreadFlow />

## 恐龍書與課堂筆記

恐龍書第 10 版 §3.1–3.4、§4.1–4.4。Linux 的行程和 POSIX 執行緒用於教材對照。

- [行程、PCB、生命週期與行程間通訊](../theory/03-processes.md)
- [執行緒、多核心與執行緒模型](../theory/04-threads.md)
