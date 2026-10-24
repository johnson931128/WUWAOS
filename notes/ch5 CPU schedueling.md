---
title: ch5 CPU schedueling

---

## CPU Scheduling 核心動機與 Burst 週期

#### 1. 系統設計的終極目標：極大化 CPU 榨取率 (Maximize CPU Utilization)
* **單工作業系統的悲歌 (Simple computer system)**：在早期的系統中，當一個 Process 執行到一半，需要向硬碟請求資料 (I/O request) 時，它必須停下來死等。這段期間，極度昂貴的 CPU 就只能處於閒置狀態 (sits idle)，這是算力的巨大浪費。
* **Multiprogramming (多重程式設計) 的解法**：為了不讓 CPU 閒下來，作業系統會在記憶體中同時載入多個 Processes。
    * **核心排程邏輯**：當 Process A 因為等待 I/O 而卡住時，作業系統會**立刻、毫不留情地將 CPU 的控制權從 Process A 手中奪走 (takes the CPU away)**，轉交給已經準備好運算的 Process B。
    * 透過這種快速切換的模式，確保在任何時刻，CPU 都在全速運作。

#### 2. 程式執行的物理節奏：CPU–I/O Burst Cycle (圖 5.1)
![image](https://hackmd.io/_uploads/H1elZ912Wg.png)
作業系統能夠進行上述的排程切換，是因為它看透了所有應用程式的行為本質。任何一支程式在執行時，必然是在以下兩種狀態之間不斷交替 (Alternating sequence)：

1. **CPU Burst (運算爆發期)**：
   * 程式正在瘋狂使用 CPU 進行加減乘除、邏輯判斷或讀取記憶體 (例如圖中的 `load`, `add`, `store`)。
   * 這段期間，程式需要獨佔 CPU 算力。
2. **I/O Burst (輸入/輸出爆發期)**：
   * 程式運算到一個段落，必須向外部設備請求資料 (例如圖中的 `read from file`, `wait for I/O`)。
   * 這段期間，程式完全**不需要** CPU 算力，它只是一直在等待硬體回應。

**【架構師觀點：排程器的介入時機】**
了解這個 Burst Cycle 是設計作業系統排程演算法的基礎。一個完美的 OS 排程器 (Scheduler)，就是要能精準捕捉到每一個程式從「CPU Burst」過渡到「I/O Burst」的那個瞬間，並在它開始發呆等待 I/O 之前，立刻把 CPU 搶走配給別人。

---


## Burst 週期分佈與 CPU 排程器本質

#### 1. 深度解析 5.1.1：CPU 爆發時間的統計學特性 (Figure 5.2)
![image](https://hackmd.io/_uploads/SJFG-cJhWg.png)
雖然所有程式都是在 CPU Burst 與 I/O Burst 之間交替，但不同的程式，其 Burst 的「時間長度」有著天壤之別。課本中 Figure 5.2 的直方圖 (Histogram) 揭示了一個極度重要的作業系統統計規律：**呈指數型分佈 (Exponential distribution)**。

* **大宗現象 (Large number of short CPU bursts)**：
    圖表左側的高峰顯示，系統中絕大多數的 CPU Burst 時間都**極度短暫**。這類程式稍微用一下 CPU（例如將變數加一、比對一個字串），就立刻跑去等待 I/O（如等待鍵盤輸入、等待網路封包）。
    👉 **術語定義**：這被稱為 **I/O-bound program (I/O 密集型程式)**。例如：文書處理軟體、網頁瀏覽器。
* **極端少數 (Small number of long CPU bursts)**：
    圖表右側的長尾顯示，只有極少數的程式會長時間霸佔 CPU 進行瘋狂運算，期間完全不發出 I/O 請求。
    👉 **術語定義**：這被稱為 **CPU-bound program (CPU 密集型程式)**。例如：影片轉檔軟體、機器學習模型訓練、ROS 2 內部的 VSLAM 矩陣解析。

**【架構師觀點】**
這張圖表是所有 CPU 排程演算法設計的根基。若系統中充滿了 I/O-bound 程式，排程器必須極度敏銳，頻繁地進行 Context Switch；若面對 CPU-bound 程式，排程器則需要考慮如何公平地強行打斷它，避免其他程式被餓死。

#### 2. 破除迷思 5.1.2：CPU Scheduler (排程器) 與 Ready Queue
這個段落之所以生硬，是因為它在打破一般人對「佇列 (Queue)」的刻板印象。

當 CPU 閒置時，作業系統必須挑選下一個程式上機執行。
* **負責挑選的元件**：稱為 **CPU Scheduler (CPU 排程器)**。
* **挑選的候選區**：稱為 **Ready Queue (就緒佇列)**。裡面存放的是所有已經準備好可以運算、只差 CPU 資源的 **PCB (Process Control Blocks)**。

**🎯 核心破除迷思：Ready Queue 絕對不是單純的排隊隊伍**
* 一般認知的 Queue 是 FIFO (先進先出)，就像在超市結帳，先排隊的先結帳。
* 但課本強調，實務上的 Ready Queue **不一定**是 FIFO。為了達到系統效能最佳化，這個資料結構可能會被實作成：
    1.  **Priority Queue (優先權佇列)**：VIP 程式（如硬體中斷處理）可以插隊。
    2.  **Tree (樹狀結構)**：例如現代 Linux 預設的 CFS (完全公平排程器)，底層就是使用 Red-Black Tree (紅黑樹) 來快速搜尋出目前佔用 CPU 時間最少的程式。
    3.  **Unordered linked list (無序鏈結串列)**：單純把所有 PCB 串起來，每次都線性掃描挑選。

**【具象化比喻】**
可以把 Ready Queue 想像成醫院的「候診室」，而 PCB 就是病人的「病歷表」。CPU Scheduler 就是叫號護理師。護理師叫號時，不一定會按照病人掛號的先後順序 (FIFO)，而是可能根據病情的嚴重程度 (Priority Queue)，或是看誰的看診時間預估最短 (這將是後續 SJF 演算法的重點) 來動態決定下一個看診的人。

---

## Preemptive vs. Nonpreemptive Scheduling


#### 1. 觸發排程的四大關鍵時機 (Circumstances)
作業系統 (CPU Scheduler) 會在以下四種特定時機點介入，決定 CPU 接下來要交給誰：

1. **Running $\rightarrow$ Waiting**：程式執行到一半，主動發出 I/O 請求 (如讀取檔案)，或是呼叫 `wait()` 等待子行程結束。此時程式無法繼續運算。
2. **Running $\rightarrow$ Ready**：程式明明還能繼續算，但被作業系統**強制中斷 (Interrupt)**。最經典的就是投影片上手寫的 **Timer (計時器中斷)**，也就是它的時間配額 (Time Slice) 用完了。
3. **Waiting $\rightarrow$ Ready**：程式等待的 I/O 設備終於處理完畢 (I/O completion)。此時程式從等待區甦醒，被放回 Ready Queue。如果這個剛甦醒的程式優先權極高，排程器可能會立刻介入。
4. **Terminates**：程式執行完畢，自然死亡 (Exit)。

#### 2. 兩大排程流派的嚴格定義
根據作業系統是否具有「強盜邏輯」，排程機制被嚴格劃分為兩類：

* **Nonpreemptive (非搶占式 / Cooperative 合作式)**：
    * **定義**：排程**只會**在情境 1 和 4 發生。
    * **邏輯**：作業系統非常佛心。一旦把 CPU 分配給某個 Process，除非該 Process **自己不想跑了** (主動去等 I/O) 或是**壽終正寢**，否則作業系統絕對不會把 CPU 搶回來。
    * **缺點**：如果有一個惡意程式寫了個無窮迴圈 `while(true){}` 且不呼叫任何 I/O，整個作業系統就會直接死機，因為其他程式永遠拿不到 CPU。
* **Preemptive (搶占式)**：
    * **定義**：排程包含情境 2 和 3 (投影片手寫註記：**強制的**)。
    * **邏輯**：作業系統握有絕對生殺大權。時間到了 (Timer) 或是遇到緊急狀況，OS 隨時可以拔掉當前 Process 的 CPU 控制權。
    * **現代標準**：目前幾乎所有的現代作業系統 (Windows, macOS, Linux, UNIX) 皆全面採用搶占式演算法。

#### 3. 系統工程師的夢魘：Preemption 帶來的兩大挑戰
搶占式排程雖然解決了系統卡死的問題，但也帶來了極度複雜的同步 (Synchronization) 災難：

* **User Space 的資料競爭 (Race Condition)**：
    誠如 Chapter 4 所討論的，如果兩個 Processes (或 Threads) 共享同一塊記憶體資料，當 Process A 修改資料改到一半時，突然被 OS 搶占 (Preempted) 換成 Process B 上場，Process B 就會讀取到「改壞的、不一致 (inconsistent)」的半殘資料。這必須依賴 Chapter 6 的 Mutex Locks (互斥鎖) 來解決。
* **Kernel Space 的崩潰危機 (Preemptive Kernel vs. Nonpreemptive Kernel)**：
    當程式發出 System Call，讓 CPU 進入 Kernel Mode 幫它修改底層的 I/O Queue 結構時，**如果在這瞬間發生中斷，可以搶占它嗎？**
    * **Nonpreemptive Kernel (非搶占式核心)**：早期的 OS 設計。只要 CPU 處於 Kernel Mode，就享有「絕對免疫權」，不允許被搶占。優點是核心資料絕對安全 (Simple structure)；缺點是對 Real-time (即時) 任務的反應極度遲鈍，因為緊急任務必須苦等核心工作做完。
    * **Preemptive Kernel (搶占式核心)**：現代作業系統的標配。即使 CPU 正在 Kernel Mode 執行底層邏輯，只要有更高優先權的任務出現，照樣瞬間搶占。這要求 OS 核心的原始碼必須寫得極度嚴謹，到處佈滿防護鎖 (Mutex locks) 以防止核心資料結構崩潰 (Chaos ensues)。

---

## Dispatcher (分派器) 與 Context Switch 的代價

#### 1. Dispatcher 的三大核心任務
![image](https://hackmd.io/_uploads/H1qT-ckh-l.png)
當 CPU Scheduler 從 Ready Queue 中挑出了一個幸運兒 (Process B) 之後，Dispatcher 就會介入並執行以下三個極度底層的硬體動作，將 CPU 的控制權正式移交：

1. **Switching context (切換上下文)**：
    * 這是最耗時的一步 (參考 Figure 5.3)。
    * 必須先將當下正在執行的 Process A 的狀態 (包含所有的 CPU 暫存器、Program Counter 指標) 完整地 `save` 存進 $PCB_A$ 裡。
    * 接著，從記憶體中把 Process B 的 $PCB_B$ 抓出來，並將裡面的狀態 `restore` 寫回實體的 CPU 暫存器中。
2. **Switching to user mode (切換回使用者模式)**：
    * 剛剛在做 Context Switch 時，CPU 必定處於權限最高的 Kernel Mode。現在要讓使用者的程式 (Process B) 繼續跑，必須將 CPU 權限降級回 User Mode，以防它亂搞系統硬體。
3. **Jumping to the proper location (跳轉到正確位址)**：
    * 根據剛才從 $PCB_B$ 恢復的 Program Counter (程式計數器) 數值，將 CPU 的執行點精準地跳回到 Process B 上次被中斷的那一行程式碼，讓它無縫接軌繼續執行 (resume)。

#### 2. 效能指標：Dispatch Latency (分派延遲)
* **定義**：從「停止 Process A」到「成功啟動 Process B」這中間所耗費的時間。這是一段純粹的系統損耗 (Overhead)，期間 CPU 完全沒有在執行任何使用者的業務邏輯。
* **架構師鐵則**：Dispatcher 必須寫得**極度神速 (as fast as possible)**。在現代 Linux 系統中，每秒可能發生上千次的 Context Switch，如果 Dispatch Latency 太高，整個系統的效能會被這成千上萬次的「切換成本」直接拖垮。

#### 3. 實務工具驗證：Linux 的切換觀察
這段落非常實用地提供了 Linux 指令，讓工程師能親眼見證 Context Switch 發生的頻率：

* **系統巨觀監控 (`vmstat`)**：
    * 輸入 `vmstat 1 3`，可以觀察全系統每秒發生的 Context Switch 總數。在高度併發的伺服器上，這個數字經常破萬。
* **單一行程微觀分析 (`cat /proc/[PID]/status`)**：
    * 透過讀取特定 Process 的狀態檔，可以看到兩種截然不同的切換次數：
    * **`voluntary_ctxt_switches` (自願切換)**：呼應前面學過的 **Nonpreemptive** 情境。程式自己呼叫了 I/O 請求 (如讀檔) 被卡住，主動把 CPU 讓出來。
    * **`nonvoluntary_ctxt_switches` (非自願/強制切換)**：呼應 **Preemptive** 情境。程式算得正開心，結果被作業系統判定 Time Slice 用完 (Timer Interrupt) 或是被更高優先權的程式插隊，硬生生把 CPU 搶走。

---

## Scheduling Criteria (系統效能的五大指標)

#### 1. 系統資源壓榨指標 (Maximize)
這兩個指標的目標是**越高越好 (Maximize)**，用來衡量作業系統是否有在認真工作：
* **CPU Utilization (CPU 使用率)**：
    * **定義**：CPU 處於非閒置狀態的時間比例。
    * **實務數據**：理論上希望達到 100%，但在真實的系統中，輕度負載約落在 40%，重度負載則期望達到 90%。
* **Throughput (吞吐量)**：
    * **定義**：單位時間內「徹底執行完畢」的 Process 數量。
    * **衡量標準**：對於短任務（如 Web API 請求），吞吐量可能是每秒數十個；對於長任務（如編譯大型 C++ 專案），可能是每小時幾個。

#### 2. 時間延遲指標 (Minimize)
這三個指標的目標是**越低越好 (Minimize)**，直接關乎使用者或上層應用的體感：
* **==Turnaround Time (周轉/完成時間) = 完成時間 - 抵達時間==**：
    * **定義**：從程式「提交給系統」那一刻起，直到「徹底執行結束」所花費的總時間。
    * **數學拆解**：$T_{turnaround} = T_{ready\_queue等待} + T_{CPU運算} + T_{I/O等待}$。
* **==Waiting Time (等待時間) = 週轉時間 - 實際 CPU 運算時間 (Burst)==**：
    * **定義**：程式在 Ready Queue 裡面「純排隊罰站」的時間總和。
    * **【核心觀念】**：作業系統的排程演算法**無法**改變程式需要運算多久 ($T_{CPU運算}$)，也**無法**改變硬體 I/O 的速度 ($T_{I/O等待}$)。因此，**排程演算法唯一能影響且必須負責的，就是 Waiting Time**。這是後續評估演算法最核心的依據。
* **==Response Time (回應時間) = 首次獲得 CPU 的時間 - 抵達時間==**：
    * **定義**：從「提交請求」到系統「給出第一次輸出/回應」的時間，**不包含**輸出所有結果的時間。
    * **實務應用**：對於互動式系統（如終端機輸入指令、ROS 2 Rviz2 的畫面更新）極度重要。使用者需要立刻看到系統有在動，而不是乾等所有運算結束。

#### 3. 系統工程師的取捨 (Trade-offs) 與課本簡化假設
* **平均值 vs. 變異數 (Average vs. Variance)**：
    * 多數情況下，演算法致力於最佳化「平均指標」(例如縮短平均等待時間)。
    * 但在**互動式系統**中，**預測性 (Predictable)** 比絕對速度更重要。如果系統的回應時間忽快忽慢（變異數極大），使用者的焦慮感會遠大於一個「平均稍慢但穩定」的系統。因此，有時需要犧牲一點平均效能，來最小化最大回應時間或變異數。
* **後續章節的簡化模型**：
    * 真實的程式是由數百個 CPU Burst 與 I/O Burst 交替組成。為了專注於排程演算法的邏輯比對，課本在接下來的 5.3 節探討各類演算法時，將做出極端簡化：**假設每個 Process 只有「唯一一個」CPU Burst，且勝負的唯一評分標準是「平均等待時間 (Average Waiting Time)」**。

---

## FCFS 理論觀念與數學剖析

#### 1. FCFS (First-Come, First-Served) 核心定義
* **本質**：作業系統中最原始、最簡單的排程演算法。先要求 CPU 資源的 Process，就先獲得 CPU 控制權。
* **底層資料結構**：使用標準的 **FIFO Queue (先進先出佇列)** 來實作 Ready Queue。當 Process 進入 Ready 狀態，其 PCB 就會被掛到 Queue 的尾端 (Tail)；CPU 閒置時，Dispatcher 永遠從 Queue 的首端 (Head) 拔出 PCB 來執行。
* **排程流派**：FCFS 屬於絕對的 **Nonpreemptive (非搶占式)**。一旦 Process 拿到 CPU，除非它自己執行完畢或主動發出 I/O 請求，否則就算它霸佔 CPU 幾個小時，作業系統也絕對不會強制中斷它。

#### 2. 數學推導：平均等待時間的高波動性 (Volatility)
![image](https://hackmd.io/_uploads/BJyOMqy2-x.png)
![image](https://hackmd.io/_uploads/rkutG5k2Zg.png)
![image](https://hackmd.io/_uploads/r1pKMckh-e.png)

課本透過兩組甘特圖 (Gantt Chart) 進行了極端的對比數學證明，突顯 FCFS 的效能完全靠運氣 (取決於抵達順序)：

**假設前提**：三個 Processes ($P_1=24$ms, $P_2=3$ms, $P_3=3$ms) 在時間點 0 幾乎同時抵達。

* **災難劇本 (CPU-bound 巨獸排第一)**：抵達順序為 $P_1, P_2, P_3$
    * $P_1$ 等待時間：$0$ ms
    * $P_2$ 等待時間：$24$ ms (必須等 $P_1$ 算完)
    * $P_3$ 等待時間：$27$ ms ($24 + 3$)
    * **平均等待時間 (Average Waiting Time)**：$\frac{0 + 24 + 27}{3} = 17$ ms。
* **最佳劇本 (短任務優先)**：抵達順序為 $P_2, P_3, P_1$
    * $P_2$ 等待時間：$0$ ms
    * $P_3$ 等待時間：$3$ ms
    * $P_1$ 等待時間：$6$ ms
    * **平均等待時間 (Average Waiting Time)**：$\frac{0 + 3 + 6}{3} = 3$ ms。

**數學結論**：僅僅是抵達順序的不同，平均等待時間就能從 17ms 暴跌到 3ms。這證明了 FCFS 在處理運算時間長短不一的任務時，效能極度不穩定，且通常無法達到最佳化。

#### 3. 系統架構災難：Convoy Effect (護衛效應)
這是 FCFS 在混合負載系統中最嚴重的架構性缺陷。

* **情境設定**：系統中有 1 個需要大量運算的 CPU-bound 程式 (大老闆)，以及許多只需短暫運算就必須讀寫檔案的 I/O-bound 程式 (小跟班)。
* **骨牌效應爆發**：
    1. 大老闆取得 CPU 開始瘋狂運算。
    2. 所有小跟班的 I/O 很快就做完了，全部擠進 Ready Queue 苦苦等待大老闆交出 CPU。**此時，硬碟與網路等 I/O 設備處於完全閒置 (Idle) 狀態，資源浪費。**
    3. 大老闆終於算完，跑去等 I/O。
    4. 擠在 Ready Queue 的小跟班們瞬間獲得 CPU，因為它們是 I/O-bound，只算了一下子就又全部跑去等 I/O。**此時，CPU 瞬間被清空，陷入完全閒置 (Idle) 狀態，算力浪費。**
* **架構師總結**：這種「所有短 Process 都在苦等一個長 Process 離開 CPU」的現象，就如同車陣中所有輕型車輛都被一輛重型大卡車擋在後方，被稱為 **Convoy Effect (護衛效應)**。它會導致 CPU 使用率與 I/O 設備使用率雙雙慘跌。


#### 4. FCFS 演算法的 C 語言具體實作與底層考量
##### 1. C 語言程式碼實作
```=c
#include <stdio.h>
#include <stdlib.h>

/* 定義系統上限，避免動態記憶體配置的額外開銷 (符合嵌入式系統原則) */
#define MAX_PROCESSES 10

/* * 系統架構定義：Process Control Block (PCB) 的精簡版
 * 將所有與排程相關的屬性高內聚於此結構中。
 */
typedef struct {
    int pid;             // 行程識別碼
    int burst_time;      // 該行程所需的 CPU 運算時間
    int waiting_time;    // 等待時間
    int turnaround_time; // 周轉時間 (等待時間 + 運算時間)
} Process;

/*
 * 核心排程邏輯：計算所有 Process 的時間指標
 * 假設所有 Process 皆於時間點 0 依序抵達 Ready Queue。
 */
void calculate_fcfs_metrics(Process processes[], int n) {
    // 陣列邊界防呆檢查
    if (n <= 0) return;

    // 第一個抵達的 Process 絕對不需要等待
    processes[0].waiting_time = 0;
    processes[0].turnaround_time = processes[0].burst_time;

    // 從第二個 Process 開始計算 (依賴前一個 Process 的狀態)
    for (int i = 1; i < n; i++) {
        /*
         * 【底層邏輯】：目前 Process 的等待時間，
         * 等於「前一個 Process 的等待時間」加上「前一個 Process 霸佔 CPU 的時間」。
         * 這精準體現了 FCFS "Convoy Effect" (護衛效應) 的數學本質。
         */
        processes[i].waiting_time = processes[i-1].waiting_time + processes[i-1].burst_time;
        
        /* 周轉時間 = 自己純排隊的時間 + 自己真正執行的時間 */
        processes[i].turnaround_time = processes[i].waiting_time + processes[i].burst_time;
    }
}

/* 顯示結果與計算平均值的模組 */
void print_gantt_metrics(Process processes[], int n) {
    float total_waiting_time = 0.0;
    float total_turnaround_time = 0.0;

    printf("PID\tBurst Time\tWaiting Time\tTurnaround Time\n");
    printf("-------------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        total_waiting_time += processes[i].waiting_time;
        total_turnaround_time += processes[i].turnaround_time;
        printf("P%d\t%d\t\t%d\t\t%d\n", 
               processes[i].pid, 
               processes[i].burst_time, 
               processes[i].waiting_time, 
               processes[i].turnaround_time);
    }

    printf("-------------------------------------------------------\n");
    printf("平均等待時間 (Average Waiting Time): %.2f ms\n", total_waiting_time / n);
    printf("平均周轉時間 (Average Turnaround Time): %.2f ms\n", total_turnaround_time / n);
}

int main() {
    // 模擬課本 Figure 5.3 之前的災難劇本 (大老闆 P1 先抵達)
    int n = 3;
    Process ready_queue[MAX_PROCESSES] = {
        {1, 24, 0, 0}, // P1: Burst = 24
        {2, 3, 0, 0},  // P2: Burst = 3
        {3, 3, 0, 0}   // P3: Burst = 3
    };

    printf("[ 系統日誌 ] 啟動 FCFS 排程器演算法...\n");
    
    // 執行排程計算
    calculate_fcfs_metrics(ready_queue, n);
    
    // 輸出結果
    print_gantt_metrics(ready_queue, n);

    return 0;
}
```

##### 2. 底層設計考量與架構解析
* **結構化封裝 (Data Locality)：** 將 `pid`、`burst_time` 與計算結果全部封裝進 Process 結構體中。這不僅提升了程式碼的「高內聚性」，在硬體底層執行時，同一結構體的記憶體位址是連續的，能大幅提高 CPU Cache 的命中率 (Cache Hit Rate)。

* **依賴鏈設計 (Dependency Chain)：** 在 calculate_fcfs_metrics 的 for 迴圈中，可以看到 $W_i = W_{i-1} + Burst_{i-1}$ 的標準實作。這個迴圈是完全循序 (Sequential) 的，無法使用 OpenMP 進行平行化加速，因為每一個任務的啟動時間都被前一個任務「強制卡死」，這就是非搶占式 (Nonpreemptive) 演算法的實作代價。


## SJF 理論觀念與數學預測模型

#### 1. 核心定義與理論上的「絕對最佳解 (Optimal)」
* **精確命名**：雖然業界統稱 SJF，但課本指出更精確的學術名稱應為 **Shortest-Next-CPU-Burst**。因為排程器比較的不是整個程式的總長度，而是「下一次 CPU Burst 的時間長短」。
* **運作邏輯**：當 CPU 閒置時，排程器會掃描 Ready Queue，挑選出下一次 CPU Burst 預估時間最短的 Process。若遇到時間一樣短的，則退化為 FCFS 來打破僵局 (Break the tie)。
* **數學上的完美證明**：
    SJF 被數學證明為**最佳化演算法 (Provably optimal)**，它能給出所有演算法中最低的「平均等待時間 (Average Waiting Time)」。
    * **底層原因**：將一個短任務移到長任務前面，短任務減少的等待時間，會大於長任務增加的等待時間。這從根本上消滅了前一節 FCFS 所引發的「護衛效應 (Convoy Effect)」。

#### 2. 工程實務的殘酷現實：如何預測未來？
SJF 理論完美，但它在作業系統底層**無法被直接實作**。因為作業系統 (OS Kernel) 根本沒有通靈能力，無法預知一個剛從 I/O 甦醒的 Process 接下來會霸佔 CPU 幾毫秒。

為了解決這個盲區，系統工程師引入了機器學習與統計學中極度經典的數學模型：**Exponential Average (指數加權移動平均法, EMA)**。透過過去的歷史紀錄，來「猜測」下一次的長度。

![image](https://hackmd.io/_uploads/rkOZEqy3We.png)


* **核心數學公式**：
  $$\tau_{n+1} = \alpha t_n + (1 - \alpha)\tau_n$$
  * $t_n$：第 $n$ 次的**實際** CPU Burst 長度 (最新的現實情報)。
  * $\tau_n$：原本對第 $n$ 次的**預測值** (累積的歷史記憶)。
  * $\tau_{n+1}$：即將用來排程的**下一次預測值**。
  * $\alpha$：權重因子 ($0 \le \alpha \le 1$)，用來控制「最新現實」與「歷史記憶」的比例。

* **數學展開與衰減特性 (為何稱為指數？)**：
  若將公式展開，會得到：
  $$\tau_{n+1} = \alpha t_n + (1 - \alpha)\alpha t_{n-1} + \dots + (1 - \alpha)^j \alpha t_{n-j} + \dots + (1 - \alpha)^{n+1}\tau_0$$
  由於 $\alpha$ 和 $(1-\alpha)$ 都是小於 1 的分數，隨著資料越舊 (即 $j$ 越大)，$(1-\alpha)^j$ 的數值會呈**指數級別急速衰減**。這代表系統認為：**越久遠的歷史資料，對預測未來的參考價值越低**。實務上，多數系統預設設定 $\alpha = \frac{1}{2}$，給予近期與歷史相等的權重。

#### 3. 搶占式 SJF：SRTF (最短剩餘時間優先)
SJF 家族依據是否具備「搶占 (Preemption)」能力，分為兩大流派：

1. **Nonpreemptive SJF**：CPU 給了就給了，算到完為止。
2. **Preemptive SJF (又稱 SRTF, Shortest-Remaining-Time-First)**：
   * 這是現代系統的標準考量。當一個新的 Process 加入 Ready Queue 時，排程器會將「新 Process 的所需時間」與「當前運行中 Process 的**剩餘時間 (Remaining Time)**」進行比對。
   * **強制搶占 (Preempt)**：如果新來的任務更短，排程器會瞬間觸發 Context Switch，把當前的長任務踢下 CPU。

![image](https://hackmd.io/_uploads/rkmL7cy2Wg.png)

**SRTF 課本甘特圖推導驗證**：
考慮加入「抵達時間 (Arrival Time)」的複雜情境：
* $P_1(0ms抵達, 需8ms)$
* $P_2(1ms抵達, 需4ms)$
* $P_3(2ms抵達, 需9ms)$
* $P_4(3ms抵達, 需5ms)$

**微觀時間軸推演**：
* **T=0**：只有 $P_1$ 在佇列，開始執行。
* **T=1**：$P_2$ 抵達 (需 4ms)。此時 $P_1$ 剛算完 1ms，**剩餘 7ms**。因為 $4 < 7$，$P_1$ 遭到強制搶占 (Preempted)，換 $P_2$ 上陣。
* **T=2**：$P_3$ 抵達 (需 9ms)。此時 $P_2$ 剩餘 3ms。$3 < 9$，不搶占。
* **T=3**：$P_4$ 抵達 (需 5ms)。此時 $P_2$ 剩餘 2ms。$2 < 5$，不搶占。
* **T=5**：$P_2$ 執行完畢。比較剩下的 $(P_1:7, P_3:9, P_4:5)$，選擇最短的 $P_4$ 執行。
* 最終平均等待時間被壓縮至 $6.5$ ms，這是在有抵達時間差異的情況下，物理極限上的最優解。

#### 4. SRTF (搶占式最短剩餘時間優先) C++ 實作
為了捕捉「隨時可能被中斷」的物理特性，採用了離散時間步進 (Discrete Time Stepping) 的狀態機設計。程式碼中的 `current_time++` 迴圈，本質上是在模擬硬體 CPU 的 Timer Interrupt。每一次迴圈的起點，都相當於作業系統發動了一次中斷，強制喚醒 CPU Scheduler 重新評估當前的 Ready Queue 狀態。
```=c++
#include <iostream>
#include <vector>
#include <climits>
#include <iomanip>

/* * 系統架構定義：Process Control Block (PCB)
 * 封裝了單一 Process 的完整生命週期狀態。
 */
struct Process {
    int pid;             // 行程識別碼
    int arrival_time;    // 抵達 Ready Queue 的時間
    int burst_time;      // 原始所需的 CPU 總時間
    int remaining_time;  // 剩餘執行時間 (SRTF 搶占判斷的核心)
    int waiting_time;    // 等待時間
    int turnaround_time; // 周轉時間
    bool is_completed;   // 是否執行完畢
};

/* * 核心排程邏輯：SRTF 狀態機模擬
 */
void simulate_srtf(std::vector<Process>& processes) {
    int current_time = 0;
    int completed_count = 0;
    int n = processes.size();
    int prev_pid = -1; // 用於追蹤 Context Switch 發生時機

    std::cout << "[ 系統日誌 ] 開始 SRTF 搶占式排程模擬...\n";

    // 當還有 Process 尚未執行完畢時，持續推進時間軸
    while (completed_count != n) {
        int shortest_idx = -1;
        int min_remaining = INT_MAX;

        // 核心排程器 (CPU Scheduler)：掃描所有已抵達且未完成的 Process
        for (int i = 0; i < n; i++) {
            if (processes[i].arrival_time <= current_time && 
                !processes[i].is_completed) {
                
                // 尋找剩餘時間最短的目標
                if (processes[i].remaining_time < min_remaining) {
                    min_remaining = processes[i].remaining_time;
                    shortest_idx = i;
                }
                // 若剩餘時間相同 (Break the tie)，預設由陣列順序(FCFS)決定
            }
        }

        // 分派器 (Dispatcher) 介入
        if (shortest_idx != -1) {
            // 偵測到搶占 (Preemption) 或全新任務載入，印出 Context Switch 資訊
            if (prev_pid != processes[shortest_idx].pid) {
                std::cout << " -> [T=" << std::setw(2) << current_time 
                          << "] Dispatcher 載入 P" << processes[shortest_idx].pid << "\n";
                prev_pid = processes[shortest_idx].pid;
            }

            // 執行 1 個單位的 CPU Burst
            processes[shortest_idx].remaining_time--;
            current_time++;

            // 檢查是否達到終止狀態 (Terminates)
            if (processes[shortest_idx].remaining_time == 0) {
                processes[shortest_idx].is_completed = true;
                completed_count++;
                
                /* * 效能指標計算：
                 * 周轉時間 = 徹底結束的時間點 - 最初抵達的時間點
                 * 等待時間 = 周轉時間 - 實際在 CPU 上運算的時間
                 */
                processes[shortest_idx].turnaround_time = current_time - processes[shortest_idx].arrival_time;
                processes[shortest_idx].waiting_time = processes[shortest_idx].turnaround_time - processes[shortest_idx].burst_time;
            }
        } else {
            // Ready Queue 內無可用 Process，CPU 進入 Idle 狀態
            current_time++;
        }
    }
    std::cout << "[ 系統日誌 ] 所有行程執行完畢。\n\n";
}

int main() {
    // 建立課本範例的 Process 陣列
    std::vector<Process> p_list = {
        {1, 0, 8, 8, 0, 0, false},
        {2, 1, 4, 4, 0, 0, false},
        {3, 2, 9, 9, 0, 0, false},
        {4, 3, 5, 5, 0, 0, false}
    };

    simulate_srtf(p_list);

    // 輸出統計結果
    float total_wt = 0;
    std::cout << "PID\tArrival\tBurst\tWaiting Time\n";
    std::cout << "-------------------------------------\n";
    for (const auto& p : p_list) {
        total_wt += p.waiting_time;
        std::cout << "P" << p.pid << "\t" 
                  << p.arrival_time << "\t" 
                  << p.burst_time << "\t" 
                  << p.waiting_time << "\n";
    }
    std::cout << "-------------------------------------\n";
    std::cout << "平均等待時間: " << (total_wt / p_list.size()) << " ms\n";

    return 0;
}
```
---

## Round-Robin 理論觀念與數學計算剖析

#### 1. RR 核心定義與硬體依賴
* **本質**：RR 其實就是**加上了「強制搶占 (Preemption)」機制的 FCFS**。
* **Time Quantum (時間量 / Time Slice 時間片)**：這是 RR 的靈魂。作業系統會規定一個極短的時間上限（通常是 10 到 100 毫秒）。任何 Process 每次上 CPU，最多只能執行這個時間長度。
* **底層運作機制 (Circular Queue)**：
    1. 排程器從 Ready Queue 首端拿出一個 Process，設定硬體**計時器 (Timer)** 倒數 1 個 Quantum。
    2. **情境 A (提早結束)**：Process 的 CPU Burst 很短，在時間片用完前就主動放棄 CPU (例如去讀檔)。排程器直接換下一個。
    3. **情境 B (時間用盡)**：Process 算太久了，Timer 發出硬體中斷 (Interrupt)。作業系統瞬間介入，執行 Context Switch 將該 Process 強制拔下 CPU，並把它**丟回 Ready Queue 的最尾端 (Tail)** 重新排隊。

#### 2. 考試重點：甘特圖推演與等待時間計算
![image](https://hackmd.io/_uploads/rk17Eckh-l.png)
![image](https://hackmd.io/_uploads/HyHXVq13Wx.png)
這是課本上最經典的計算題範例（對應講義中的 $P_1, P_2, P_3$）：
* **條件設定**：$P_1$ 需 24ms，$P_2$ 需 3ms，$P_3$ 需 3ms。全部在時間 $0$ 抵達。**Time Quantum $q = 4$ ms。**

**【時間軸微觀推演 (Gantt Chart)】**
1. **$T=0 \sim 4$**：$P_1$ 先上場。執行 4ms 後，Timer 中斷。$P_1$ 被強制踢回佇列尾端。(此時佇列排隊順序：$P_2, P_3, P_1$)
2. **$T=4 \sim 7$**：換 $P_2$ 上場。$P_2$ 只需要 3ms，所以在 $T=7$ 時提早結束並離開系統。(佇列剩：$P_3, P_1$)
3. **$T=7 \sim 10$**：換 $P_3$ 上場。同樣只需要 3ms，在 $T=10$ 時結束離開。(佇列剩：$P_1$)
4. **$T=10 \sim 30$**：系統中只剩下 $P_1$ 唯一一個 Runnable Process。因此 $P_1$ 雖然每 4ms 會被中斷一次，但因為後面沒人排隊，它會立刻再次獲得 CPU，直到 $T=30$ 執行完畢。

**【等待時間 (Waiting Time) 嚴謹算法】**
在 RR 演算法中，計算等待時間很容易出錯，建議掌握以下兩種計算驗證法：

* **方法一：追蹤斷層法 (課本邏輯)**
    計算該 Process 「真正在排隊發呆」的時間區段。
    * **$P_1$ 等待時間**：一開始 $T=0$ 就執行了，沒等到。但在 $T=4$ 被踢下來，直到 $T=10$ 才又輪到它。所以排隊罰站了 $10 - 4 = \mathbf{6}$ ms。
    * **$P_2$ 等待時間**：從 $T=0$ 抵達，一直等到 $T=4$ 才上場，之後就結束了。等待 $\mathbf{4}$ ms。
    * **$P_3$ 等待時間**：從 $T=0$ 抵達，等到 $T=7$ 才上場。等待 $\mathbf{7}$ ms。
    * **平均等待時間**：$\frac{6 + 4 + 7}{3} = \mathbf{5.66}$ ms。
* **方法二：總周轉反推法 (萬無一失的公式)**
    利用公式：$Waiting\ Time = Turnaround\ Time - Burst\ Time$
    * $P_1$ 結束於 30，抵達於 0。周轉為 30。等待 = $30 - 24 = \mathbf{6}$ ms。
    * $P_2$ 結束於 7，抵達於 0。周轉為 7。等待 = $7 - 3 = \mathbf{4}$ ms。
    * $P_3$ 結束於 10，抵達於 0。周轉為 10。等待 = $10 - 3 = \mathbf{7}$ ms。

#### 3. 系統效能的極端拉扯：Quantum 該設多大？(Figure 5.5)
![image](https://hackmd.io/_uploads/BJ1qN9J2-g.png)

RR 的效能完全取決於 Time Quantum ($q$) 的大小設定，這是一個典型的工程 Trade-off (取捨) 問題：

* **極端一：$q$ 極度巨大 (例如 $q \to \infty$)**
    * RR 演算法將直接退化成 **FCFS**。護衛效應 (Convoy Effect) 將再次出現。
* **極端二：$q$ 極度微小 (例如 $q = 1$ ms)**
    * 系統看起來超級公平，每個 Process 都覺得自己擁有一個速度只有原本 $\frac{1}{n}$ 的專屬 CPU (Processor Sharing)。
    * **致命代價 (Context Switch Overhead)**：如 Figure 5.5 所示，原本 10ms 的工作，如果 $q=1$，會被強迫中斷 9 次。每一次中斷，Dispatcher 都要執行暫存器儲存與模式切換。如果 Context Switch 本身就要花 0.1ms，那麼系統會把大量的算力浪費在「切換」這個純耗損動作上，導致整體的 Turnaround Time 慘不忍睹。
* **系統架構師法則 (Rule of Thumb)**：
    1. Context Switch 的耗時，必須只佔 Time Quantum 的極小部分 (通常小於 10%)。
    2. **80% 的 CPU Bursts 長度，應該要短於 Time Quantum**。這確保多數的 I/O-bound 程式能在一個時間片內就處理完並主動離開，降低非自願 Context Switch 的次數。

#### 4. Round-Robin (RR) C++ 實作
為了忠實呈現 RR 演算法中「Circular Queue (環狀佇列)」與「Time Quantum (時間片)」的物理特性，此 C++ 實作將引入標準模板庫 (STL) 中的 std::queue 來擔任 Ready Queue 的角色，並精準模擬中斷與重排隊的過程。
```=c++
#include <iostream>
#include <vector>
#include <queue>
#include <iomanip>
#include <algorithm> // 引入 std::min

/*
 * 系統架構定義：Process Control Block (PCB)
 * 針對 RR 演算法，必須獨立追蹤剩餘時間 (remaining_time)。
 */
struct Process {
    int pid;             // 行程識別碼
    int burst_time;      // 原始所需的 CPU 總時間
    int remaining_time;  // 剩餘執行時間 (判斷是否觸發搶占的基準)
    int waiting_time;    // 等待時間
    int turnaround_time; // 周轉時間
};

/*
 * 核心排程邏輯：Round-Robin 狀態機模擬
 * 假設所有 Process 皆於 T=0 抵達系統 (依據課本 Figure 5.6 範例)
 */
void simulate_rr(std::vector<Process>& processes, int quantum) {
    int current_time = 0;
    int completed_count = 0;
    int n = processes.size();
    
    // 實作 Ready Queue：此處儲存的是 processes 陣列的索引值 (Index)
    std::queue<int> ready_queue; 

    // 初始化：將所有抵達的 Process 依序推入 Ready Queue
    for (int i = 0; i < n; i++) {
        ready_queue.push(i);
    }

    std::cout << "[ 系統日誌 ] 啟動 RR 搶占式排程模擬 (Time Quantum = " << quantum << "ms)\n";

    // 當還有 Process 尚未執行完畢時，持續推進時間軸
    while (completed_count < n) {
        if (ready_queue.empty()) {
            current_time++; // 系統閒置
            continue;
        }

        // CPU Scheduler 從 Ready Queue 首端拔出下一個任務
        int idx = ready_queue.front();
        ready_queue.pop();

        /*
         * 【核心物理模擬】：
         * 程式實際獲得的執行時間，取決於「剩餘所需時間」與「硬體時間片」兩者中較小的值。
         * 1. 提早結束：remaining_time < quantum，主動交出 CPU。
         * 2. 時間用盡：remaining_time > quantum，被 Timer 強制中斷。
         */
        int execute_time = std::min(processes[idx].remaining_time, quantum);

        std::cout << " -> [T=" << std::setw(2) << current_time 
                  << "] Dispatcher 載入 P" << processes[idx].pid 
                  << " (執行 " << execute_time << "ms)\n";

        // 推進時間軸與扣除剩餘時間
        processes[idx].remaining_time -= execute_time;
        current_time += execute_time;

        // 檢查狀態：是否自然終止？
        if (processes[idx].remaining_time == 0) {
            completed_count++;
            
            // 計算效能指標 (因全數 T=0 抵達，Turnaround = 結束時間點)
            processes[idx].turnaround_time = current_time;
            processes[idx].waiting_time = processes[idx].turnaround_time - processes[idx].burst_time;
            
            std::cout << "    [ 狀態 ] P" << processes[idx].pid << " 執行完畢離開系統。\n";
        } else {
            // 被強制搶占：丟回 Ready Queue 的尾端重新排隊
            ready_queue.push(idx);
            std::cout << "    [ 狀態 ] P" << processes[idx].pid << " 時間片耗盡，強制踢回佇列尾端。\n";
        }
    }
    std::cout << "[ 系統日誌 ] 所有行程執行完畢。\n\n";
}

int main() {
    // 建立課本範例的 Process 陣列
    std::vector<Process> p_list = {
        {1, 24, 24, 0, 0},
        {2,  3,  3, 0, 0},
        {3,  3,  3, 0, 0}
    };

    int time_quantum = 4; // 設定時間片大小
    
    simulate_rr(p_list, time_quantum);

    // 輸出統計結果
    float total_wt = 0;
    float total_tt = 0;
    std::cout << "PID\tBurst\tWaiting Time\tTurnaround Time\n";
    std::cout << "-----------------------------------------------\n";
    for (const auto& p : p_list) {
        total_wt += p.waiting_time;
        total_tt += p.turnaround_time;
        std::cout << "P" << p.pid << "\t" 
                  << p.burst_time << "\t" 
                  << p.waiting_time << "\t\t" 
                  << p.turnaround_time << "\n";
    }
    std::cout << "-----------------------------------------------\n";
    std::cout << "平均等待時間: " << (total_wt / p_list.size()) << " ms\n";
    std::cout << "平均周轉時間: " << (total_tt / p_list.size()) << " ms\n";

    return 0;
}
```
* `std::queue` 的解耦與輕量化：佇列中不應該儲存巨大的 Process 結構體實體 (避免昂貴的記憶體複製開銷)，而是僅儲存目標在陣列中的 Index (或指標)。這完美模擬了作業系統在 Ready Queue 裡只掛載 PCB 記憶體指標的底層行為。

* `std::min()` 的設計哲學：這行程式碼優雅地將兩種複雜的 Context Switch 情境 (自願 vs. 非自願) 統一。如果任務極短，`execute_time` 將等同於剩餘時間，不會浪費完整的 Quantum；如果任務冗長，則被死死限制在 `quantum` 內，實現保護機制。


---



## Priority Scheduling 理論觀念與系統架構剖析
#### 1. 核心定義與優先權數值慣例
* **基本運作邏輯**：作業系統會賦予每個 Process 一個「優先權數值 (Priority Number)」。當 CPU 閒置時，排程器永遠挑選**優先權最高**的 Process 上機執行。若遇到優先權相同的，則退化成 FCFS 順序。
* **數值慣例 (Smallest Integer = Highest Priority)**：
  不同作業系統對數值的定義不同，但在這本教科書與大多數 UNIX 系統中，**數字越小，優先權越高** (例如 $0$ 是最高優先權，$127$ 是最低)。
* **與 SJF 的繼承關係**：
  SJF 演算法本質上就是一種優先權排程，只是它的優先權定義為 $p = \frac{1}{\text{predicted next CPU burst}}$。預測的執行時間越長，優先權數值就越大 (優先級越低)。

#### 2. 系統架構的致命缺陷：Starvation (飢餓)
在一個負載極重的系統中，如果持續有高優先權的 Process 湧入 Ready Queue，會發生什麼事？
* **Indefinite Blocking (無限期阻塞)**：那些低優先權的 Process 會被永遠卡在佇列後面，連 1 毫秒的 CPU 資源都拿不到。這在作業系統理論中被稱為 **Starvation (飢餓)**。
* **歷史軼事**：課本提到一個經典的都市傳說 (Rumor)。1973 年 MIT 關閉他們的 IBM 7094 電腦時，在系統裡發現了一個 1967 年提交但「優先權太低」的 Process，它足足在系統裡餓了 6 年都沒被執行。

#### 3. 架構師的解法：Aging (老化 / 年資提升)
為了解決飢餓問題，系統工程師引入了 **Aging** 機制。
* **機制運作**：只要 Process 待在 Ready Queue 裡「排隊等待的時間夠久」，作業系統就會**定期調升它的優先權** (也就是把它的優先權數字減 $1$)。
* **最終結果**：即使一開始是一個優先權為 $127$ (最低) 的廢物任務，只要等得夠久，它的優先權最終會被扣減到 $0$ (最高)，從而保證它絕對有機會被執行，徹底消滅了系統死鎖 (Crash) 或遺失任務的風險。

#### 4. 進階考試重點：Priority 結合 Round-Robin (Figure f4914c & f49167)
![image](https://hackmd.io/_uploads/ByXzOckh-x.png)
![image](https://hackmd.io/_uploads/SkSQuc1hZe.png)
現代作業系統極少單獨使用純粹的 Priority Scheduling，而是將其與前一節的 Round-Robin (RR) 混合使用。
* **混合規則**：
  1. 系統嚴格執行最高優先權的任務。
  2. 當系統中存在**多個「優先權相同」且皆為當前最高**的任務時，這些任務之間採用 **Round-Robin** 輪流執行。

**【混合甘特圖微觀推演驗證 (Time Quantum $q = 2$)】**
* 任務清單：$P_1$(Burst=4, Pri=3), $P_2$(Burst=5, Pri=2), $P_3$(Burst=8, Pri=2), $P_4$(Burst=7, Pri=1), $P_5$(Burst=3, Pri=3)。
* **推演過程**：
  1. **$T=0 \sim 7$**：最高優先權是 $P_4$ (Pri=1)。只有它一個，所以它直接把 7ms 執行完畢。(期間不被中斷)
  2. **$T=7 \sim 16$**：剩下的最高優先權是 $P_2$ (Pri=2) 與 $P_3$ (Pri=2)。兩者優先權平手，觸發 RR 機制 ($q=2$)。
     * $P_2$ 跑 2ms ($T=7 \sim 9$) $\rightarrow$ $P_3$ 跑 2ms ($T=9 \sim 11$) $\rightarrow$ $P_2$ 跑 2ms ($T=11 \sim 13$) $\rightarrow$ $P_3$ 跑 2ms ($T=13 \sim 15$) $\rightarrow$ $P_2$ 跑完最後 1ms 結束 ($T=15 \sim 16$)。
  3. **$T=16 \sim 20$**：只剩下 $P_3$ (Pri=2) 擁有最高優先權。此時沒有其他 Pri=2 的對手，所以 $P_3$ 直接把剩下的 4ms 跑完。
  4. **$T=20 \sim 27$**：剩下 $P_1$ (Pri=3) 與 $P_5$ (Pri=3) 平手，再次觸發 RR ($q=2$) 輪流執行直到結束。

#### 5. Priority + RR 混合排程演算法 C++ 實作
為了在 C++ 中完美且高效地實作「依優先權分級，同級間輪轉」的物理特性，此實作將引入 Multilevel Queue (多層佇列) 的設計模式，利用標準模板庫 (STL) 中的 `std::map` 搭配 `std::queue` 來建立一個自動排序的排程中樞。
```=c++
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <iomanip>
#include <algorithm>

/*
 * 系統架構定義：Process Control Block (PCB)
 * 封裝行程的優先權與時間屬性。
 */
struct Process {
    int pid;             // 行程識別碼
    int burst_time;      // 原始所需的 CPU 總時間
    int priority;        // 優先權 (數值越小，優先權越高)
    int remaining_time;  // 剩餘執行時間
    int waiting_time;    // 等待時間
    int turnaround_time; // 周轉時間
};

/*
 * 核心排程邏輯：多層佇列狀態機模擬
 * 假設所有 Process 皆於 T=0 抵達系統 (依據課本範例)
 */
void simulate_priority_rr(std::vector<Process>& processes, int quantum) {
    int current_time = 0;
    int completed_count = 0;
    int n = processes.size();
    
    /*
     * 【底層資料結構設計：多層佇列 (Multilevel Queue)】
     * 使用 std::map 將優先權數值 (Key) 對應到一個獨立的 FIFO 佇列 (Value)。
     * std::map 底層由紅黑樹實作，會自動將 Key 依遞增排序。
     * 因此 map.begin() 永遠指向數值最小 (最高優先權) 的佇列，完美契合 OS 理論！
     */
    std::map<int, std::queue<int>> ready_queues;

    // 初始化：將所有抵達的 Process 分發到對應優先權的佇列中
    for (int i = 0; i < n; i++) {
        ready_queues[processes[i].priority].push(i);
    }

    std::cout << "[ 系統日誌 ] 啟動 Priority + RR 混合排程模擬 (Time Quantum = " << quantum << "ms)\n";

    // 當還有 Process 尚未執行完畢時，持續推進時間軸
    while (completed_count < n) {
        // 1. 尋找當前系統中，擁有最高優先權且「非空」的佇列
        auto it = ready_queues.begin();
        while (it != ready_queues.end() && it->second.empty()) {
            it++;
        }

        // 若所有佇列皆為空 (理論上此範例不會發生，因全數 T=0 抵達)
        if (it == ready_queues.end()) {
            current_time++;
            continue;
        }

        int current_priority = it->first;
        std::queue<int>& current_q = it->second;

        // 2. 從該優先權層級中，拔出排在最前面的 Process
        int idx = current_q.front();
        current_q.pop();

        /*
         * 3. 執行 Time Quantum 切割 (同上一節 RR 邏輯)
         */
        int execute_time = std::min(processes[idx].remaining_time, quantum);

        std::cout << " -> [T=" << std::setw(2) << current_time 
                  << "] Dispatcher 載入 P" << processes[idx].pid 
                  << " (Pri=" << current_priority 
                  << ", 執行 " << execute_time << "ms)\n";

        // 推進時間軸與扣除剩餘時間
        processes[idx].remaining_time -= execute_time;
        current_time += execute_time;

        // 4. 檢查終止狀態或重排隊
        if (processes[idx].remaining_time == 0) {
            completed_count++;
            processes[idx].turnaround_time = current_time;
            processes[idx].waiting_time = processes[idx].turnaround_time - processes[idx].burst_time;
            
            std::cout << "    [ 狀態 ] P" << processes[idx].pid << " 執行完畢離開系統。\n";
        } else {
            // 被強制搶占：丟回「自己所屬優先權層級」的佇列尾端重新排隊
            current_q.push(idx);
            std::cout << "    [ 狀態 ] P" << processes[idx].pid << " 時間片耗盡，退回 Pri=" << current_priority << " 佇列尾端。\n";
        }
    }
    std::cout << "[ 系統日誌 ] 所有行程執行完畢。\n\n";
}

int main() {
    // 建立課本範例的 Process 陣列
    // PID, Burst, Priority, Remaining, Wait, Turnaround
    std::vector<Process> p_list = {
        {1, 4, 3, 4, 0, 0},
        {2, 5, 2, 5, 0, 0},
        {3, 8, 2, 8, 0, 0},
        {4, 7, 1, 7, 0, 0},
        {5, 3, 3, 3, 0, 0}
    };

    int time_quantum = 2; // 設定時間片大小
    
    simulate_priority_rr(p_list, time_quantum);

    // 輸出統計結果
    float total_wt = 0;
    std::cout << "PID\tPri\tBurst\tWaiting Time\n";
    std::cout << "--------------------------------------\n";
    for (const auto& p : p_list) {
        total_wt += p.waiting_time;
        std::cout << "P" << p.pid << "\t" 
                  << p.priority << "\t" 
                  << p.burst_time << "\t" 
                  << p.waiting_time << "\n";
    }
    std::cout << "--------------------------------------\n";
    std::cout << "平均等待時間: " << (total_wt / p_list.size()) << " ms\n";

    return 0;
}
```

---

**【優先權反轉與解決方案】**
當最高優先權任務 ($P_{high}$) 被最低優先權任務 ($P_{low}$) 握有的鎖卡住，而 $P_{low}$ 又被中等優先權任務 ($P_{medium}$) 搶占時，就會發生「高優先權被中優先權間接餓死」的荒謬現象。1997 年 NASA 火星探路者號正是因為這個 Bug 導致系統不斷觸發 Watchdog Timer 重啟。
現代作業系統 (如即時 Linux 核心或 ROS 2 底層的 RTOS) 採用的標準解法是 **Priority Inheritance Protocol (優先權繼承協定)**：當 $P_{high}$ 發現鎖被 $P_{low}$ 拿走時，作業系統會**暫時把 $P_{low}$ 的優先權提升到與 $P_{high}$ 一樣高**。這樣 $P_{low}$ 就不會被 $P_{medium}$ 霸凌，能迅速執行完畢並釋放鎖，讓 $P_{high}$ 順利接手。

解決了這個歷史難題，接下來正式進入這份課本講義的 5.3.5 節：**Multilevel Queue Scheduling (多層佇列排程)**。這正是上一節 C++ 程式碼中 `std::map` 搭配 `std::queue` 的理論原型。

以下為步驟一的深度理論與系統架構剖析。


---

## Multilevel Queue 理論觀念與系統架構 

#### 1. 效能瓶頸的突破：從 $O(n)$ 到 $O(1)$ (Figure 5.7)
* **單一佇列的效能災難**：在傳統的 Priority Scheduling 中，如果把所有 Process 塞在同一個 Ready Queue 裡，排程器每次挑選任務時，都必須掃描整個陣列來尋找優先權最高的任務，時間複雜度高達 $O(n)$。在頻繁 Context Switch 的系統中，這會造成可怕的算力浪費。
* **物理分層 (Separate Queues)**：系統工程師改為「每一個優先權層級」建立一個獨立的 FIFO 佇列。排程器只需要從最高優先權的佇列「頭部」拔出任務即可，搜尋時間瞬間降為絕對的 $O(1)$。
![image](https://hackmd.io/_uploads/Bkvyicy2-e.png)

#### 2. 異質化排程：因材施教 (Intra-Queue Scheduling)
如課本 Figure 5.8 所示，系統不再只用數字來區分優先權，而是直接根據 Process 的「物理屬性 (Process Type)」將其永久綁定 (Assigned statically) 在特定的佇列中。最常見的劃分是：
* **Foreground Queue (前景 / 互動式行程)**：例如終端機輸入、UI 介面、ROS 2 的急停控制節點。這類任務要求極低的回應時間。
    * **專屬排程器**：內部採用 **Round-Robin (RR)**，確保即使有多個前景任務，大家也能公平且快速地輪流獲得微小的時間片。
* **Background Queue (背景 / 批次行程)**：例如影片轉檔、編譯 C++ 專案、SLAM 地圖的大型矩陣計算。這類任務不要求即時回應，只求吞吐量。
    * **專屬排程器**：內部採用 **FCFS (先到先服務)**，徹底省下 Context Switch 的開銷，讓 CPU 專心執行數學運算。

#### 3. 跨佇列排程策略 (Inter-Queue Scheduling)
![image](https://hackmd.io/_uploads/H1_xo9y3bl.png)
既然把任務分到了不同的佇列，那 CPU 該如何決定要服務哪一個佇列？課本提出了兩種架構策略：

* **策略一：Absolute Priority (絕對優先權 / Fixed-priority Preemptive)**
    * **邏輯**：階級森嚴。Real-time (即時任務) > System > Interactive > Batch。只有當上層所有佇列都「完全清空」時，下層佇列的任務才准許碰到 CPU。如果 Batch 佇列正在執行，突然來了一個 Interactive 任務，Batch 會立刻被強制搶占 (Preempted)。
    * **致命風險**：這會完美重演上一節提到的 **Starvation (飢餓)** 災難。如果前景任務源源不絕，背景任務將永無天日。
* **策略二：Time-Slicing Among Queues (佇列間時間切片)**
    * **邏輯**：為了防止飢餓，作業系統不採用絕對的階級壓制，而是把整體的 CPU 算力像切蛋糕一樣按比例分配 (Portion of CPU time)。
    * **實務設定**：例如分配 **80%** 的 CPU 總時間給 Foreground Queue (讓其內部去跑 RR)，保留 **20%** 的 CPU 時間給 Background Queue (讓其內部去跑 FCFS)。這樣既確保了互動式任務的高響應，又保證了背景任務擁有最低限度的推進力 (推進保證)。

#### 4. Multilevel Queue C++ 實作與架構解析
此 C++ 實作將建立兩個完全獨立的 std::queue：一個代表前景佇列 (套用 RR)，一個代表背景佇列 (套用 FCFS)。並且採用「絕對優先權 (Absolute Priority)」的跨佇列排程策略。
```=c++
#include <iostream>
#include <vector>
#include <queue>
#include <iomanip>
#include <algorithm>

/*
 * 系統架構定義：佇列類別 (Process Type)
 * 使用 enum class 強制型別安全，定義行程所屬的物理分層。
 */
enum class QueueType {
    FOREGROUND, // 前景任務：互動式、要求低延遲 (採用 RR)
    BACKGROUND  // 背景任務：批次運算、要求高吞吐量 (採用 FCFS)
};

/*
 * Process Control Block (PCB) 擴充
 * 加入 type 屬性，讓排程器知道該將其派發至哪一層佇列。
 */
struct Process {
    int pid;
    QueueType type;
    int burst_time;
    int remaining_time;
    int waiting_time;
    int turnaround_time;
};

/*
 * 核心排程邏輯：多層佇列狀態機模擬
 * 跨佇列策略：絕對優先權 (Foreground 必須為空，才能執行 Background)
 * 假設所有 Process 皆於 T=0 抵達系統。
 */
void simulate_multilevel_queue(std::vector<Process>& processes, int quantum) {
    int current_time = 0;
    int completed_count = 0;
    int n = processes.size();

    // 【底層資料結構：物理隔離的雙佇列】
    std::queue<int> fg_queue; // 儲存 Foreground 任務的 Index
    std::queue<int> bg_queue; // 儲存 Background 任務的 Index

    // 初始化：依據物理屬性進行路由分發 (Routing)
    for (int i = 0; i < n; i++) {
        if (processes[i].type == QueueType::FOREGROUND) {
            fg_queue.push(i);
        } else {
            bg_queue.push(i);
        }
    }

    std::cout << "[ 系統日誌 ] 啟動 Multilevel Queue 排程模擬\n";
    std::cout << "  -> 前景佇列策略: Round-Robin (q=" << quantum << "ms)\n";
    std::cout << "  -> 背景佇列策略: FCFS\n\n";

    // 持續推進時間軸，直到所有任務完成
    while (completed_count < n) {
        /*
         * 【跨佇列排程器 (Inter-Queue Scheduler)】
         * $O(1)$ 的決策邏輯：永遠先檢查 fg_queue 是否有任務。
         */
        if (!fg_queue.empty()) {
            // --- 處理前景任務 (Round-Robin) ---
            int idx = fg_queue.front();
            fg_queue.pop();

            int execute_time = std::min(processes[idx].remaining_time, quantum);

            std::cout << " [T=" << std::setw(2) << current_time 
                      << "] [前景佇列] 載入 P" << processes[idx].pid 
                      << " (執行 " << execute_time << "ms)\n";

            processes[idx].remaining_time -= execute_time;
            current_time += execute_time;

            if (processes[idx].remaining_time == 0) {
                completed_count++;
                processes[idx].turnaround_time = current_time;
                processes[idx].waiting_time = processes[idx].turnaround_time - processes[idx].burst_time;
            } else {
                fg_queue.push(idx); // RR：未完成則退回前景佇列尾端
            }

        } else if (!bg_queue.empty()) {
            // --- 處理背景任務 (FCFS) ---
            // 只有當 fg_queue 徹底清空時，才會進入此區塊。
            int idx = bg_queue.front();
            bg_queue.pop();

            // FCFS 不受 Time Quantum 限制，一口氣跑到完
            int execute_time = processes[idx].remaining_time;

            std::cout << " [T=" << std::setw(2) << current_time 
                      << "] [背景佇列] 載入 P" << processes[idx].pid 
                      << " (霸佔 CPU 執行 " << execute_time << "ms)\n";

            processes[idx].remaining_time -= execute_time;
            current_time += execute_time;

            completed_count++;
            processes[idx].turnaround_time = current_time;
            processes[idx].waiting_time = processes[idx].turnaround_time - processes[idx].burst_time;

        } else {
            // 理論上不會進入，除非有任務尚未抵達 (模擬器防呆機制)
            current_time++;
        }
    }
    std::cout << "\n[ 系統日誌 ] 所有行程執行完畢。\n\n";
}

int main() {
    // 建立 Process 陣列
    // PID, Type, Burst, Remaining, Wait, Turnaround
    std::vector<Process> p_list = {
        {1, QueueType::FOREGROUND, 5, 5, 0, 0},
        {2, QueueType::BACKGROUND, 8, 8, 0, 0},
        {3, QueueType::FOREGROUND, 3, 3, 0, 0},
        {4, QueueType::BACKGROUND, 6, 6, 0, 0}
    };

    int time_quantum = 2; // 前景 RR 的時間片大小
    
    simulate_multilevel_queue(p_list, time_quantum);

    // 輸出統計結果
    float total_wt = 0;
    std::cout << "PID\tType\t\tBurst\tWait\n";
    std::cout << "----------------------------------------\n";
    for (const auto& p : p_list) {
        total_wt += p.waiting_time;
        std::string t_str = (p.type == QueueType::FOREGROUND) ? "Foreground" : "Background";
        std::cout << "P" << p.pid << "\t" 
                  << std::setw(10) << std::left << t_str << "\t" 
                  << p.burst_time << "\t" 
                  << p.waiting_time << "\n";
    }
    std::cout << "----------------------------------------\n";
    std::cout << "平均等待時間: " << (total_wt / p_list.size()) << " ms\n";

    return 0;
}
```





---

前一節提到的系統死鎖與 `slam_toolbox` 被餓死的災難，正是因為「靜態綁定 (Statically Assigned)」帶來的僵化。為了解決這個問題，作業系統演化出了 CPU 排程理論的終極型態，也是現代多數作業系統（包含 Windows 與 macOS 底層）排程器的核心基礎：**Multilevel Feedback Queue Scheduling (多層回饋佇列排程，簡稱 MFQ)**。

---

## Multilevel Feedback Queue 理論與架構剖析 
#### 1. 核心破局點：動態遷移機制 (Dynamic Mobility)
* **前代痛點**：上一節的 Multilevel Queue 規定一個 Process 出生在哪個佇列，這輩子就只能待在那個佇列 (Permanently assigned)。如果前景佇列跑進了一個偽裝成互動式程式的 CPU-bound 巨獸，整個系統的響應速度就會崩潰。
* **MFQ 的革命**：允許 Process 在不同的佇列之間**自由升降級 (Move between queues)**。
    * **降級機制 (Demotion / Penalizing)**：如果一個 Process 太貪婪，把分配給它的 CPU 時間片 (Time Quantum) 整個耗盡，系統就會認定它是 CPU-bound，並無情地將它踢到下一個優先權較低的佇列。
    * **升級機制 (Promotion / Aging)**：為了防止最低優先權的任務餓死 (Starvation)，只要一個 Process 在底層等得太久，系統就會啟動前幾節學過的 Aging (老化) 機制，把它重新拉回高層佇列。

#### 2. 完美篩選器的物理運作 (Figure 5.9 推演)
![image](https://hackmd.io/_uploads/rkSDT9yhZe.png)

課本提供了一個極度經典的三層佇列模型，這個模型就像是一個「重力篩選器」，能自動分辨出 I/O-bound 與 CPU-bound 的程式，而**完全不需要預先知道程式的屬性**。

* **架構設定**：
    * **Queue 0 (最高層)**：採用 RR 演算法，Time Quantum = **8 ms**。
    * **Queue 1 (中間層)**：採用 RR 演算法，Time Quantum = **16 ms**。
    * **Queue 2 (最低層)**：採用 FCFS 演算法。
* **微觀推演**：
    1. **新任務抵達**：所有新來的 Process，無論是誰，一律先放進最高級的 Queue 0。
    2. **第一次過濾**：Process 獲得 8 ms 的 CPU 時間。
        * 如果它是 I/O-bound (例如只需 2 ms 就去讀檔)，它會主動釋放 CPU。下次甦醒時，**它依然會被排在 Queue 0**，享有最高優先權。
        * 如果它是 CPU-bound，8 ms 耗盡後，它會被強制中斷，並**貶謫到 Queue 1** 的尾端。
    3. **第二次過濾**：當 Queue 0 空了，Queue 1 的 Process 開始執行，獲得 16 ms。
        * 同理，沒跑完的巨獸會再次被強制中斷，**貶謫到最底層的 Queue 2**。
    4. **無盡的深淵 (FCFS)**：掉入 Queue 2 的 CPU-bound 程式，只能在 Queue 0 和 Queue 1 都完全閒置的深夜時段，透過 FCFS 慢慢消化它龐大的運算量。

#### 3. 系統架構師的終極考驗：五大定義參數
MFQ 是最通用 (Most general) 的排程演算法，因為它可以被塑造成任何形狀。但也因為如此，它是最複雜 (Most complex) 的。實作一個真正的 MFQ，系統工程師必須精準定義以下五個參數：
1. **佇列的數量** (The number of queues)。
2. **每個佇列內部的排程演算法** (The scheduling algorithm for each queue)。
3. **升級條件** (When to upgrade a process)。
4. **降級條件** (When to demote a process)。
5. **初始分發規則** (Which queue a process will enter initially)。

---

**【架構師觀點：解決 AMR 的物理瑕疵】**

回到上一節結尾的 AMR 思考題。如果採用 MFQ，當機器人駛入人群時：
1. 「緊急避障 Node」因為每次只需要極短的運算時間 (小於 8 ms) 就會送出馬達訊號並休眠，因此它會**永遠穩坐在最高優先權的 Queue 0**，確保 AMR 的絕對安全與零延遲。
2. `slam_toolbox` 在建圖最佳化時，由於會瘋狂消耗 CPU，它會迅速吃滿 8 ms 與 16 ms 的時間片，自動**沉底到 Queue 2** 默默運算。
3. 若人群持續太久導致 Queue 0 一直被佔用，底層的 `slam_toolbox` 會觸發 Aging 升級條件，暫時被拉回 Queue 0 取得算力，更新地圖後再沉底。
這完美消除了佔用網格地圖 (Occupancy Grid Map) 因為 SLAM 節點徹底餓死而產生大面積未探索盲區 (Unknown space) 的物理瑕疵！

#### 4. Multilevel Feedback Queue (MFQ) C++ 實作
多層回饋佇列 (Multilevel Feedback Queue, MFQ) 的狀態機確實是排程演算法中最複雜的頂點，涉及跨佇列搶占、動態降級與老化升級機制。
```=c++
#include <iostream>
#include <vector>
#include <queue>
#include <iomanip>
#include <algorithm>

/*
 * 系統架構定義：Process Control Block (PCB)
 * 紀錄該行程目前身處的佇列層級 (current_queue_level)
 */
struct Process {
    int pid;
    int burst_time;
    int remaining_time;
    int current_queue_level; // 0: 最高優先權, 1: 中等, 2: 最低 (FCFS)
    int waiting_time;
    int turnaround_time;
};

/*
 * 核心排程邏輯：MFQ 狀態機模擬 (3 層結構)
 * Q0: Time Quantum = 8
 * Q1: Time Quantum = 16
 * Q2: FCFS
 */
void simulate_mfq(std::vector<Process>& processes) {
    int current_time = 0;
    int completed_count = 0;
    int n = processes.size();

    // 建立三層物理隔離的佇列
    std::queue<int> q0; // 最高優先權 (RR, q=8)
    std::queue<int> q1; // 中等優先權 (RR, q=16)
    std::queue<int> q2; // 最低優先權 (FCFS)

    // 初始化：所有新任務一律進入最高優先權的 Q0
    for (int i = 0; i < n; i++) {
        processes[i].current_queue_level = 0;
        q0.push(i);
    }

    std::cout << "[ 系統日誌 ] 啟動 MFQ 排程模擬\n";
    std::cout << "  -> Q0: RR (Quantum=8)\n";
    std::cout << "  -> Q1: RR (Quantum=16)\n";
    std::cout << "  -> Q2: FCFS\n\n";

    // 持續推進時間軸
    while (completed_count < n) {
        int idx = -1;
        int quantum_limit = 0;
        int current_level = -1;

        /*
         * 【跨佇列排程器 (Inter-Queue Scheduler)】
         * 嚴格遵守絕對優先權：Q0 > Q1 > Q2
         */
        if (!q0.empty()) {
            idx = q0.front();
            q0.pop();
            quantum_limit = 8;
            current_level = 0;
        } else if (!q1.empty()) {
            idx = q1.front();
            q1.pop();
            quantum_limit = 16;
            current_level = 1;
        } else if (!q2.empty()) {
            idx = q2.front();
            q2.pop();
            quantum_limit = processes[idx].remaining_time; // FCFS 沒有時間片限制
            current_level = 2;
        } else {
            current_time++;
            continue;
        }

        /*
         * 【執行與降級判定 (Execution & Demotion)】
         */
        int execute_time = std::min(processes[idx].remaining_time, quantum_limit);

        std::cout << " [T=" << std::setw(2) << current_time 
                  << "] 載入 P" << processes[idx].pid 
                  << " (自 Q" << current_level << " 取出, 執行 " << execute_time << "ms)\n";

        processes[idx].remaining_time -= execute_time;
        current_time += execute_time;

        // 檢查是否執行完畢
        if (processes[idx].remaining_time == 0) {
            completed_count++;
            processes[idx].turnaround_time = current_time;
            processes[idx].waiting_time = processes[idx].turnaround_time - processes[idx].burst_time;
            std::cout << "    -> [狀態] P" << processes[idx].pid << " 執行完畢。\n";
        } 
        // 若未完畢，觸發降級懲罰 (Demotion) 機制
        else {
            if (current_level == 0) {
                processes[idx].current_queue_level = 1;
                q1.push(idx);
                std::cout << "    -> [狀態] P" << processes[idx].pid << " 耗盡 Q0 時間片，降級至 Q1。\n";
            } else if (current_level == 1) {
                processes[idx].current_queue_level = 2;
                q2.push(idx);
                std::cout << "    -> [狀態] P" << processes[idx].pid << " 耗盡 Q1 時間片，降級至 Q2 (沉底)。\n";
            } else {
                // 已在最底層，繼續排在 Q2 尾端 (理論上 FCFS 不會被中斷，但若有高優先權搶占機制則需重排)
                // 此處簡化模型中，FCFS 拿到的 quantum_limit 是全部剩餘時間，故不會進入此分支
                q2.push(idx); 
            }
        }
    }
    std::cout << "\n[ 系統日誌 ] 所有行程執行完畢。\n\n";
}

int main() {
    // 建立 Process 陣列 (模擬不同長度的任務)
    std::vector<Process> p_list = {
        {1, 30, 30, 0, 0, 0}, // CPU-bound 巨獸 (需 30ms)
        {2,  5,  5, 0, 0, 0}, // I/O-bound 短任務 (需 5ms)
        {3, 15, 15, 0, 0, 0}  // 中型任務 (需 15ms)
    };
    
    simulate_mfq(p_list);

    // 輸出統計結果
    float total_wt = 0;
    std::cout << "PID\tBurst\tWait\tTurnaround\n";
    std::cout << "------------------------------------\n";
    for (const auto& p : p_list) {
        total_wt += p.waiting_time;
        std::cout << "P" << p.pid << "\t" 
                  << p.burst_time << "\t" 
                  << p.waiting_time << "\t" 
                  << p.turnaround_time << "\n";
    }
    std::cout << "------------------------------------\n";
    std::cout << "平均等待時間: " << (total_wt / p_list.size()) << " ms\n";

    return 0;
}
```
## Contention Scope 理論重構與雙層排程解析 

在探討這節之前，必須先建立一個核心的認知翻轉：**在現代作業系統中，排程器真正分配 CPU 算力的單位，已經不再是 Process (行程)，而是 Kernel-level Thread (核心級執行緒)。**

由於執行緒分為 User-level (使用者級) 與 Kernel-level (核心級)，它們為了爭奪最終的物理 CPU，會在兩個不同的「競技場」進行廝殺。這兩個競技場就是所謂的 Contention Scope。

#### 1. 競技場一：Process-Contention Scope (PCS, 行程競爭範圍)
* **發生位置**：User Space (使用者空間)。
* **負責裁判**：Thread Library (執行緒函式庫，例如 Pthreads)。
* **競爭對象**：**同一個 Process 內部**的所有 User-level Threads。
* **搶奪目標**：**LWP (輕量級行程)**。LWP 對 User Thread 來說，就像是一顆「虛擬處理器」。
* **排程規則**：通常由程式設計師手動設定優先權 (Priority)。函式庫會挑選優先權最高的執行緒上機。但課本特別警告：**PCS 通常不保證提供 Time-slicing (時間切片)**，也就是說，如果兩個執行緒優先權一樣高，函式庫不一定會像 Round-Robin 那樣讓它們公平輪流，有可能其中一個會霸佔虛擬處理器直到阻塞。

#### 2. 競技場二：System-Contention Scope (SCS, 系統競爭範圍)
* **發生位置**：Kernel Space (核心空間)。
* **負責裁判**：作業系統的 CPU Scheduler (CPU 排程器，如上一節學的 MFQ)。
* **競爭對象**：**全系統中所有**的 Kernel-level Threads (無論它們隸屬於哪一個 Process)。
* **搶奪目標**：**實體的 Physical CPU Cores (物理核心)**。
* **排程規則**：由作業系統底層極度複雜的演算法全權決定，確保系統整體的吞吐量與公平性。

---

#### 💡 具象化情境解析：M:M 模型的「雙層大逃殺」

為了徹底理解這兩個競技場的運作，我們以 Many-to-Many (M:M) 模型為例，設定以下物理環境：
* **硬體**：這台電腦有 **4 顆**實體 CPU 核心。
* **系統狀態**：全系統還有其他軟體派出的 **50 個** Kernel Threads 正在背景執行。
* **你的程式**：你寫了一個 Process，內部開啟了 **10 個** User-level Threads。作業系統評估後，發給這個 Process **3 個** LWP (Kernel Threads) 作為載具。

這時候，排程將無可避免地分為兩個階段進行：

**【第一戰場：內部選拔賽 (PCS)】**
你的 **10 個 User Threads** 發現只有 3 個 LWP 座位。此時，Thread Library 介入，根據你寫的程式碼優先權，在 Process 內部舉辦選拔賽 (這就是 **PCS**)。最終，有 **3 個** User Threads 贏得比賽，成功綁定到這 3 個 LWP 上，拿到了進入作業系統核心的門票。
*(注意：此時它們還沒有真正拿到物理 CPU 算力，只是坐上了虛擬處理器！)*

**【第二戰場：全國大會戰 (SCS)】**
這 3 個贏得 LWP 的執行緒，化身為 3 個 Kernel Threads 進入了 OS 核心。現在，它們必須與全系統另外那 50 個 Kernel Threads 一起，總共 **53 個 Kernel Threads**，交由作業系統的 CPU Scheduler 來決定誰能搶到那 **4 顆實體 CPU 核心**。這場跨越 Process 藩籬的無差別大亂鬥，就是 **SCS**。

---

#### 🛠️ 架構師的現實考量：Ubuntu Linux 與 One-to-One 模型

將理論拉回我們開發 ROS 2 AMR 的實際環境 (Ubuntu Linux)：
現代 Linux 與 Windows 系統為了效能與架構簡化，全面採用 **One-to-One (1:1) 模型**。
* 在 1:1 模型中，你每建立 1 個 User Thread，作業系統就直接送你 1 個專屬的 Kernel Thread。
* 也就是說，10 個 User Threads 直接配發 10 個 Kernel Threads。
* **核心結論**：因為沒有人需要搶載具，**在 Linux 環境下，PCS 徹底消失 (不存在內部選拔)**。這 10 個執行緒全部直接進入底層，參與 **SCS** 的物理 CPU 爭奪戰。因此，系統的效能完全取決於 OS 排程器的強弱，而非執行緒函式庫。

---


## Pthread Scheduling API 架構與設計哲學剖析

這份課本範例程式碼展示了如何在 C 語言環境下，對作業系統底層的「執行緒競爭範圍 (Contention Scope)」進行精細的控制。其核心理論可拆解為以下三大架構觀念：

#### 1. POSIX 屬性物件設計模式 (Attribute Object Pattern)
在作業系統底層開發中，建立一個執行緒牽涉到數十種硬體與軟體參數（包含堆疊大小、排程策略、競爭範圍、分離狀態等）。
* **傳統痛點**：如果將這些參數全部寫進 `pthread_create` 的函式傳參中，會導致 API 極度臃腫且缺乏擴充性。
* **Pthread 解法**：引入了高度封裝的結構體 **`pthread_attr_t` (執行緒屬性物件)**。系統工程師只需先建立這個「設定檔物件」，把需要的參數調整好，最後在建立執行緒時，將這個物件的指標傳入即可。這完美體現了軟體工程中的「高內聚、低耦合」原則。

#### 2. API 狀態流轉邏輯 (State Transition)
觀察課本範例的生命週期，可以歸納出標準的系統 API 呼叫 SOP：
* **初始化 (Initialization)**：呼叫 `pthread_attr_init()`。作業系統會將該屬性物件填入當前平台的「預設值」。
* **狀態探測 (Inspection)**：呼叫 `pthread_attr_getscope()`。系統工程師不該盲目假設底層環境，必須先透過 Getter 函式探測當前作業系統預設採用的競爭範圍（是 PCS 還是 SCS？）。
* **強制覆寫 (Override)**：呼叫 `pthread_attr_setscope()`。開發者依照系統架構需求，強行將屬性物件內的競爭範圍標籤修改為 `PTHREAD_SCOPE_SYSTEM` (SCS)。
* **實體化 (Instantiation)**：呼叫 `pthread_create()`。作業系統讀取修改後的屬性物件，向 Kernel 申請建立對應的 LWP 與 User Thread。

#### 3. Linux 環境的物理現實驗證
如同上一節所探討的理論，如果將這段程式碼直接放到開發常用的 Ubuntu (WSL2) 環境中編譯並執行，會觀察到以下物理現實：
1. **探測階段**：`pthread_attr_getscope` 會回傳 `PTHREAD_SCOPE_SYSTEM`。因為 Linux 預設就是 One-to-One 模型。
2. **覆寫階段**：將其設定為 `PTHREAD_SCOPE_SYSTEM` 絕對能成功。但如果工程師刻意將其改為 `PTHREAD_SCOPE_PROCESS`，`pthread_attr_setscope` 會直接回傳錯誤碼 (通常為 `ENOTSUP`)，因為 Linux Kernel 底層根本沒有實作 M:M 模型的 PCS 載具分配機制。

#### 4. Pthread scheduling API.
```=c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 5

/*
 * 執行緒的進入點函式 (Entry Point)
 */
void *runner(void *param) {
    /* 實際的業務邏輯 (例如：感測器資料讀取、矩陣運算等) */
    
    // 執行緒生命週期結束，主動退出
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    int i, scope;
    pthread_t tid[NUM_THREADS];
    pthread_attr_t attr; // 核心：執行緒屬性物件

    /* * 1. 初始化屬性物件
     * 系統會將此物件填入當前作業系統的「預設參數」
     */
    pthread_attr_init(&attr);

    /* * 2. 探測階段 (Inspection)
     * 取得當前屬性物件中的競爭範圍設定，並透過指標寫入 scope 變數
     */
    if (pthread_attr_getscope(&attr, &scope) != 0) {
        fprintf(stderr, "[ 系統錯誤 ] 無法取得排程競爭範圍狀態\n");
    } else {
        if (scope == PTHREAD_SCOPE_PROCESS)
            printf("[ 系統日誌 ] 當前預設範圍: PTHREAD_SCOPE_PROCESS (PCS)\n");
        else if (scope == PTHREAD_SCOPE_SYSTEM)
            printf("[ 系統日誌 ] 當前預設範圍: PTHREAD_SCOPE_SYSTEM (SCS)\n");
        else
            fprintf(stderr, "[ 系統錯誤 ] 讀取到非法的競爭範圍數值\n");
    }

    /* * 3. 強制覆寫階段 (Override)
     * 將排程演算法明確設定為 SCS (系統競爭範圍)，要求 OS 提供專屬 Kernel Thread
     * 註：若在 Linux 下強行設定為 PTHREAD_SCOPE_PROCESS，此 API 將回傳錯誤碼。
     */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0) {
        fprintf(stderr, "[ 系統警告 ] 競爭範圍設定失敗 (底層 OS 可能不支援該模型)\n");
    }

    /* * 4. 實體化階段 (Instantiation)
     * 根據設定好的 attr 屬性物件，向作業系統請求建立執行緒
     */
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], &attr, runner, NULL);
    }

    /* * 5. 資源同步與回收
     * 阻塞主程式，確保所有子執行緒都完成工作，防止產生 Zombie Threads
     */
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    /* 銷毀屬性物件，釋放系統記憶體資源 */
    pthread_attr_destroy(&attr);

    printf("[ 系統日誌 ] 所有執行緒處理完畢，系統安全退出。\n");
    return 0;
}
```

---

## 多處理器排程架構理論與佇列設計剖析
這段內容宣告了作業系統排程理論正式跨入現代硬體架構的領域：**Multiple-Processor Scheduling (多處理器排程)**。前面 5.3 節學過的所有演算法 (FCFS, SJF, RR, MFQ)，其預設前提都是「系統只有一顆單核 CPU」。一旦引入了多顆物理核心 (Multicore CPUs)，系統就能實現真正的**負載共享 (Load sharing)** 與平行運算，但隨之而來的架構複雜度也呈指數級上升。
在探討具體的多核心排程時，作業系統架構師首先必須決定系統整體的「權力分配結構」。課本點出了兩大流派：

#### 1. 權力集中的極端：Asymmetric Multiprocessing (AMP, 非對稱多處理)
* **架構邏輯**：採用「主從架構 (Master-Slave)」。系統中會指定唯一一顆核心擔任 **Master Server (主伺服器)**，只有它有權限執行作業系統的排程決策、I/O 處理與修改 Kernel 資料結構。其他的核心 (Slaves) 淪為純粹的無情運算機器，只負責跑使用者的 Application Code。
* **優點**：作業系統的開發極度簡單。因為只有 Master 會去動 Kernel 的資料，所以絕對不會發生資料競爭 (Data Sharing / Race Condition)，完全不需要寫複雜的鎖 (Locks)。
* **致命缺陷 (Bottleneck)**：當系統核心數增加到 8 核、16 核時，Master 核心會因為要同時幫幾十個核心分配工作而過載，成為拖垮整個系統吞吐量的**效能瓶頸 (Potential Bottleneck)**。

#### 2. 現代標準的去中心化：Symmetric Multiprocessing (SMP, 對稱多處理)
* **架構邏輯**：眾生平等。每一顆物理核心都是獨立且全能的，各自擁有自己的 CPU Scheduler。當某顆核心閒置時，它會「自己去挑選 (Self-scheduling)」下一個要執行的 Thread。
* **普及度**：幾乎所有現代作業系統（包含 Windows、macOS、Linux，以及手機端的 Android/iOS）皆全面採用 SMP 架構。

#### 3. 系統工程師的終極難題：SMP 的底層佇列設計 (Figure 5.11)
既然每一顆核心都要自己去 Ready Queue 抓任務來做，那麼這個 Ready Queue 在記憶體中該長什麼樣子？課本提出了兩種資料結構策略，這也是考卷上最愛考的效能瓶頸分析：
![image](https://hackmd.io/_uploads/rJ7X-SKnWg.png)


* **策略一：Common Ready Queue (全域共享佇列)**
    * **設計**：所有的 Threads 都塞在同一個巨大的 Queue 裡面。所有核心都去這個 Queue 拔任務。
    * **物理災難 (Lock Contention)**：如 Chapter 6 將探討的，當 Core 0 和 Core 1 同時試圖從 Queue 拔出同一個 Thread 時，會引發 Race Condition。為了防止系統崩潰，這個 Queue 必須加上 Mutex Lock。結果就是：**存取這個 Queue 變成了一個極度嚴重的效能瓶頸 (Performance Bottleneck)**，因為所有核心都在排隊等著解鎖。
* **策略二：Per-Core Run Queues (單核專屬私有佇列)**
    * **設計**：作業系統為「每一顆核心」建立一個專屬的 Ready Queue。當一個 Thread 被建立時，OS 會把它分發到某顆核心的私有佇列中。核心只需要看著自己的 Queue 即可。
    * **系統優勢**：
        1. **Lock-Free (無鎖化)**：因為沒有其他核心會來搶自己的 Queue，大幅降低了 Lock Contention 的純耗損。
        2. **Cache Affinity (快取親和性)**：這對 C++ 效能極度重要！Thread 每次都在同一顆核心上喚醒，該核心的 L1/L2 Cache 裡面還有它上次算到一半的熱資料 (Hot Data)，記憶體讀取速度極快。
    * **現代標配與新挑戰**：這是現代 SMP 支援的最常見作法。但也衍生出一個新問題：**Load Imbalance (負載不均)**。如果 Core 0 的 Queue 塞滿了 CPU-bound 巨獸，而 Core 1 的 Queue 早就空了，Core 1 就會在旁邊閒置發呆。這必須仰賴後續的「負載平衡演算法」來解決。

---

## Multicore Processors 底層架構與雙層排程剖析

#### 1. 效能的物理死穴：Memory Stall (記憶體停滯 / Figure 5.12)
![image](https://hackmd.io/_uploads/r1bhIoJhZx.png)

* **硬體現實**：現代將多個運算核心塞進同一塊實體晶片中，稱為 **Multicore Processor (多核心處理器)**。雖然 CPU 運算速度極快，但主記憶體 (RAM) 的讀取速度卻完全跟不上。
* **Cache Miss 的代價**：當 C++ 程式試圖讀取一個不在 L1/L2 快取內的資料時，CPU 必須去遙遠的 RAM 抓資料。這段等待的時間，被稱為 **Memory Stall (記憶體停滯)**。
* **Figure 5.12 解析**：圖中的 `C` 代表 CPU 正在認真運算 (Compute cycle)，`M` 代表 CPU 正在乾等記憶體資料送來 (Memory stall cycle)。在極端情況下，CPU 甚至會把 50% 的時間浪費在 `M` 狀態上發呆，這對系統吞吐量是毀滅性的打擊。

#### 2. 硬體架構師的救贖：Chip Multithreading (晶片多執行緒 / Figure 5.13 & 5.14)
為了解決 CPU 發呆的問題，硬體工程師在「單一顆實體物理核心」裡面，塞入了兩套（或多套）暫存器與 Program Counter。這被稱為 **Hardware Threads (硬體執行緒)**。
![image](https://hackmd.io/_uploads/HyP0Uok3-g.png)
* **交錯掩護 (Figure 5.13)**：當 `thread_0` 遇到 Memory Stall 卡住時 (進入 `M` 狀態)，物理核心的運算單元 (ALU) 不會跟著發呆，而是瞬間切換過去執行 `thread_1` 的程式碼 (進入 `C` 狀態)。兩者完美交錯，把物理算力榨乾。

![image](https://hackmd.io/_uploads/HyH4wiknbg.png)
* **作業系統的幻覺 (Figure 5.14)**：這項技術被 Intel 稱為 **Hyper-threading (超執行緒)**，學術上稱為 **SMT (Simultaneous Multithreading)**。對 Ubuntu Linux 等作業系統而言，它根本不知道底層實體核心長怎樣。如果一顆 CPU 有 4 顆實體核心，每顆核心有 2 個硬體執行緒，作業系統的 `/proc/cpuinfo` 就會被騙，以為自己擁有 8 顆 **Logical CPUs (邏輯處理器)**。

#### 3. 硬體層級的切換策略：Coarse-grained vs. Fine-grained
實體核心在兩個「硬體執行緒」之間切換，有兩種設計流派：
* **Coarse-grained (粗粒度)**：只有發生「長時間延遲事件」(例如嚴重的 Memory Stall) 時才切換。缺點是切換成本極高，必須把整個 CPU Pipeline (指令管線) 清空 (Flush) 才能換人跑。
* **Fine-grained (細粒度 / Interleaved)**：在非常微小的指令邊界上瘋狂切換。這需要極度複雜的硬體邏輯電路支援，但切換成本近乎為零。

#### 4. 系統架構的終極型態：Two Levels of Scheduling (雙層排程 / Figure 5.15)
![image](https://hackmd.io/_uploads/H18HvjyhWe.png)
當軟體遇上 SMT 硬體，系統排程就變成了極度複雜的「兩階段過濾器」：
* **Level 1 (OS 軟體排程)**：這是我們前面 5.3 節學的所有東西。Ubuntu Linux 作業系統的排程器 (如 CFS 或 MFQ)，負責從一堆 User/Software Threads 中挑選幸運兒，把它們派發到 8 個「邏輯處理器 (Logical CPUs)」上。
* **Level 2 (CPU 硬體排程)**：這是晶片內部的電路邏輯。決定實體物理核心的運算單元 (ALU)，當下這 1 奈秒到底要負責跑哪一個硬體執行緒的指令。(例如 Intel 使用動態急迫性數值，UltraSPARC 使用 Round-Robin)。
**【架構師觀點：OS Awareness (作業系統感知) 的重要性】**

課本最後一段提到了一個極度關鍵的觀念：**作業系統如果知道硬體的共享拓樸，排程會更聰明。**

想像你在開發 ROS 2 AMR 系統，裡面有兩個極度消耗 CPU 的核心節點：`slam_toolbox` (建圖) 與 `nav2_planner` (路徑規劃)。
如果你把這兩個 Node (Software Threads) 都排程在同一個實體核心的兩個「邏輯 CPU」上，它們將會共用同一個 L1 Cache 與運算管線。這會導致它們瘋狂互相踢掉對方的快取資料 (Cache Thrashing)，執行速度比單獨跑還要慢！
一個「有硬體感知能力 (Hardware-aware)」的作業系統，會刻意把這兩個重度任務，拆分到「不同實體核心」的邏輯 CPU 上，讓它們擁有各自獨立的 L1 Cache 與物理算力。

---




## 負載平衡、處理器親和性與 NUMA 衝突架構

#### 1. Load Balancing (負載平衡)：消滅閒置算力
既然每顆核心都有自己的 Ready Queue，就必然會發生「貧富不均」的現象。Core 0 可能塞滿了 10 個 CPU-bound 巨獸，而 Core 1 的 Queue 早就空了在發呆。為了讓 SMP 系統發揮最大吞吐量，OS 必須進行 **Load Balancing (負載平衡)**。

作業系統有兩種搬運 Thread 的底層機制：
* **Push Migration (推式遷移)**：系統中有一個特定的背景巡邏任務 (Task)。它會定期檢查所有 CPU 的負載，如果發現 Core 0 太忙，它會主動介入，把 Core 0 佇列裡的 Threads **「推」** 到閒置的 Core 1 佇列中。
* **Pull Migration (拉式遷移)**：這是一種自發性機制。當 Core 1 執行完自己的任務，發現自己的佇列空了 (Idle)，它會主動去 Core 0 的佇列裡面，把別人的 Thread **「拉」** 過來自己算。
* **實務架構**：現代的 Linux CFS 排程器是**同時實作**這兩種機制的，雙管齊下確保算力不浪費。

#### 2. Processor Affinity (處理器親和性)：保護珍貴的快取
前面說負載平衡會把 Thread 搬來搬去，但這在硬體層面上要付出慘痛的代價。

* **Warm Cache (暖快取)**：當一個 Thread 在 Core 0 上跑了一陣子，Core 0 的 L1/L2 Cache 裡面就會塞滿這個 Thread 剛讀取過的變數與指令。這時候記憶體存取速度極快。
* **Migration Penalty (遷移懲罰)**：如果負載平衡機制硬把這個 Thread 搬到 Core 1 去跑。它在 Core 0 建立的心血 (Cache) 會全部作廢 (Invalidated)，並且必須在 Core 1 重新經歷痛苦的 Memory Stall，把資料從遠端的 RAM 慢慢拉進 Core 1 的 Cache 裡 (Repopulated)。
* **架構師的防禦**：為了避免這種純耗損，OS 演算法會盡最大努力讓 Thread 「綁定」在它原本執行的 CPU 上，這就稱為 **Processor Affinity (處理器親和性)**。
    * **Soft Affinity (軟親和性)**：OS「盡量」不搬移你，但如果系統真的太閒或太擠，還是會把你搬走 (Linux 預設機制)。
    * **Hard Affinity (硬親和性)**：系統工程師透過 System Call (如 Linux 的 `sched_setaffinity()`) 強制下令，規定這個 Thread **絕對只能**在 Core 0 或 Core 1 上跑，OS 絕對不准把它搬到其他核心。


#### 3. 🚨 考試重點：NUMA (Non-Uniform Memory Access) 與排程的連鎖反應
![image](https://hackmd.io/_uploads/B1OXl3y3-e.png)
早期的電腦，所有 CPU 都是透過同一條匯流排 (Bus) 去存取同一塊記憶體，這叫 UMA (Uniform Memory Access)。但隨著 CPU 核心數暴增到幾十顆，這條共用的 Bus 會被塞爆。為了解決硬體頻寬極限，工程師發明了 **NUMA (非均勻記憶體存取)** 架構。

* **NUMA 物理架構 (Figure 5.16)**：
    主機板上有多顆物理 CPU 晶片 (Node)，**每一顆 CPU 晶片旁邊，都直接插著專屬的記憶體 (Local Memory)**。多顆 CPU 之間再透過一條高速通道 (Interconnect) 互連。
* **存取速度的絕對差異**：
    * **Fast Access**：CPU 0 讀取插在自己旁邊的 Local Memory，速度極快。
    * **Slow Access**：如果 CPU 0 要讀取的資料，剛好存在 CPU 1 旁邊的記憶體裡，指令就必須跨越 Interconnect 去拿，這會造成極大的延遲 (Latency)。
* **NUMA-aware Scheduling (具備 NUMA 感知能力的排程)**：
    一個及格的現代作業系統，其 CPU Scheduler 必須跟 Memory-placement Algorithm (記憶體配置演算法) 完美聯動。**當 OS 決定把 Thread 排在 CPU 0 上執行時，OS 必須確保這個 Thread 呼叫 `malloc()` 或 `new` 要記憶體時，是從 CPU 0 的 Local Memory 割一塊給它**。

#### 4. 系統架構的終極矛盾：Load Balancing vs. Cache/NUMA Affinity
課本最後一段點出了作業系統排程演算法的終極難題：**自然張力 (Natural Tension)**。

1.  **為了系統吞吐量**，Load Balancing 想要把 Thread 搬到閒置的 CPU 上。
2.  **但為了單一 Thread 的極致效能**，Cache Affinity 要求 Thread 留在原本的 CPU 上。
3.  **在 NUMA 架構下**，如果 Load Balancing 把一個原本在 CPU 0 的 Thread 搬到了 CPU 1，但它的資料依然留在 CPU 0 的 Local Memory 裡，這個 Thread 接下來所有的讀寫都會變成 **Slow Access (跨晶片讀取)**，效能不僅沒變快，反而可能垂直崩潰！

因此，現代多核心排程演算法 (如 Linux CFS) 變得極度複雜，必須在這三股拉扯的力量中尋找數學上的最佳平衡點。

---

## Real-Time 排程理論與延遲架構剖析
這段內容正式進入了作業系統中最嚴苛、也與機器人控制 (AMR) 息息相關的領域：**Real-Time CPU Scheduling (即時 CPU 排程)**。

在一般桌上型電腦或伺服器中，排程追求的是「平均吞吐量」或「公平性」；但在即時系統中，排程追求的是**「絕對的時間可預測性 (Predictability)」**與**「極低的延遲 (Latency)」**。

#### 1. 系統架構的生死分界：Soft vs. Hard Real-Time
即時系統並不是「跑得越快越好」，而是「必須在規定的時間內給出答案」。依照時間底線的嚴格程度，架構分為兩類：
* **Soft Real-Time Systems (軟即時系統)**：
    * **定義**：系統會盡全力給予關鍵任務最高的優先權，但**不保證**任務一定能在截止時間 (Deadline) 內完成。
    * **後果**：稍微超時是可以容忍的。例如 AMR 上的遠端影像串流 (Video Streaming) 任務，如果因為排程延遲掉了一兩幀畫面，只會造成畫面些微卡頓，不會引發物理災難。
* **Hard Real-Time Systems (硬即時系統)**：
    * **定義**：任務**絕對必須**在嚴格的截止時間內完成。一旦超時，其計算結果等同於完全無效，甚至會引發災難。
    * **後果**：例如 AMR 底層的防鎖死煞車系統 (Antilock Brake System) 或防墜落紅外線感測。如果偵測到樓梯邊緣，系統必須在 3 到 5 毫秒內鎖死馬達。若延遲達到 10 毫秒才煞車，機器人早已摔落樓梯，造成嚴重的硬體損毀。

#### 2. 效能的死穴：Event Latency (事件延遲 / Figure 5.17)
![image](https://hackmd.io/_uploads/Sy0wB0Jh-x.png)
在事件驅動 (Event-driven) 的架構下，從「真實世界的物理事件發生 (例如感測器偵測到障礙物)」到「作業系統真正輸出控制訊號 (例如馬達煞車)」，這中間所耗費的總時間稱為 **Event Latency**。
為了將這個總延遲壓到最低，系統工程師必須拆解並最佳化底層的兩大延遲來源：**Interrupt Latency** 與 **Dispatch Latency**。


#### 3. 硬體與核心的交界：Interrupt Latency (中斷延遲 / Figure 5.18)
![image](https://hackmd.io/_uploads/Skl98Ryn-g.png)
* **定義**：從「CPU 硬體腳位收到中斷訊號」到「第一行中斷服務常式 (ISR, Interrupt Service Routine) 開始執行」的時間差。
* **底層物理耗損**：當 CPU 收到中斷時，它不能立刻跳轉。它必須先：
    1. 把當下正在執行的那行機器語言指令跑完。
    2. 判斷這是哪一種中斷 (Determine interrupt type)。
    3. 把當前苦主 Process 的所有暫存器狀態存入 PCB (Context Switch 的前半段)。
* **架構師的挑戰**：中斷延遲最大的敵人是「作業系統自己」。當 OS 核心正在更新關鍵的內部資料結構時，為了防止 Race Condition，它會**短暫關閉接收中斷的功能 (Interrupts disabled)**。在硬即時系統中，核心關閉中斷的時間必須被嚴格限制在極短的微秒級別內。

#### 4. 軟體排程的深淵：Dispatch Latency 與衝突階段 (Figure 5.19)
![image](https://hackmd.io/_uploads/HklJjICy2Wg.png)
* **定義**：排程器 (Dispatcher) 停下當前的任務，並將 CPU 控制權正式移交給即時任務 (Real-time process) 所花費的時間。
* **Conflict Phase (衝突階段的兩大障礙)**：
    這是 Dispatch Latency 中最耗時、也最難以預測的階段。要讓高優先權的即時任務上場，系統必須先排除兩個障礙：
    1. **Preemption (搶占)**：如果當下有一個低優先權的任務正在 Kernel Mode (核心模式) 中執行系統呼叫，OS 必須有能力強制中斷它 (這呼應了 5.1 節提到的 **Preemptive Kernel** 的重要性)。如果 OS 是 Nonpreemptive Kernel，即時任務就必須苦等到系統呼叫結束，這對 Hard Real-Time 來說是不可接受的。
    2. **Resource Release (資源釋放)**：這就是前幾節提過的 **Priority Inversion (優先權反轉)** 災難。如果即時任務需要的 Mutex Lock 目前被低優先權任務握在手裡，系統必須啟動 Priority Inheritance (優先權繼承) 機制，逼迫低優先權任務趕快把鎖交出來。

---


## 即時排程理論與週期性任務數學模型

#### 1. 軟即時 (Soft Real-Time) 的底層實作門檻
要讓系統具備最基本的即時響應能力，作業系統排程器**必須**具備兩大特性：**Priority-Based (基於優先權)** 與 **Preemption (搶占式)**。
* **架構邏輯**：如同前面章節所述，系統必須賦予即時任務 (Real-time processes) 絕對最高的優先權。當即時任務進入 Ready Queue 時，如果當下 CPU 正在執行非即時的低優先權任務，排程器必須瞬間觸發 Context Switch 進行搶占。
* **主流作業系統的妥協**：現代通用型 OS (如 Windows, Linux, macOS) 皆內建此機制。例如 Windows 共有 32 個優先權層級，並將最高的 16 到 31 級「硬性保留」給即時任務專用。
* **致命缺陷**：課本明確指出，純粹的「搶占式優先權排程」**只能保證軟即時 (Soft Real-Time)**。它只能保證即時任務會「盡快」被執行，但從數學與系統物理層面上，它**無法保證**任務一定能在絕對的截止時間 (Deadline) 內完成。

#### 2. 硬即時 (Hard Real-Time) 的數學模型：週期性任務 (Periodic Tasks)
為了在數學上保證 100% 不超時，作業系統必須精確掌握物理世界的動態。在機器人控制或工業自動化中，絕大多數的即時任務都是**週期性 (Periodic)** 的（例如：AMR 的 LiDAR 每 50 毫秒會送出一幀點雲資料）。

硬即時排程演算法要求開發者必須精確定義以下三個時間物理量 (參考 Figure 5.20)：
* **$p$ (Period / 週期)**：任務發生的頻率間隔。例如每 50 毫秒觸發一次。任務的觸發頻率 (Rate) 即為 $1/p$。
* **$t$ (Processing Time / 處理時間)**：CPU 執行該任務所需的純運算時間。例如需要 10 毫秒算完點雲。
* **$d$ (Deadline / 截止時間)**：從任務觸發開始算起，必須交付運算結果的最後期限。通常截止時間會等於下一次週期開始之前，但也可能更短。

這三者的物理極限與數學關係必須嚴格滿足：
$$0 \le t \le d \le p$$

#### 3. 系統架構的防波堤：Admission-Control (准入控制)
這是 Hard Real-Time 作業系統與一般 Windows/Linux 最大的差異。
* **一般 OS 的做法**：來者不拒。開越多程式，系統就越卡，每個任務分配到的 CPU 時間越來越少，大家一起變慢。
* **Hard RTOS 的做法 (Admission-Control)**：當一個新的即時任務試圖啟動並向系統宣告它的 $t, d, p$ 需求時，排程器會先進行一場嚴格的數學推演。如果排程器發現接下這個任務，會導致未來系統中「任何一個」任務錯過 Deadline，排程器就會**直接拒絕 (Reject)** 這個新任務的執行請求。
* **架構師哲學**：在硬即時系統中，「寧可一開始就拒絕執行，也絕對不允許答應了卻在期限後才交出結果」，因為遲到的控制訊號（例如晚了 0.1 秒的煞車指令）等同於引發物理災難。

---

**【步驟一暫停點】**

這段內容將排程從單純的「演算法比對」昇華成了「具備嚴格數學證明的資源分配」。

請確認對於 Soft Real-Time 的物理侷限、$t, d, p$ 週期性任務的數學模型，以及 Admission-Control (准入控制) 寧缺勿濫的架構哲學是否已經徹底掌握？

確認無誤後，請給予指示。將進入步驟二，探討在 C++ / ROS 2 架構下，如何精確測量任務的 $t$ (Processing Time)，以及未來會學到哪些具體的 Hard Real-Time 排程演算法 (如 RMS 或 EDF) 來實作准入控制。
## 演算法評估的先決條件與複合指標(5.8)
### 1. 核心痛點：沒有「絕對無敵」的演算法
誠如 5.3 節所見，每一種演算法都有其偏袒的對象。RR 演算法對互動式介面很好，但 Context Switch 耗損極大；FCFS 吞吐量大，但會引發護衛效應。因此，在挑選演算法之前，我們不能只是盲目地套用，而是必須先「定義標準」。
### 2. 架構師的精確定義：複合式評估準則 (Composite Criteria)
回顧 5.2 節，我們學過五大單一指標（CPU 使用率、吞吐量、周轉時間、等待時間、回應時間）。但在實務的系統設計上，我們通常不會只看單一指標，而是會制定帶有「**硬性約束 (Constraints)**」的複合指標。
* **範例一：「在保證最大回應時間不超過 300 毫秒的前提下，最大化 CPU 使用率。」**
    * **實務意義：** 這是典型「互動式系統」的要求。我希望 CPU 盡可能滿載運作（例如在背景瘋狂編譯程式碼），但只要使用者一敲鍵盤或移動滑鼠，系統絕對必須在 0.3 秒內給出畫面回饋。這就要求排程器必須在 RR 與 FCFS 之間找到完美的 Time Quantum 平衡點。
* **範例二：「最大化吞吐量，並確保周轉時間（Turnaround Time）與總執行時間呈現『線性正比』。」**
    * **實務意義：** 這是為了防範「極端不公平」的現象。意思是我希望系統狂解任務，但不能為了衝高吞吐量，就把所有需要算很久的大型任務（CPU-bound）永遠卡在死角（飢餓 Starvation）。它要求：需要算 10 秒的任務，總共花的時間應該大約是算 1 秒任務的 10 倍，而不是被無情地拖延成 100 倍。

### 3. 進入評估方法 (Evaluation Methods)

#### Deterministic Modeling 理論與數學驗證
在定義好上一節的「複合評估指標」後，系統工程師需要科學方法來驗證演算法是否達標。課本介紹的第一種方法即為最基礎的 **Deterministic Modeling (確定性建模)**。
##### 1. 分析評估 (Analytic Evaluation) 的本質
這是一大類評估方法的總稱。核心邏輯是：將「特定的排程演算法」與「特定的系統負載 (Workload)」丟進數學模型中，產出一個具體的公式或數字，藉此客觀評斷效能優劣。
##### 2. 確定性建模的運作邏輯與課本數值驗證
確定性建模要求系統負載必須是**預先決定好且完全固定** (Predetermined) 的死數據。
課本提供了一組包含五個 Process 的極端負載測試：
* 抵達順序與 CPU Burst 時間：$P_1=10$, $P_2=29$, $P_3=3$, $P_4=7$, $P_5=12$。
**for the FCFS algorithm**
![image](https://hackmd.io/_uploads/Hk6v7rKn-x.png)
**for the SJF algorithm**
![image](https://hackmd.io/_uploads/HkR-4Hf2We.png)
**for the RR algorithm**
![image](https://hackmd.io/_uploads/BkNGNSGnZe.png)

將這組固定負載分別套用至三種演算法，計算其平均等待時間 (Average Waiting Time)：
* **FCFS (先到先服務)：** 產生了嚴重的護衛效應，平均等待時間高達 28 ms。
* **Nonpreemptive SJF (非搶占式最短任務優先)：** 將 $P_3, P_4$ 等短任務往前移，平均等待時間大幅壓縮至 13 ms。
* **Round-Robin (Quantum = 10 ms)：** 經過多次 Context Switch 交錯執行，平均等待時間為 23 ms。
* **數據結論：** 在此一特定的固定負載下，SJF 的效能輾壓 FCFS（等待時間不到一半），而 RR 則表現居中。這提供了一個絕對精準的基準線 (Baseline)。

##### 3. 工程實務的應用場景與致命侷限
* **侷限性：** 這些精確的答案「僅適用於」這組特定輸入。在充滿使用者隨機點擊、網路封包不可預測的通用系統 (如桌上型電腦) 中，這種模型幾乎沒有參考價值。
* **最佳應用場景：** 適用於任務完全固定、循環執行的專用系統。在開發自定義微型作業系統 (例如 wuwa-os) 的早期階段，若開機啟動的各項底層任務、驅動程式載入順序與所需時間是固定且可精確測量的，便能完美利用確定性建模，在編譯前就決定出最佳的排程策略。

### Queueing Models 理論與必考數學模型剖析
在通用型作業系統（如桌上型 Ubuntu 或伺服器）中，每天執行的 Process 數量與長度都完全不同。既然無法預知未來，系統架構師就必須借助機率與統計的力量。
##### 1. 從固定數值走向機率分佈 (Probability Distributions)
既然無法確定下一個任務要跑多久，就去測量歷史數據，並將其擬合為機率分佈（通常為指數分佈 Exponential distribution）。在這種分析方法中，電腦系統被抽象化為一個「伺服器網路 (Network of servers)」。
* CPU 是一個伺服器，擁有自己的 Ready Queue。
* 硬碟 (I/O) 也是一個伺服器，擁有自己的 Device Queue。
* 只要掌握了**抵達率 (Arrival rates)** 與 **服務率 (Service rates)**，就能計算出平均佇列長度與等待時間。這門學問被稱為 **Queueing-network analysis (佇列網路分析)**。

##### 2. 🚨 考試絕對重點：Little's Formula (李特爾法則)
這是本節的核心數學公式。在一個處於 **「穩定狀態 (Steady state)」** 的系統中（即進入佇列的任務數量 = 離開佇列的任務數量），以下三個變數存在著絕對的物理張力關係：
> $$n = \lambda \times W$$

必須精確掌握這三個代數的嚴格定義：
* **$n$：長期的平均佇列長度（Average queue length）**。注意考題陷阱：這裡的長度不包含當下正在被 CPU 執行的那個 Process！
* **$\lambda$：任務的平均抵達率（Average arrival rate）**。例如：每秒鐘有 3 個新 Process 進入系統。
* **$W$：一個任務在佇列中的平均等待時間（Average waiting time）**。

##### 3. 課本範例推演
假設系統平均每秒湧入 7 個 Process ($\lambda = 7$)，而系統工程師觀察到 Ready Queue 裡面通常排了 14 個 Process ($n = 14$)。
根據公式 $14 = 7 \times W$，可以直接反推得出：每一個 Process 平均要在佇列裡苦等 2 秒鐘 ($W = 2$) 才能拿到 CPU。
* **這個公式的超能力：** 它無視任何排程演算法與分佈狀態。不管底層是用 RR, SJF 還是 MFQ，只要系統是穩定的，這個等式就絕對成立！

##### 4. 理論與現實的巨大鴻溝 (Limitations)
雖然佇列分析看起來很強大，但課本在最後一段無情地指出了它的極限：
* **數學的殘酷：** 一旦排程演算法變得很複雜（例如加上了老化機制、優先權反轉防護），其背後的微積分與機率方程式會變得根本解不出來 (Difficult to work with)。
* **被迫妥協的假設：** 為了讓數學算得出來，研究人員只好做出許多「不切實際 (Unrealistic)」的假設，例如假設每個 Process 的抵達時間是完全獨立的。
* **結論：** 因為這些妥協，Queueing Models 算出來的數字永遠只是真實系統的近似值 (Approximations)，其精確度在實務開發上往往備受質疑。
#### Simulations 模擬器理論與系統架構剖析
當數學公式 (佇列模型) 因為太過複雜而無法精確求解時，系統架構師會選擇直接用程式碼「打造一個虛擬的電腦系統」。這就是 5.8.3 Simulations (模擬器) 的核心概念。
##### 模擬器的底層運作邏輯
![image](https://hackmd.io/_uploads/HkqUBdfnbx.png)
模擬器內部會宣告一個變數來代表「系統時鐘 (Clock)」。隨著時鐘變數不斷遞增，軟體會精確修改虛擬設備、虛擬 Process 與虛擬排程器的狀態，並在執行過程中沿途收集效能統計數據 (Statistics)。
要讓模擬器動起來，必須餵給它海量的「任務抵達與執行時間」資料。這裡有兩種截然不同的資料提供策略：
* **策略一：Distribution-Driven (機率分佈驅動)**
    * **作法：** 撰寫亂數產生器 (Random-number generator)，依照前一節提到的數學分佈（如均勻分佈、指數分佈、卜瓦松分佈）來無中生有產生假任務。
    * **致命缺陷 (失去時序關聯性)：** 純粹的機率分佈只知道「發生的次數與長度」，卻無法反映真實系統中事件發生的「順序 (Order)」。在真實系統裡，十個連續的 I/O 密集型任務可能會引發嚴重的連鎖效應，但亂數產生器可能會把這十個任務均勻打散在時間軸上，導致模擬結果呈現出一片祥和的假象。
* **==策略二：Trace Files (軌跡檔 / Figure 5.31)==**
    * **作法：** 直接在「真實的電腦系統」上掛載監控程式，將系統一整天真實發生的 每一次 CPU Burst、每一次 I/O 請求的**精確發生時間與長度**，**按照順序毫秒不差地「紀錄」下來**，**存成一個巨大的文字檔**。
    * **絕對客觀的對照實驗 (Figure 5.31 解析)：** 將這份錄滿真實血淚的 Trace tape，同時餵給 FCFS、SJF、RR 的模擬器。因為這三套演算法面對的是「絕對一模一樣」的極端狀況、爆發順序與任務負載，最終產出的效能數據 (Performance statistics) 具有無可挑剔的比較意義。這就是標準的 Apples-to-apples comparison。
    * **物理代價 (Expensive)：** 完美是有代價的。Trace files 往往極度龐大，佔用海量儲存空間；且開發一個不會崩潰、邏輯嚴密的模擬器本身就是一項艱鉅的軟體工程任務。跑一次數十 GB 的軌跡檔模擬，可能需要耗費真實伺服器好幾個小時的運算時間。
#### 5.8.4 Implementation (實作)
當數學公式（佇列模型）與虛擬環境（模擬器）都無法滿足效能評估的精度要求時，系統工程師必須面對最終的物理現實：
##### 1. 真實環境的物理代價與防護網
將全新的排程演算法編譯進作業系統核心（Kernel），是風險極高的一件事。
* **測試環境隔離：** 實作初期絕對不能在專用實體硬體上直接跑，必須先在虛擬機 (Virtual Machines) 中進行測試。
* **迴歸測試 (Regression Testing)：** 這是軟體工程的核心防線。當排程器程式碼被替換後，系統必須重新跑過成千上萬個歷史測試案例，證明：
    1. 新演算法沒有把原本正常的系統搞壞。
    2. 新演算法沒有把以前修好的舊 Bug（例如某種特定條件下的 Deadlock）重新放出來。
##### 2. 架構師的盲區：使用者與程式的「作弊行為」
這是實作階段最常遇到、但在數學模型中絕對算不出來的問題：環境會隨著排程器改變而改變。
* **規則改變生態：** 如果系統規定「只要是互動式任務 (Interactive processes，例如有接收鍵盤輸入或螢幕輸出的程式)，就能獲得最高優先權」。
* **惡意鑽漏洞 (Circumvent scheduling algorithms)：** 底層的應用程式開發者為了讓自己的 CPU-bound 巨獸程式跑得更快，會刻意在程式碼裡加上「每秒鐘向終端機輸出一個無意義字元」的廢指令。這個動作成功騙過了作業系統，讓該程式被誤判為互動式任務，進而霸佔系統最高優先權。
* **結論：** 排程器的設計必須防範這種「惡意對抗」，不能依賴過於簡單的分類標籤。

##### 3. 現代作業系統的最終解答：參數化與 API 授權
既然沒有任何一種演算法能完美適應從「高階圖形工作站」到「背景網頁伺服器」的所有情境，現代 OS (如 Linux, Windows, macOS) 選擇了放權：
* 既然沒有任何一種演算法能完美適應從「高階圖形工作站」到「背景網頁伺服器」的所有情境，現代 OS (如 Linux, Windows, macOS) 選擇了放權：
* **開發者 API：** 提供如 POSIX Thread APIs (pthread_setschedparam)、Java 或 Windows APIs，讓應用程式開發者可以自行修改 Process 或 Thread 的優先權。
* **效能代價：** 課本最後給出了一個工程警語：過度針對單一應用程式進行效能微調 (Performance-tuning)，往往會導致系統在面對其他通用任務時，整體效能嚴重下滑。



