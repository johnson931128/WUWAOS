<script setup>
import ThreadFlow from '../.vitepress/theme/components/ThreadFlow.vue'
</script>

# Thread、OS 與 CPU

WUWAOS 由核心直接排程 Thread。同一個 Process 可以有多條 Thread，共用位址空間與 FD table，各自保存暫存器、堆疊與等待狀態；目前 QEMU 只有一個 hart，同一時間只執行一條 Thread。

**B／C 的多 Thread API、額外堆疊與共享資源生命週期已實作，完整多 Thread runtime 驗收仍為 NOT_RUN。** [Spec 34](../SPEC-34-threads.md) 定義狀態與 ABI；[B／C 紀錄](../thread-stage-b.md) 說明實作與驗收邊界。階段 A 的歷史回歸不等於 B／C 已驗收。

## 一條 Thread 如何等待、恢復與退出

先看 Ready queue、hart 0 與 Waiting 的關係，再切換情境逐步閱讀。圖中的 A／B／C 是固定教學代號；不連接正在執行的 WUWAOS，也不改變核心時間片。

<ThreadFlow />

| 狀態 | 代表什麼 | 下一步由誰決定 |
| --- | --- | --- |
| Unused | 空的 TCB slot | 配置程式保留為 New |
| New | 正在準備 stack／frame，尚未入隊 | 建立成功後發布 Ready；失敗回滾 |
| Ready | 可執行，排在 FIFO 佇列 | 排程器從隊首派發 |
| Running | 目前在 hart 0 執行 | 時間片／yield、阻塞或退出 |
| Waiting | 等待事件、同步、子行程或 join | 完成、逾時、取消後排回 Ready |
| Zombie | 已退出，仍保存結果並占槽 | join 或 Process 群組清理後回收 |

Ready 不代表 I/O 一定成功；取消與逾時也會先喚醒，讓原核心呼叫恢復並處理錯誤。50 ms 是派發時間片，核心內仍不強制搶占，因此不是最大回應時間保證。

### 用 cat 對照真實追蹤

`cat code.txt` 由 shell fork 出子 Process 與初始 Thread。讀取檔案內容及 metadata 可能多次等待 block I/O；同一個 PID／TID 重複出現 Running → Waiting → Ready → Running，表示同一條 Thread 多次等待與恢復，不是不斷建立 Thread。只有一般 event 訊息時，不能逐筆確定等待的是哪一個來源。

在 WUWAOS shell 輸入 `trace off`，正常的 thread／exec／exit／wait 訊息會一起關閉；`cat` 正常完成時只輸出內容與原有提示字元。`trace on` 恢復追蹤。此全域設定重開機不保留，也不隱藏真正錯誤或程式主動輸出。

## 為什麼一個程式需要多條執行路徑

假設編輯器一邊接受鍵盤、一邊整理文字、一邊存檔。把這些工作寫進同一個會等待的迴圈，存檔期間可能無法處理輸入。多 Thread 讓工作各有自己的執行進度；等待 I/O 的那條暫停，其他 Ready Thread 仍有機會執行。

這是用途示意，**不是目前 fan 已有背景存檔**。事件迴圈也能處理部分相同問題；Thread 帶來的是另一種組織工作與阻塞行為的方法，不會自動把每個函式變快。

課程筆記 `notes/ch4 Thread.md` 把 Thread 視為 CPU 使用的基本單位，並列出 responsiveness、resource sharing、economy、scalability。本章將這些概念對照 WUWAOS 的實際限制，不沿用「一定大幅降低切換成本」或「不必處理 TLB」這類過度概括的說法。

## Process 保有資源，Thread 保存執行進度

Process 是資源與生命週期的容器；Thread 是可被排程的一條執行路徑。程式碼和資料可以共用，但執行到哪一行、暫存器值與函式呼叫堆疊要各自保存。

```text
One Process / one shared address space
+--------------------------------------------------------+
| PID / program / global data / heap / FD table / cwd     |
|                                                        |
| +----------------+ +----------------+ +--------------+ |
| | Thread A       | | Thread B       | | Thread C     | |
| | TID / PC       | | TID / PC       | | TID / PC     | |
| | registers     | | registers     | | registers   | |
| | user stack    | | user stack    | | user stack  | |
| | kernel stack  | | kernel stack  | | kernel stack| |
| | state / wait  | | state / wait  | | state / wait| |
| +----------------+ +----------------+ +--------------+ |
+--------------------------------------------------------+
```

