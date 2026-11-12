<script setup>
import ThreadFlow from '../.vitepress/theme/components/ThreadFlow.vue'
</script>

# 執行緒如何在 WUWAOS 裡運作

程式讀檔時，磁碟可能還在找資料，CPU 卻已經可以做別的事。作業系統會暫停等待資料的**執行緒（thread）**，讓另一條執行緒先跑。資料就緒後，原本的執行緒會回到排程佇列，輪到它時，再從暫停的地方繼續。

這個做法也能用在同一個程式裡。例如文書編輯器可以讓一條執行緒處理鍵盤輸入，另一條負責存檔。存檔等磁碟時，介面仍有機會處理下一個按鍵。以下用這個分工說明概念，並以 WUWAOS 的 `cat` 追蹤示範實際的等待與恢復。

## Process 保有資源，Thread 保存執行進度

**行程（process）**負責管理程式使用的資源。**執行緒**負責執行指令，同一個行程可以有多條。以編輯器為例，輸入和存檔的執行緒都能存取同一份文件資料，但兩條執行緒可能正在跑不同的函式，因此需要各自的暫存器和堆疊。

- **PID 和 TID** 分別是行程和執行緒的編號。查到同一個 PID 下有不同 TID，就表示這些執行緒屬於同一個行程。
- **位址空間（address space）** 是行程能使用的虛擬位址範圍及其映射。同一行程的執行緒可透過同一個指標存取全域變數或堆積上的動態配置資料。WUWAOS 的 `fork()` 會複製行程的私有記憶體。
- **程式計數器（PC）** 記錄 CPU 接著要取指的位置。**堆疊（stack）** 保存函式呼叫所需的資料，例如區域變數與返回位置。每條執行緒都有使用者程式和核心各自使用的堆疊。由於執行緒共用位址空間，取得另一條執行緒的有效堆疊指標時，仍可透過它存取記憶體。
- **檔案描述元（file descriptor，FD）** 是程式傳給 `read()`、`close()` 等函式的整數。**FD table** 用這個整數找到對應的開啟檔案物件。同一行程共用這張表，所以其中一條執行緒關閉 FD，會影響其他執行緒之後使用該 FD 的結果。

<figure class="thread-figure">
  <figcaption>同一個行程中的共用資源與兩條執行緒</figcaption>
  <div class="thread-process">
    <p><strong>共用資源</strong><br>程式碼、全域資料、堆積（heap）、開啟檔案表、目前工作目錄</p>
    <ul class="thread-owners">
      <li><strong>輸入執行緒 A</strong><br>自己的 TID、暫存器、使用者堆疊、核心堆疊與等待狀態</li>
      <li><strong>存檔執行緒 B</strong><br>自己的 TID、暫存器、使用者堆疊、核心堆疊與等待狀態</li>
    </ul>
  </div>
</figure>

## OS 負責什麼，硬體負責什麼

**排程器（scheduler）** 是核心中負責挑選下一條執行緒的程式。核心用 **TCB（Thread Control Block，執行緒控制區塊）** 保存每條執行緒的身分、狀態及恢復執行所需的資訊。暫停時把執行狀態存起來，換另一條執行緒繼續跑，稱為**上下文切換（context switch）**。核心把等待 CPU 的執行緒放在**就緒佇列（ready queue）**，選中其中一條並交給 CPU 執行的動作稱為**派發（dispatch）**。

**頁表（page table）**記錄虛擬位址如何對應到實體記憶體，以及每頁的讀、寫、執行權限。**MMU（Memory Management Unit，記憶體管理單元）**是執行這些檢查和轉譯的硬體。計時器則能在時間片到期時發出中斷，讓核心取得控制權，再由排程器決定誰接著執行。[實體頁與 Sv39](./memory.md)

<figure class="thread-figure">
  <figcaption>從建立執行緒 B 到 CPU 執行 B</figcaption>
  <ol class="thread-sequence">
    <li><strong>建立</strong>　程式呼叫 <code>thread_create()</code>，核心準備 B 的 TCB 和堆疊，把 B 放入就緒佇列。</li>
    <li><strong>派發</strong>　排程器取出隊首的 B，啟用它所屬行程的頁表，恢復 B 的執行狀態。</li>
    <li><strong>執行</strong>　CPU 依恢復的 PC 取指。指令需要讀寫記憶體時，由 MMU 依頁表做位址轉譯和權限檢查。</li>
  </ol>