| 項目 | 同 Process 的 Thread | 不同 Process，依 WUWAOS 目前設計 |
| --- | --- | --- |
| PID／TID | PID 相同，TID 各自不同 | PID 不同；每條 Thread 也有獨立 TID |
| 程式、global、heap | 共用同一個位址空間 | fork 複製私有內容，沒有 COW |
| 暫存器與 PC | 各自保存 | 各 Process 內的 Thread 各自保存 |
| 使用者與核心堆疊 | 每 Thread 各有一組 | 各 Process 的 Thread 各有一組 |
| FD table、cwd | 共用，變更會被其他 Thread 看見 | 各 Process 有自己的表與 cwd |
| 開啟檔案的 offset | 相同 OFD 共用，由鎖序列化 | fork／dup 也可能共用 OFD |
| 一條執行路徑等待 | 只讓該 Thread Waiting | 不直接改變其他 Process／Thread 的執行狀態 |

「自己的 stack」是使用與管理上的分配，不是同 Process 內的記憶體隔離。Thread A 拿到 B stack 的有效指標，仍可能讀寫它；B 結束後更不能繼續依賴其區域變數。

## OS 負責什麼，硬體負責什麼

硬體執行的是目前暫存器、PC 與位址轉譯設定所描述的指令流。OS 保存每條軟體 Thread 的執行狀態，再由排程器選擇下一條。建立 TCB 或呼叫 `thread_create` 不會增加實體 CPU。

例如 OS 恢復 B 的 `PC` 與 `sp` 後，CPU 從該 PC 取指、解碼並執行，指令更新暫存器或透過 MMU 讀寫記憶體，再繼續下一個位置。實際處理器可有 pipeline 或其他最佳化，這裡只描述概念上的執行順序。CPU 不會為了下一條 C 函式，自動到 TCB 表尋找新 Thread。

```text
Application       OS / kernel                    CPU / memory
-----------------------------------------------------------------
thread_create --> allocate TCB + stacks
                  publish READY
                         |
                         v
                  scheduler selects TID B
                         |
                  restore B's state -----------> PC + registers
                  select address space --------> MMU / page tables
                         |                            |
                         +--------------------------> execute B
                                                      |
                  trap handler <---------------- timer / ECALL
                  save state in RAM
                  decide what runs next
```

| OS 的責任 | 硬體／平台提供的機制 |
| --- | --- |
| 建立 TID、TCB、stack，決定 Thread 狀態 | 暫存器與 RAM 保存目前／暫停的執行資訊 |
| 決定 Ready queue 順序與時間片 | 計時器能在設定條件下產生中斷 |
| 保存／恢復程式狀態，挑選下一條 Thread | trap 入口與特權切換指令 |
| 建立頁表、配置頁面、處理缺頁 | MMU 依頁表轉譯並檢查存取權限 |
| 管理 join、mutex、I/O 等待與喚醒 | 原子指令、裝置佇列與中斷等基礎能力 |

計時器不會自己挑選 TID；mutex 也不是 CPU 替程式辨識「誰擁有這段 C 資料」。這些政策與身分由 OS／程式庫維護。

## 軟體 Thread、hart、core 與 SMT