</figure>

## 軟體 Thread、hart、core 與 SMT

| 名詞 | 用途 |
| --- | --- |
| **軟體執行緒（software thread）** | 讓程式的一條執行路徑可以獨立暫停、等待與恢復。WUWAOS 核心以 TCB 管理它。 |
| **hart（hardware thread）** | RISC-V 執行環境提供的硬體執行位置，可獨立推進一條指令流。WUWAOS 在 QEMU 以 `-smp 1` 啟動，客體只看到一個 hart，編號為 0。 |
| **CPU 核心（core）** | 實際取指、解碼、運算的硬體單位。一個實體核心可提供一個或多個硬體執行位置。 |
| **SMT（Simultaneous Multithreading，同時多執行緒）** | 讓同一實體核心同時支援多條硬體指令流。每條有各自的架構暫存器狀態，共用部分執行單元與快取。Intel Hyper-Threading 是一個例子。 |

<figure class="thread-figure">
  <figcaption>同一個時間點，各種配置能執行幾條指令流</figcaption>
  <div class="thread-table-scroll" tabindex="0" role="region" aria-label="單 hart、雙核心與 SMT 配置比較">
    <table>
      <thead><tr><th scope="col">配置</th><th scope="col">執行緒 A</th><th scope="col">執行緒 B</th></tr></thead>
      <tbody>
        <tr><th scope="row">WUWAOS 單 hart</th><td>在 hart 0 執行</td><td>等 hart 0 空出來</td></tr>
        <tr><th scope="row">雙核心，各有一個 hart</th><td>在核心 0 執行</td><td>同時在核心 1 執行</td></tr>
        <tr><th scope="row">單核心，兩個 SMT 執行位置</th><td>在核心 0 的位置 0 執行</td><td>同時在核心 0 的位置 1 執行</td></tr>
      </tbody>
    </table>
  </div>
</figure>