RISC-V 使用 **hart** 表示執行環境中能推進指令的硬體執行資源，與 OS 的軟體 Thread context 分開。一個執行環境可把客體 harts 映射到主機資源；所以 guest hart 數不能直接當成主機實體 core 數。[RISC-V ISA：Execution Environments and Harts](https://docs.riscv.org/reference/isa/v20260120/unpriv/intro.html)

| 名詞 | 指的是什麼 | 與 WUWAOS 的關係 |
| --- | --- | --- |
| Software Thread | OS／程式庫管理的執行路徑 | 已實作的 TID／TCB 與 Thread API |
| Hart | RISC-V 執行環境提供的硬體執行資源 | 目前 QEMU `-smp 1` 提供一個 guest hart |
| CPU core | 處理器核心的硬體實作單位 | 多核心平台可提供多個可執行 context |
| SMT | 同一實體核心提供多個硬體執行 context 的實作方式 | 第一版不管理 SMT 拓樸 |

SMT 可讓同一核心的多個硬體 context 各保有架構狀態，並共享部分執行單元與 cache；它不等於增加相同數目的完整獨立核心，效能也不保證等比例增加。Intel Hyper-Threading 是 SMT 的一個例子，不是 WUWAOS 的 RISC-V 平台功能。[Intel 處理器資料表：Hyper-Threading](https://edc.intel.com/content/www/us/en/design/ipla/software-development-platforms/client/platforms/alder-lake-desktop/12th-generation-intel-core-processors-datasheet-volume-1-of-2/001/intel-hyper-threading-technology/)

```text
Software threads:   A     B     C     D
                     \    |    /     /
                      \   |   /     /
OS scheduler ---------- selects available execution contexts

Case 1: one hart       [ A ] -> [ B ] -> [ C ] -> [ D ]
                      time-sharing on one guest execution context

Case 2: two harts      hart 0: [ A ] -> [ C ]
                      hart 1: [ B ] -> [ D ]
                      requires an SMP-capable kernel

Case 3: SMT core       core 0: [context 0] [context 1]
                                \           /
                                 shared resources
```

這是拓樸示意，不是 WUWAOS trace。第一版只做 Case 1。未來增加 harts 時，仍要實作每 hart 的 current Thread／scheduler stack、跨 hart 鎖、記憶體順序、TLB shootdown、啟動與喚醒協定；修改 `-smp` 不會補齊這些機制。

## 單 hart 為什麼仍然有價值

單 hart 下，多條 Thread 交替前進是 concurrency（並行處理工作）；多個可同時執行的硬體 context 才能讓不同指令流產生硬體上的平行執行。排程與同步會消耗時間，CPU-bound 工作不會因 Thread 數增加就自動縮短。

以下每格代表一個教學步驟，不是毫秒或實際效能量測：

```text
Step                 0       1       2       3       4
----------------------------------------------------------
hart 0 executes      A       B       C       A       B
Thread A             RUN     WAIT    READY   RUN     READY
Thread B             READY   RUN     READY   READY   RUN
Thread C             READY   READY   RUN     WAIT    WAIT
Disk                 submit  ...     done
```

A 提交磁碟工作後阻塞，B／C 使用 CPU；磁碟完成時 A 變 Ready，排程器稍後才讓它 Running。裝置完成通知不是「A 立即取得 CPU」的保證；同一時刻 hart 0 仍只執行一條軟體 Thread 的指令流。

這需要所有下層同步都辨識 Thread。若 KernelGate 仍只辨識 PID，A 持有 FS gate 等待磁碟時，B 可能因同 PID 被誤認成遞迴進入，破壞正在進行的交易。這也是 WUWAOS 不能只新增 `thread_create` 就完成工作的原因。

## 一次 RISC-V 切換要保存什麼

Trap 進入 S-mode 時，架構更新 `sepc`、`scause`、`stval` 與 `sstatus` 相關狀態；一般整數暫存器和 stack 切換由入口組合語言處理。返回時 `sret` 依 `sepc` 與特權狀態續行；`satp` 決定位址轉譯使用的頁表根。這些是執行機制，不包含 Ready queue 的選擇政策。[RISC-V Supervisor ISA](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html)

在 WUWAOS 目前原始碼中，對應關係如下：

| 保存資料 | 現行位置 | 用途 |
| --- | --- | --- |
| User registers 與 `sepc/sstatus/scause/stval` | TCB 的 `TrapFrame`，目前 288 bytes | 保留被 trap 打斷的使用者狀態 |
| `ra/sp/s0..s11` | `KernelContext`，目前 112 bytes | 在核心 C 函式呼叫邊界暫停與恢復 |
| Kernel C call stack | 每 TCB 的 16 KiB kernel stack | I/O 等待後從原函式繼續 |
| User function frames | User stack 所在的 RAM／swap pages | 區域變數、函式返回位置等 |
| Page table root | `Process.page_table` | 留在共用 Process 資源 |

```text
Thread A in U-mode
    |
    | timer / ECALL / fault
    v
trap.S -> A's TrapFrame -> kernel handler
                               |
                               | block / preempt
                               v
                      save A's KernelContext
                               |
                               v
                       scheduler kernel stack
                               |
                               | select B
                               v
                      restore B's KernelContext
                               |
                               v
                      finish B's kernel call
                               |
                               v
                      restore B's TrapFrame -> sret
```

CPU 當下只有目前 context 的暫存器值；暫停 Thread 的值保存在 RAM，不是每新增一條 Thread 就在 CPU 內多一組永久暫存器。`TrapFrame` 與 `KernelContext` 解決不同的返回位置，不可只保存後者就宣稱能從任意使用者指令安全切換。

LP64 函式呼叫須維持 16-byte stack alignment；`tp` 在 ABI 中是 thread pointer，但要有 runtime 配置 TLS 與載入器／編譯器配合，才有每 Thread 的 TLS 資料。本版只保存整數 context，尚不提供 TLS。[RISC-V psABI：Integer Register／Calling Convention](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/)

## 共用記憶體如何維持生命週期

共享讓 Thread 能直接交換資料，也讓一條 Thread 的錯誤可能影響整個 Process。以邏輯上的 read／add／write 拆解，交錯可能如下：

```text
shared counter starts at 0

A: read 0 -> calculate 1 ---------> write 1
B:          read 0 -> calculate 1 ---------> write 1

two increments, final value 1
```

這只是遺失更新的概念示意；不是鼓勵執行有未定義資料競爭的 C 程式。實作示範應用明確的同步點控制步驟，正式共用更新用 mutex 或合適的原子操作。`volatile` 不能代替互斥、原子性或同步協定。

目前 B／C 已接入以下保護，完整 runtime 驗收仍待完成：

- A read 等待期間，B close 並重用同一 FD number：已開始的 read 要保留原 OFD，不能醒來後讀到另一個檔案。
- A 改 cwd：B 已開始處理的相對路徑使用當次快照，不應隨等待時的 cwd 改變。
- A thread_exit：保留 B 仍在使用的 page table、FD table 與 TTY；A 的 stack 也要等退出原 kernel stack 後才能回收。
- A 呼叫 Process exit：通知所有 Thread 安全退出，等 DMA／交易清理完成後才能銷毀共用空間。

同 Process 切換通常能共用位址空間，但是否省略 `satp` 切換與 TLB fence 是核心實作決定。WUWAOS 現在的 `vm_enable` 明確執行 fence，第一版保留這條安全路徑；本章不宣稱已量到 Thread 切換較快，也不宣稱 context switch 一定清空或一定保留所有 cache。

## WUWAOS 目前提供的 API 與限制

| 項目 | 目前實作；runtime 待驗收 |
| --- | --- |
| 建立／等待／退出 | `thread_create(entry, arg)`、`thread_join(tid, status)`、`thread_exit(status)`、`gettid()` |
| 共用資源 | 每 Process 一份 VmSpace／FD table／cwd；同 Process 的 TID 不同 |
| 容量 | 16 Process、全系統 32 Thread、每 Process 最多 8 Thread，含未 join 的 Zombie |
| User stack | 初始 Thread 保留原一頁 stack；額外 Thread 每個 32 KiB，另有 guard |
| 排程 | 單 hart RR，user timer 可搶占；核心只在指定等待點切換 |
| 終止語意 | Thread 函式 return 等同 thread_exit；既有 exit 結束整個 Process |
| fork／exec | 占用 Thread slot 超過一個時回 ERR_BUSY，含尚未回收的 Zombie |

這裡的「由核心排程的使用者 Thread」會在 U-mode 執行函式，並在 syscall／fault／interrupt 時進入核心；它與永遠處理核心工作的 kernel worker 不同。第一版也不是把多條 user-level fibers 全映射到單一 OS Thread 的 M:1 模型。

以下是符合目前 API 的程式片段，需整合至使用 WUWAOS user library 的程式；不是可直接貼進 shell 的命令，也不是已驗收的測試結果：

```c
#include "user/lib/api.h"

static long shared_result;

static long worker(void *argument) {
    long *result = argument;
    *result = 42;
    return 7; /* trampoline calls thread_exit(7) */
}

void thread_example(void) {
    long status = 0;
    long tid = thread_create(worker, &shared_result);
    if (tid < 0) return; /* creation failed */
    if (thread_join(tid, &status) != tid) return; /* handle join error */
    /* successful join: shared_result == 42, status == 7 */
}
```

例子使用 Process 生命週期的 static 資料，避免把很快返回的 caller stack 當成長期有效的 argument。成功 join 要保證能觀察到已退出 Thread 的寫入；若兩條 Thread 都還在修改 shared_result，則仍需同步保護。

目前可用 `ps`、`pcb`、`demo sync` 等既有命令觀察；`ps`／`pcb` 仍以 Process 與代表 TCB 為主，不能當成完整 Thread 清單。階段 D 的 `demo threads`／專用 Thread 清單尚不存在。正常啟動 fan 也不會自動建立多條 Thread：shell fork 產生子 Process 與初始 Thread，exec 沿用該 PID／TID，fan 目前沒有呼叫 thread_create。[完整 ABI、生命週期、錯誤與驗收條件](../SPEC-34-threads.md)

## 資料來源與實作對照

硬體與 ABI 觀念參考上述 RISC-V 官方 ISA、psABI 與 Intel 特定處理器資料表；Intel 例子只用於說明 SMT。Thread 容量、API、stack 位址與退出規則則是 WUWAOS 設計選擇。課程脈絡可對照 `notes/ch4 Thread.md`，本章沒有轉載筆記中的第三方圖片。

目前已存在的實作：

- [Process 與執行狀態](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.h)、[排程器](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/scheduler.c)。
- [Trap 入口](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/trap.S)、[核心 context switch](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/swtch.S)、[計時器](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/timer.c)。
- [KernelGate](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/gate.c)、[mutex／semaphore](https://github.com/johnson931128/WUWAOS/blob/main/kernel/ipc/sync.c)、[VM](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c)。

教學圖不是硬體模擬器或真實 trace。多 Thread API 與 B／C 生命週期為 CODE_PRESENT，階段 D 介面為 NOT_IMPLEMENTED，多 Thread 完整 QEMU／效能測試為 NOT_RUN；文件網站建置與 OS 驗收分開記錄。