QEMU 的 hart 數是客體看到的執行資源。WUWAOS 要使用多個 hart，核心還需要為每個 hart 管理目前執行緒及排程器堆疊，並處理跨 hart 的鎖、喚醒和位址轉譯快取更新。[RISC-V ISA：Execution Environments and Harts](https://docs.riscv.org/reference/isa/v20260120/unpriv/intro.html) · [Intel：Hyper-Threading](https://edc.intel.com/content/www/us/en/design/ipla/software-development-platforms/client/platforms/alder-lake-desktop/12th-generation-intel-core-processors-datasheet-volume-1-of-2/001/intel-hyper-threading-technology/)

## 單 hart 為什麼仍然有價值

**併發（concurrency）**讓多項工作在同一段時間內都能前進。單 hart 靠交替執行做到這件事。**平行（parallelism）**指多條指令流能在同一時刻執行，需要多個硬體執行位置。增加執行緒能讓等待裝置的時間留給其他工作，但單 hart 的純運算工作仍得輪流使用 CPU。

<figure class="thread-figure">
  <figcaption>四個時刻的單 hart 排程示意</figcaption>
  <div class="thread-table-scroll" tabindex="0" role="region" aria-label="磁碟等待時序表">
    <table>
      <thead><tr><th scope="col">觀察對象</th><th scope="col">A 發出讀取</th><th scope="col">磁碟處理中</th><th scope="col">磁碟完成</th><th scope="col">再次派發 A</th></tr></thead>
      <tbody>
        <tr><th scope="row">hart 0 執行誰</th><td>A</td><td>B</td><td>B</td><td>A</td></tr>
        <tr><th scope="row">A 的狀態</th><td>執行中</td><td>等待中</td><td>就緒</td><td>執行中</td></tr>
        <tr><th scope="row">B 的狀態</th><td>就緒</td><td>執行中</td><td>執行中</td><td>就緒</td></tr>
      </tbody>
    </table>
  </div>
</figure>

磁碟完成後，A 已經具備執行條件。這時 B 仍在使用 CPU，A 要等下一次派發才會繼續讀取。

## 一條 Thread 如何等待、恢復與退出

WUWAOS 用**輪轉排程（round robin）**從就緒佇列的隊首挑選執行緒。每次派發會給一段**時間片（time slice）**，也就是計時器允許這條執行緒連續使用 CPU 的時間。執行緒用完時間片或呼叫 `yield()` 主動讓出 CPU 時，會排到隊尾。若它正在等讀取等 **I/O（input/output，輸入輸出）**，核心先把它移出可執行的名單，等事件結束再排回去。

| 狀態 | 核心用它記錄什麼 |
| --- | --- |
| Unused（空閒） | 這格 TCB 可供建立新的執行緒。 |
| New（建立中） | 核心已保留 TCB，正在準備堆疊與初始執行狀態。 |
| Ready（就緒） | 執行緒具備執行條件，在佇列中等待 CPU。 |
| Running（執行中） | 排程器已選中這條執行緒，由 hart 0 執行。 |
| Waiting（等待中） | 執行緒正在等裝置、同步物件、子行程或另一條執行緒。完成、逾時和取消都可能把它喚醒，恢復後再處理等待結果。 |
| Zombie（已退出、待回收） | 執行緒已停止，核心仍保留退出值和 TCB，供 `thread_join()` 或行程清理時回收。 |

下圖以 A、B、C 代稱執行緒，可切換時間片輪轉、I/O 等待和退出三種情境。每次派發的時間片是 50 ms；計時器只會在使用者模式下**搶占**，也就是中斷目前執行緒並讓核心重新排程。核心程式本身會在指定的等待點讓出 CPU，因此 50 ms 不構成回應時間上限。

<ThreadFlow />

### 用 cat 對照真實追蹤

在 WUWAOS shell 執行 `cat code.txt` 時，shell 以 `fork()` 建立子行程與其初始執行緒，再由子行程執行 shell 內建的 `cat` 函式。讀取檔案內容或檔案系統的管理資料時，`cat` 可能多次等待磁碟。如果同一組 PID 和 TID 反覆從 Running 進入 Waiting，再回到 Ready 和 Running，那就是同一條執行緒多次等待、恢復。一般事件訊息只能確認狀態變化，辨認具體的等待來源需要更詳細的追蹤。[Shell 的 cat 與子行程實作](https://github.com/johnson931128/WUWAOS/blob/main/user/shell/shell_exec.c)

`trace off` 會關閉正常的排程與行程追蹤訊息，保留程式輸出和錯誤。使用 `trace on` 可恢復追蹤。設定適用於整個系統，重開機後回到預設值。

## 一次 RISC-V 切換要保存什麼

**Trap** 是 CPU 因中斷、例外或系統呼叫而把控制權交給核心的機制。在 WUWAOS，應用程式跑在 **U-mode（使用者模式）**，核心跑在權限較高的 **S-mode（監督者模式）**。例如 `read()` 透過 `ecall` 指令請求核心服務。進入 S-mode 時，硬體會設定部分控制暫存器，包括保存返回位置的 `sepc`、記錄進入原因的 `scause`、視原因補充錯誤位址等資訊的 `stval`，以及保存特權與中斷相關狀態的 `sstatus`。

<figure class="thread-figure">
  <figcaption>從 A 的使用者指令切換到 B 的使用者指令</figcaption>
  <ol class="thread-sequence">
    <li><strong>進入核心</strong>　<code>trap.S</code> 把 A 的使用者暫存器和 trap 狀態存進 <code>TrapFrame</code>，讓核心稍後能返回使用者程式。</li>
    <li><strong>暫停核心函式</strong>　若 A 要等 I/O 或被搶占，<code>swtch.S</code> 把 A 在核心函式中的返回位址、堆疊指標與須保留的暫存器存進 <code>KernelContext</code>，換到排程器的堆疊。</li>
    <li><strong>恢復 B</strong>　排程器選到 B 後，先還原 B 的 <code>KernelContext</code>，接著從 B 的 <code>TrapFrame</code> 還原使用者狀態，以 <code>sret</code> 返回 U-mode。</li>
  </ol>
</figure>

| 資料 | WUWAOS 保存位置與用途 |
| --- | --- |
| 使用者暫存器與 trap 狀態 | `TrapFrame`（288 bytes），存放在執行緒的核心堆疊上，由 TCB 的 `frame` 指標指向。 |
| 核心函式的返回位置 | TCB 內的 `KernelContext`（112 bytes），保存 `ra`、`sp` 和 `s0` 到 `s11`。 |
| 核心函式的區域資料 | 每條執行緒的 16 KiB 核心堆疊，讓阻塞的讀取能從原函式繼續。 |
| 虛擬位址映射 | `Process.page_table`，由同一行程的執行緒共用。核心用 `satp` 指定目前生效的頁表根。 |

暫停的執行緒把狀態留在記憶體，CPU 的暫存器則給目前執行的指令流使用。WUWAOS 切換頁表時也執行 `sfence.vma`，讓後續位址轉譯使用正確的映射。**TLB** 是硬體用來加速位址轉譯的快取。[RISC-V Supervisor ISA](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html)

程式和編譯器之間的二進位介面規則稱為 **ABI（Application Binary Interface，應用程式二進位介面）**。例如 RISC-V LP64 的 `long` 與指標都是 64 位元，函式呼叫時堆疊要維持 16-byte 對齊，`tp` 則保留給執行緒指標。**TLS（Thread-Local Storage，執行緒區域儲存）**會讓每條執行緒各有一份同名變數。WUWAOS 目前保存整數暫存器，尚未配置 TLS。[RISC-V psABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/)

## 共用記憶體如何維持生命週期

同一行程的兩條執行緒可以直接讀寫同一份資料。即使只有一個 hart，切換也可能發生在讀取和寫回之間。以計數器從 0 開始為例：

<figure class="thread-figure">
  <figcaption>遺失更新：兩次加一，最後只留下 1</figcaption>
  <div class="thread-table-scroll" tabindex="0" role="region" aria-label="計數器的讀寫順序">
    <table>
      <thead><tr><th scope="col">順序</th><th scope="col">執行緒 A</th><th scope="col">執行緒 B</th><th scope="col">共用值</th></tr></thead>
      <tbody>
        <tr><th scope="row">1</th><td>讀到 0，算出 1</td><td></td><td>0</td></tr>
        <tr><th scope="row">2</th><td></td><td>讀到 0，算出 1</td><td>0</td></tr>
        <tr><th scope="row">3</th><td>寫入 1</td><td></td><td>1</td></tr>
        <tr><th scope="row">4</th><td></td><td>寫入 1</td><td>1</td></tr>
      </tbody>
    </table>
  </div>
</figure>

這是讀取、加一、寫回的交錯示意。C 程式可以用 **mutex（互斥鎖）**，讓同一時間只有一條執行緒能進入受保護的更新區段，或使用**原子操作**把加一作為一個不可分割的更新。

同樣的問題也出現在核心。WUWAOS 的 **KernelGate** 用來限制同一時間哪些執行緒可以操作共用核心資源，例如檔案系統。假設 A 持有檔案系統的 gate 並等待磁碟，B 也要操作檔案系統。如果 gate 只看 PID，可能把 B 認成 A 再次進入。用 TID 辨認持有者，才能讓 B 等到 A 做完再進入。

- **讀取期間關閉 FD。** 核心讓已開始的 `read()` 暫時保有原 **OFD（Open File Description，開啟檔案描述）**。OFD 記錄開啟的檔案與目前讀寫位置。即使另一條執行緒關閉 FD 又把同一個編號用於新檔案，進行中的讀取仍指向原檔案。`fork()` 或 `dup()` 也可能讓不同 FD 指向同一個 OFD。
- **操作期間變更工作目錄。** 核心在處理相對路徑時先取得 **cwd（current working directory，目前工作目錄）** 的快照，讓該次操作在等待後仍以同一個目錄為起點。
- **退出後回收。** `thread_exit()` 結束單一執行緒，行程資源仍可供其他執行緒使用。行程的 `exit()` 會通知其餘執行緒退出，等它們完成必要的裝置與檔案系統清理、離開各自的核心堆疊後，再銷毀共用資源。

## WUWAOS 目前提供的 API 與限制

| 介面 | 何時使用 |
| --- | --- |
| `thread_create(entry, arg)` | 建立一條執行緒，以 `arg` 呼叫入口函式 `entry`，成功時回傳新 TID。 |
| `thread_join(tid, status)` | 等待同一行程內指定的執行緒結束，把退出值寫到 `status` 指向的變數，並回收該執行緒。成功時回傳目標 TID。 |
| `thread_exit(status)` | 結束呼叫者並保存退出值。入口函式直接返回時，啟動用的組合語言程式也會代為呼叫它。 |
| `gettid()` | 取得目前執行緒的 TID，供追蹤、識別或管理執行緒使用。 |

以下片段可放進使用 WUWAOS 使用者函式庫的程式。主執行緒建立 `worker`，再等它寫入資料並退出。成功回收後，`shared_result` 為 42，`status` 為 7。

```c
#include "user/lib/api.h"

static long shared_result;

static long worker(void *argument) {
    long *result = argument;
    *result = 42;
    return 7;
}

void thread_example(void) {
    long status = 0;
    long tid = thread_create(worker, &shared_result);
    if (tid < 0) return;
    if (thread_join(tid, &status) != tid) return;
    /* shared_result == 42, status == 7 */
}
```

`shared_result` 具有靜態儲存期，因此在 `worker` 執行期間持續有效。這個例子先 `join` 再讀結果；如果兩條執行緒需要在執行中同時修改共用值，還需要互斥鎖或原子操作。

- 系統最多有 16 個行程和 32 條執行緒。單一行程最多占用 8 格 TCB，尚未回收的 Zombie 也占一格。
- 初始執行緒使用原本的一頁使用者堆疊。額外執行緒使用 32 KiB 的使用者堆疊，旁邊留一頁 **guard page（防護頁）**，讓越界碰到未映射頁時觸發例外。
- `fork()` 用來建立子行程，`exec()` 用來替換目前行程執行的程式。行程占用超過一格 TCB 時，兩者都會回傳 `ERR_BUSY`。目前 `ps` 和 `pcb` 主要顯示行程及其代表執行緒，專用執行緒清單和 `demo threads` 尚未實作。

階段 B／C 的 API 與共用資源清理已有程式碼，完整多執行緒 QEMU 執行驗收尚待完成。[執行緒規格](../SPEC-34-threads.md) · [階段 B／C 實作紀錄](../thread-stage-b.md)

## 資料來源與實作對照

- [TCB 與執行緒狀態](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/thread.h) · [就緒佇列](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/scheduler.c) · [排程主迴圈與生命週期](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c)
- [Trap 入口](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/trap.S) · [核心上下文切換](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/swtch.S) · [計時器](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/timer.c)
- [KernelGate](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/gate.c) · [同步物件](https://github.com/johnson931128/WUWAOS/blob/main/kernel/ipc/sync.c) · [虛擬記憶體](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c)

<style scoped>
.thread-figure {
  margin: 24px 0;
  padding: 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  background: var(--vp-c-bg-soft);
}

.thread-figure figcaption {
  margin-bottom: 12px;
  font-weight: 600;
}

.thread-process {
  padding: 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
}

.thread-process p,
.thread-figure .thread-owners,
.thread-figure .thread-sequence,
.thread-figure table {
  margin: 0;
}

.thread-figure .thread-owners {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 185px), 1fr));
  gap: 10px;
  margin-top: 12px;
  padding: 0;
  list-style: none;
}

.thread-owners li {
  margin: 0;
  padding: 12px;
  border-radius: 8px;
  background: var(--vp-c-bg);
}

.thread-sequence li + li {
  margin-top: 10px;
}

.thread-table-scroll {
  overflow-x: auto;
}

.thread-table-scroll:focus-visible {
  outline: 2px solid var(--vp-c-brand-1);
  outline-offset: 3px;
}

.thread-figure table {
  display: table;
  width: 100%;
}

.thread-figure :is(th, td) {
  min-width: 72px;
}
</style>
