---
title: ch4 Thread

---

# Chapter 4 執行緒 (Threads) 學習藍圖
### 1. 核心觀念的典範轉移 (The Notion of a Thread)
* **核心定義：** 
    * Thread 是 CPU 運作的「基本單位 (Fundamental unit of CPU utilization)」。
* **與 Process 的根本差異：**
    * 在前一章中，Process 被視為一個「擁有獨立記憶體資源的沉重容器」。而在現代作業系統中，這個容器內部真正負責在 CPU 上奔跑、執行機器碼指令的實體，就是 Thread。
* **輕量級的優勢：**
    * 一個 Process 內部可以容納多個 Threads。這些 Threads 會共享所屬 Process 的 Text (程式碼)、Data (全域變數) 與 Heap (動態記憶體) 區段，但各自擁有獨立的 Program Counter (程式計數器) 與 Stack (堆疊)。這使得 Thread 之間的 Context Switch 成本遠低於 Process。

### 2. 關鍵學習路徑 (Chapter Objectives & Roadmap)
1. 多核心程式設計 (Multicore Programming)
    * 探討如何利用 Multithreading 架構榨乾現代多核心硬體的算力。
    * 實務連結：在 AMR 導航專案中，必須確保 `rclcpp::executors::MultiThreadedExecutor` 能夠將感測器資料接收、SLAM 建圖與 Nav2 路徑規劃分配到不同的 CPU 核心上平行處理，避免單一迴圈卡死整個系統。
2. 執行緒模型與 API 函式庫 (Models & Libraries)
    * 剖析 User Space 執行緒如何映射 (Map) 到作業系統底層的 Kernel Space。
    * 學習實務上的三大 API。在 WSL2 Ubuntu 的 C++ 開發環境中，最核心的將會是 **Pthreads (POSIX Threads)**。
3. 隱式執行緒 (Implicit Threading)
    * 探討如何將建立與管理 Thread 的繁雜工作，交由編譯器或底層函式庫代勞（例如 Thread Pools 執行緒池、OpenMP 等機制），以降低程式碼複雜度。
4. 並行開發的地雷區 (Threading Issues)
    * 這是實作中最容易發生系統崩潰的區域。探討當一個擁有多個執行緒的 Process 突然呼叫 `fork()` 或 `exec()` 時，記憶體該如何複製？外部傳來的中斷 Signal 到底該由哪個 Thread 負責接收？
5. 作業系統實例 (OS Examples)
    * 檢視 Linux 與 Windows 核心到底是如何實作這些架構的（例如 Linux 中的 `clone()` 系統呼叫）。
# 多執行緒的動機、架構與四大優勢 (Motivation & Benefits)
### 1. 核心動機：重磅與輕量的對決
在現代軟體開發中，單一應用程式通常需要「同時」處理多項任務。
* **傳統解法 (Heavy-weight)：** 如果依靠前一章的 `fork()` 為每個任務建立新的 Process，作業系統必須為每個 Process 配置獨立的記憶體空間、複製 Page Table，這會消耗大量 RAM 且建立速度緩慢。
* **現代解法 (Light-weight)：** 在單一應用程式 (Process) 內部，建立多個 **Threads (執行緒)**。
    * **實務應用舉例：**
        * **文書軟體：** 一個 Thread 負責接收鍵盤輸入更新畫面，另一個 Thread 在背景執行拼字檢查，第三個負責定時自動存檔。
        * **作業系統核心 (Kernel)：** 現代 OS 本身也是多執行緒的，例如 Linux 核心中會有多個 Kernel Threads 專門負責管理記憶體或處理中斷。
### 2. 多執行緒伺服器架構 (Multithreaded Server Architecture)
![image](https://hackmd.io/_uploads/SyrpsYjjWx.png)
在網路伺服器 (如 Web Server 或 ROS2 的 Service Server) 的設計上，Thread 提供了極高效率的併發 (Concurrency) 模型：
* **Dispatcher / Worker 模式：**
    1. 伺服器主執行緒 (Main Thread) 專門死守在 Port 上 監聽 (Listen) 客戶端的 Request。
    2. 一旦收到請求，主執行緒不會親自處理，而是立刻 建立 (Create) 一個新的 Worker Thread 將任務交接出去。
    3. 主執行緒瞬間回到監聽狀態，準備迎接下一個連線。這確保了伺服器永遠不會因為處理單一使用者的複雜運算，而導致後續連線全部塞車逾時 (Timeout)。

### 3. 導入 Thread 的四大核心優勢 (Benefits)
將系統改寫為多執行緒架構，可以帶來四個維度的顯著提升：
1. **Responsiveness (高響應性)：**
    * 即使程式的某個部分卡住了 (Blocked，例如等待網路封包或硬碟讀取) 或是正在執行耗時運算，其他 Threads 依然可以繼續執行。這對於包含使用者介面 (UI) 的程式至關重要，能避免出現「程式無回應」的凍結狀態。
2. **Resource Sharing (資源共享)：**
    * 同一個 Process 底下的所有 Threads，天生就共享 Text (程式碼區段) 與 Data (全域變數區段)。
    * **優勢：**
        * 完全不需要像 Chapter 3 那樣辛苦地設定 Shared Memory 或 Message Passing IPC，多個 Thread 可以直接讀寫同一個全域變數來交換資料。
3. **Economy (經濟性 / 低效能損耗)：**
    * **建立成本低：**
        * 分配一個 Thread 的資源遠比建立一個 Process 便宜。
    * **切換成本極低 (Lower Context Switching Overhead)：**
        * 這是效能關鍵！在切換 Thread 時，因為它們共用同一塊記憶體位址空間，作業系統不需要清空並重建快取 (Cache) 與 TLB (Translation Lookaside Buffer)。

4. **Scalability (可擴展性 / 支援多核心)：**
    * 在單核心系統上，多執行緒只是在快速切換製造「同時」的錯覺 (Concurrency)；但在多核心硬體 (Multiprocessor architectures) 上，作業系統可以將不同的 Threads 真正分配到不同的實體核心上同時平行執行 (Parallelism)，徹底榨乾硬體算力。

# Multicore Programming (多核心程式設計)
![image](https://hackmd.io/_uploads/rJctX2jsWl.png)
### 1. 核心觀念的絕對分水嶺：Concurrency vs. Parallelism
在計算機科學理論中，這兩個名詞有著嚴格的物理層面差異：
* **Concurrency (併發)：**
    * **定義：** 系統支援多個任務「交替」前進 (making progress)。
    * **硬體條件：** 在單一 CPU 核心 (Single processor / core) 上即可實現。完全依賴底層排程器 (Scheduler) 快速進行 Context Switch，製造出多個任務同時運作的「錯覺」。
* **Parallelism (平行)：**
    * **定義：** 系統能在「同一個物理時間點」執行多個任務 (simultaneously)。
    * **硬體條件：** 必須具備多核心 (Multicore) 或多處理器架構。

### 2. 兩種平行化架構策略 (Types of Parallelism)
當硬體擁有多顆核心時，軟體架構師可以從兩種維度來分配運算資源：
* **Data parallelism (資料平行化)：**
    * **邏輯：** 任務 (程式碼) 相同，但把**巨大的資料切塊**分給不同核心。
    * **實例：** 在處理感測器數據時，將龐大的 2D LiDAR 點雲陣列平均切成四等份，交由四個 CPU 核心「同時執行相同的濾波演算法」。
* **Task parallelism (任務平行化)：**
    * **邏輯：** 資料不一定相關，把不同的任務 (功能) 分配給不同核心。
    * **實例：** 在解耦的 AMR 專案架構中，Core 1 專職負責大腦決策層 (狀態機與任務排程)，Core 2 負責小腦導航層 (Nav2 與 Smac Planner 演算法)，Core 3 負責與底盤進行實體層通訊。

### 3. 多核心開發的五大工程挑戰 (Programming Challenges)
享受多核心效能的同時，系統工程師必須親自處理以下五個嚴峻的挑戰：
1. **Dividing activities (切割任務)：** 如何將單一龐大的程式，解耦成能獨立運作的多個 Thread。
2. **Balance (負載平衡)：** 必須確保任務分配均勻，避免發生 Core 1 運算到過熱，而 Core 2 卻長期處於閒置 (Idle) 的效能浪費。
3. **Data splitting (資料分割)：** 如何精準切割記憶體資料，確保平行處理的範圍不重疊。
4. **Data dependency (資料依賴)：** 最危險的痛點。當任務 B 必須等待任務 A 的運算結果才能執行時，若沒有嚴謹的同步管制 (如 Mutex)，將導致嚴重的 Race Condition 或是 Deadlock (死結)。
5. **Testing and debugging (測試與除錯)：** 多執行緒的 Bug 通常極度難以捕捉 (海森堡 Bug)。因為每次 CPU 排程器切換 Thread 的時間點都不同，導致錯誤無法穩定重現。這也是為何在開發環境遇到問題時，必須嚴格執行檢查 C++ 編譯警告與 RCLCPP_ERROR 日誌的 SOP，而非單靠除錯器單步追蹤。

### 4. 硬體架構的演進 (Hardware Threads)
* 隨著軟體對 Thread 的需求暴增，硬體層面也提供了支援。現代 CPU 除了實體核心 (Cores) 外，還具備 **Hardware threads (硬體執行緒/邏輯處理器)** 技術 (例如 Intel 的 Hyper-Threading)。
* 講義範例：Oracle SPARC T4 具備 8 個實體核心，每個核心支援 8 個硬體執行緒，因此作業系統層面會看到 $8 \times 8 = 64$ 個邏輯處理器可供排程。
# ==Thread的觀念==
![image](https://hackmd.io/_uploads/ByCOv0iobx.png)
Thread 既不是實體的硬體，也不是靜態的程式碼。如果 Process 的定義是 "A program in execution"（執行中的程式），那麼 Thread 最精準的定義就是：**"A single sequential flow of control within a process"**（行程內部一條獨立的控制序列）。

### 1. 作業系統視角：Thread 本質上就是一個「資料結構 (TCB)」
![image](https://hackmd.io/_uploads/SJvvUCsobe.png)
在 OS 核心的原始碼中，Thread 其實就是一個輕量級的 `struct`，稱為 **TCB** (Thread Control Block, 執行緒控制表)。當作業系統在排程 CPU 時，它真正放上 CPU 去跑的其實是 TCB，而不是 PCB。一個 TCB 裡面「只」打包了三樣維持 CPU 運作的最低限度資源：
1. **Program Counter (PC, 程式計數器)：** 一個指標，紀錄這個 Thread 下一瞬間要讀取 Text Section (程式碼區段) 裡的哪一行機器碼。
2. **CPU Registers (暫存器狀態)：** 當這個 Thread 被踢下 CPU 時，原本算到一半的暫存資料（加減乘除的結果）會保存在這裡。
3. **Stack (堆疊)：** 一塊專屬的記憶體區域，用來儲存這個 Thread 自己呼叫的 Local Variables (區域變數) 與 Function Return Addresses (函式回傳位址)。

**結論：** Thread 具體來說，就是「一套 PC + Registers + Stack 的組合包」。它本身沒有自己的倉庫 (Heap) 和藍圖 (Text)，只能「寄生」在 Process 的記憶體空間裡運作。

### 2. 系統實作視角：Thread 是怎麼造出來的？
在 Linux 底層，無論是建立 Process 還是 Thread，最終都會呼叫同一個終極的 C 語言系統呼叫：`clone()`。它們的差異僅在於傳入的參數 (Flags) 不同：
* **造 Process (`fork()`)：**
    * 底層呼叫 `clone(SIGCHLD)`。OS 會完整複製一份父行程的記憶體、Page Table 和檔案描述符，建立一個全新且隔離的 PCB。這個動作極度耗時。
* **造 Thread (`pthread_create()`)：**
    * 底層呼叫 `clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)`。
    * `CLONE_VM`：告訴 OS「不要複製記憶體，直接讓新誕生的 TCB 指向原本 Process 的記憶體空間」。
    * OS 只需要在 Kernel 裡 `malloc` 一小塊空間來存放 TCB，並在原本的記憶體中劃出一塊新的 Stack 區段給這個新 Thread 用。
    * 這個新建的 TCB 會被直接丟進 Ready Queue 等待 CPU 排程。
**結論：** 造 Thread 的過程，就是 OS 建立了一個新的 TCB 資料結構，並將其記憶體指標「綁定 (Bind)」到現有的 Process 空間上，成本極低。

### 3. CPU 實體視角：Thread 是怎麼運作的？
CPU 是一顆沒有靈魂的運算晶片，它根本不知道什麼是 Process 或 Thread。CPU 唯一會做的事情，就是去讀取硬體暫存器裡 **Program Counter (PC)** 所指向的記憶體位址，把那行二進位指令抓出來執行。

當 OS 排程器將不同的 Thread 切換上 CPU 時：
* 其實就只是把不同 TCB 裡的 Program Counter 數值，塞進 CPU 的硬體暫存器裡。
* 如果塞入的是 Thread A 的 PC，CPU 就去執行網路封包接收的程式碼。
* 下一毫秒塞入 Thread B 的 PC，CPU 就去執行 VSLAM 矩陣運算的程式碼。

### 4. 總結
* **Process 是一個沉重的「資源容器」**，**負責向系統圈地**（要記憶體、要檔案權限）。
* Thread 是這個容器內部的「動態執行軌跡」，是一個由 TCB (PC + Stack + Registers) 構成的輕量級資料結構。
* 怎麼造出來的：透過呼叫底層 API (`clone()`)，**要求 OS 建立一個與原 Process 共享同一塊實體記憶體的 TCB**。

# Amdahl's Law (平行化效能極限定律)
### 1. 核心定義與數學公式
在任何一個軟體應用程式中，程式碼必定可以被拆分為兩個部分：
* **$S$ (Serial portion)：** 絕對無法平行化，必須具有嚴格先後順序的循序程式碼比例。
* **$(1-S)$ (Parallel portion)：** 資料與邏輯互不依賴，可以完美分散給多個核心同時執行的程式碼比例。
當系統具備 $N$ 顆處理器核心 (Processing cores) 時，整體系統的加速比 (Speedup) 理論上限為：
* $$speedup \le \frac{1}{S + \frac{(1-S)}{N}}$$

### 2. 無限核心的極限推導
Amdahl's Law 最核心的警告在於探討「極限值」。假設將該系統部署到擁有無限多顆核心的超級叢集上 ($N \to \infty$)，公式分母的 $\frac{(1-S)}{N}$ 將趨近於 $0$。

最大理論加速比將被徹底鎖死：
    $$\lim_{N \to \infty} speedup = \frac{1}{S}$$

**工程結論：** 承接上述 $S = 0.25$ 的範例，$\frac{1}{0.25} = 4$。這意味著，只要系統架構中有 25% 的程式碼是循序且無法解耦的，那麼就算給予一萬顆 CPU 核心，這套系統的極限運作速度永遠不可能超過單核心的 4 倍。循序區段 (Serial portion) 對於整體效能具備不成比例的巨大影響力 (Disproportionate effect)。

### 3. 實務開發的底層思維 (結合 C++ 與 ROS 2 AMR 專案)
在設計高可移植性的 AMR (Autonomous Mobile Robot) 軟體架構時，Amdahl's Law 是系統解耦的最高指導原則。

**效能瓶頸點：** 若系統中 LiDAR 點雲的接收、座標轉換 (TF Tree) 與 Nav2 局部路徑規劃可以透過 `MultiThreadedExecutor` 完美分配給多核心平行運算；但最終的「大腦狀態機 (State Machine) 決策層」卻被寫成了一個巨大且不可分割的 `while` 迴圈 (即 $S$)。

**硬體限制：** 最終若要將此專案部署至算力受限的樹莓派 (Raspberry Pi) 上，無論樹莓派未來升級到多少核心，只要不重構並極小化決策層的 $S$ 佔比，系統的整體導航頻率與反應延遲，永遠會被那段循序執行的狀態機給卡死。這也是為何高內聚、低耦合的非同步設計在機器人工程中如此致命。

# 執行緒陣營與映射模型
### 1. 執行緒的兩大陣營 (User Threads vs. Kernel Threads)
在作業系統架構中，執行緒依據其「管理者」與「執行空間」被嚴格分為兩個層級：
* **User Threads (使用者等級執行緒)**
    * **管理者：** 完全由 ==User Space== (使用者空間) 中的 Thread Library (執行緒函式庫) 來進行建立、排程與銷毀。
    * **系統感知度：** 底層的 OS Kernel 完全不知道這些 Thread 的存在。對 OS 而言，它只看到一個單一的 Process。
    * **常見函式庫：** POSIX Pthreads (Linux/UNIX 系統開發最核心的標準)、Windows threads、Java threads。
    * **架構特性：** 因為不需要進行 System Call 切換至 Kernel Mode，所以建立與切換的速度極快。

### 2. 多執行緒映射模型概覽 (Multithreading Models)
既然系統中同時存在「應用程式自己建立的 User Threads」以及「底層硬體真正在跑的 Kernel Threads」，系統架構師就必須決定這兩者之間的對應關係 (Mapping)。

這直接決定了系統能否真正發揮多核心平行運算的威力。根據對應方式，作業系統理論將其劃分為三大經典模型：
1. Many-to-One 模型 (多對一)：多個 User Threads 綁定到單一個 Kernel Thread 上。
2. One-to-One 模型 (一對一)：每一個 User Thread 都精準對應到一個專屬的 Kernel Thread。(這是現代 Linux 與 Windows 的預設實作方式)。
3. Many-to-Many 模型 (多對多)：多個 User Threads 動態對應到多個 Kernel Threads 上。

這牽涉到系統開發中最致命的 Blocking I/O (阻塞式輸入/輸出) 問題。如果應用程式使用的是純 User Threads，當其中一個 Thread 因為等待網路封包而卡住 (Blocked) 時，因為 OS Kernel 只看得到一個 Process，它會毫不留情地把整個 Process 從 CPU 上拔除。這會導致該程式內部其他所有明明可以執行的 User Threads 一起跟著陪葬，瞬間失去併發能力。映射模型 (Multithreading Models) 的設計，正是為了解決這類效能瓶頸。

### 3. Many-to-One Multithreading Model
![image](https://hackmd.io/_uploads/rk36OG3jWg.png)
#### 1. 架構定義與核心機制
* **定義：** 在這種架構下，應用程式在 User Space 建立的「多個」User-level Threads，在底層全部被綁定、擠在「單一個」Kernel Thread 身上。
* **管理權限：** 執行緒的建立、排程、同步與銷毀，**100% 由 User Space 的 Thread Library** (執行緒函式庫) 負責。作業系統核心 (Kernel) 完全不插手，也完全不知道這些 User Threads 的存在。**對 OS 而言，它只看到一個標準的 Process 帶著一個 Kernel Thread**。

#### 2. 系統架構師視角：兩大致命缺陷
在現代多核心架構下，Many-to-One 模型幾乎已被完全淘汰 (Few systems currently use this model)。投影片中精準點出了它的兩個致命傷：
* **致命缺陷一：** 一人卡死，全軍覆沒 (One thread blocking causes all to block)
    * **底層邏輯：** 因為 OS Kernel 的視角極度受限，它只看得到那「唯一的 Kernel Thread」。當其中一個 User Thread 呼叫了 Blocking I/O (例如等待硬碟讀取、等待網路封包) 時，OS 會毫不猶豫地將該 Kernel Thread 標記為 Blocked (等待狀態)，並剝奪其 CPU 執行權。
    * **系統災難：** 即使該 Process 內部還有其他 99 個不需要等待 I/O、正準備進行高強度運算的 User Threads，也會因為底層唯一的載具 (Kernel Thread) 失去動力，而跟著被強迫停擺，徹底失去併發 (Concurrency) 能力。
* **致命缺陷二：** 無法發揮多核心算力 (No True Parallelism)
    * **底層邏輯：** 回顧前面 Concurrency vs. Parallelism 的觀念。因為底層只有一個 Kernel Thread，而 OS 排程器分配 CPU 資源的最小實體單位就是 Kernel Thread。這意味著在任何一個物理時間點，這個 Process 最多只能被分配到「一顆」CPU 核心上執行。
    * **系統災難：** 就算將系統部署到擁有 8 核心的硬體上，這 8 個核心也只有 1 個會工作，其餘 7 個完全無法幫忙分擔這個 Process 內部的其他 User Threads。

#### 3. 替代視角：為何早期系統要採用此模型？
* **極致的切換效能：** 在早期的「單核心 CPU」時代，本來就不可能達成實體層面的 Parallelism。在這種模型下，所有的 Thread 切換都在 User Space 完成，**完全不需要觸發硬體中斷**、**也不需要進行耗時的 Context Switch 切換至 Kernel Mode**。因此，它的切換速度是三種模型中最快的，非常適合算力極度匱乏的早期單核環境。

### 4. One-to-One Multithreading Model (一對一模型)
![image](https://hackmd.io/_uploads/Byrwtmhi-x.png)
#### 1. 架構定義與核心機制
* **定義：** 每一個 User-level Thread 都精準地映射 (Map) 到一個專屬的 Kernel Thread。
* **底層行為：** 當應用程式呼叫 API (如 C++ 的 std::thread 或 pthread_create) 在 User Space 建立一個 Thread 時，作業系統核心必定會同步在 Kernel Space 建立一個對應的 Kernel Thread 作為載具。

#### 2. 系統架構師視角：解決痛點與獲得真正平行化
* **消弭阻塞災難 (More concurrency)：**
    * 這是最關鍵的升級！若其中一個 Thread 因為執行 System Call (如等待網路封包) 而進入 Blocked 狀態，作業系統只會凍結那個特定的 Kernel Thread。同一 Process 內的其他 Thread 依然擁有各自的 Kernel Thread 載具，可以毫不受阻地繼續執行。
* **釋放多核心算力 (True Parallelism)：**
    * 因為 OS 排程器看得到多個獨立的 Kernel Thread，它可以將這些 Thread 打散分配到多顆實體 CPU 核心上同時運作。
    * **實務連結：** 在 WSL2 Ubuntu 上開發 AMR 專案時，若 2D LiDAR 節點因等待 Serial Port 而阻塞，負責 Nav2 導航與狀態機決策的節點絕對不會因此停擺，因為 Linux 底層預設採用的正是 One-to-One 模型。

#### 3. 效能代價 (Trade-off) 與系統限制
天下沒有白吃的午餐，獲得了極致的併發能力，就必須付出系統層面的代價：
* **核心建立成本 (Overhead)：** 每次在 User Space 建立或銷毀 Thread 時，都必須透過 System Call (系統呼叫) 越界進入 Kernel Mode，要求作業系統配置或回收底層的 Kernel Thread 資源。這種 Mode Switch (模式切換) 以及核心介入處理的開銷相當昂貴。
* **數量限制 (Restricted capacity)：** Kernel Thread 的建立需要消耗核心記憶體空間。若一個程式無節制地開啟數千個 Thread，不僅記憶體會爆滿，OS 排程器在切換這成千上萬個載具時所產生的純耗損 (Overhead) 也會直接拖垮系統效能。因此，作業系統通常會對單一 Process 的 Thread 數量設定硬性上限。

#### 4. 現代作業系統的主流標準
目前幾乎所有主流的通用作業系統，包含 Linux、Windows 以及 Solaris 9 之後的版本，皆全面採用此模型作為預設的架構。

### 5. Many-to-Many Multithreading Model (多對多模型)
![image](https://hackmd.io/_uploads/Sk3WhQ3oWe.png)
#### 1. 架構定義與 LWP 仲裁層
* **定義：** 允許開發者建立任意數量的 User Threads，並將其動態映射 (Multiplexing) 到數量相等或較少的 Kernel Threads 上。
* **==LWP (Lightweight Process) 的角色：==**
    * LWP 是安插在 User Thread 與 Kernel Thread 之間的中介資料結構。
    * **User Space 視角：** 執行緒函式庫 (Thread Library) 將 LWP 視為可以排程的「虛擬處理器」。函式庫負責決定要把哪個 User Thread 放到哪個 LWP 上執行。
    * **Kernel Space 視角：** 每一個 LWP 底層都死綁著一個實體的 Kernel Thread。作業系統核心只負責將 Kernel Thread 排程到實體 CPU 核心上。

#### 2. 系統架構師視角：理論上的完美折衷
這個模型誕生是為了解決前面兩種模型的所有痛點，達成理論上的最優解：
* **解決「多對一」的阻塞問題：** 如果某個 User Thread 執行了 Blocking I/O，導致其所在的 LWP 與 Kernel Thread 被作業系統凍結，執行緒函式庫可以立刻將其他未阻塞的 User Threads 重新調度到其他剩餘的 LWP 上，繼續平行運作。
* **解決「一對一」的資源耗盡問題：** 開發者可以毫無顧忌地建立成千上萬個 User Threads 來處理邏輯，而不需要擔心耗盡核心記憶體。因為作業系統只需建立「足夠數量 (Sufficient number)」的 Kernel Threads（通常等同於硬體 CPU 核心數，或依據應用程式需求動態調整）。

#### 3. 實務發展與逐漸沒落
* **高度複雜性：** 儘管理論完美，但要維持 User Space 排程器與 OS Kernel 排程器之間的狀態同步，實作難度與維護成本極高。
* **硬體演進的淘汰：** 隨著現代硬體資源（記憶體與 CPU 核心數）變得極度廉價，One-to-One 模型中「Kernel Thread 數量受限」的缺點已經不再是嚴重問題。
* **現代 OS 選擇：** 如投影片所示，Solaris 在第 9 版之後已經放棄此模型。現代多數系統（Linux、現代 Windows）皆全面轉向單純的 One-to-One 模型。在 Windows 中若要使用此架構，必須額外手動載入 ThreadFiber 套件來實作 (Fiber 即為 Windows 中的 User-level thread 稱呼)。

# Thread Libraries (執行緒函式庫實作機制)
### 1. 函式庫的核心定位
* **定義：** 提供應用程式設計師一組標準化的 API，用來建立 (Create)、同步 (Synchronize) 與銷毀 (Destroy) 執行緒。
* **開發視角：** 在 C++ 中，無論是直接引入 `<pthread.h>` 或是使用 C++11 標準的 ``<thread>``，底層都是在呼叫特定的 Thread Library。

### 2. 兩種底層實作途徑 (Primary ways of implementing)
這兩種實作方式，完美對應了前一個主題所探討的映射模型 (Multithreading Models) 的底層支援邏輯：
* **途徑一：** 完全在使用者空間 (Library entirely in user space)
    * **運作機制：** 該函式庫所有的程式碼與資料結構 (如儲存 TCB 的陣列) 都存放在 User Space。
    * **系統呼叫特性：** 呼叫這個函式庫的 API (例如建立一個 Thread) 時，只會發生本地端的 C++ 函式呼叫 (Local function call)，絕對不會觸發 System Call，也不會發生 Mode Switch 進入作業系統核心。
    * **架構對應：** 這正是 Many-to-One 模型的標準實作方式。速度極快，但受限於 OS 看不到這些 Threads，容易引發全域阻塞災難。
* **途徑二：** 核心等級函式庫 (Kernel-level library supported by the OS)
    * **運作機制：** 該函式庫的程式碼與資料結構存在於 Kernel Space，由作業系統直接提供支援。
    * **系統呼叫特性：** 呼叫這個函式庫的 API 時，通常會直接觸發 **System Call** (例如 Linux 底層的 `clone()`)，要求作業系統核心親自介入處理。
    * **架構對應：** 這正是現代 **One-to-One 模型**的標準實作方式。

### 3. 實務開發連結 (WSL2 Ubuntu 平台)
在目前預設的 WSL2 Ubuntu 24.04 開發環境中，Linux 系統預設採用的是 **NPTL (Native POSIX Thread Library)**。
* NPTL 屬於上述的「途徑二 (Kernel-level library)」。
* 這確保了在開發 ROS 2 Jazzy 專案時，若使用 `std::thread` 或 `MultiThreadedExecutor` 分配節點任務，底層都會精準對應到獨立的 Kernel Thread，完美發揮硬體的多核心算力並避免 I/O 阻塞牽連。

### 4. 實作舉例
提供在 Linux (如 WSL2 Ubuntu) 環境中最核心、最標準的 C 語言執行緒函式庫：POSIX Threads (Pthreads) 的具體實作範例。這段程式碼完美展現了前面理論探討中，主執行緒 (Main Thread) 建立工作執行緒 (Worker Threads) 並等待其完成的標準生命週期。
# POSIX Threads (Pthreads) 實作剖析
### 1. 標準 Pthreads 程式碼實作
以下為講義中計算 $1$ 到 $N$ 總和的完整 C 語言實作。這段程式碼展示了主執行緒如何將運算任務「外包」給 Worker Thread。
```=c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* 【資源共享區】 
 * 宣告在全域的變數存在於 Data Section。
 * Process 內的所有 Threads 都能直接讀寫此變數。
 */
int sum; 

/* 【執行緒任務進入點】 
 * 必須符合簽名：void *function_name(void *param)
 */
void *runner(void *param) {
    int i, upper = atoi(param); // 將傳入的 void* 參數轉回字串並解析為整數
    sum = 0;
    
    // 執行運算任務
    for (i = 1; i <= upper; i++) {
        sum += i;
    }
    
    // 乾淨退出執行緒，並可回傳狀態碼 (此處傳回 0 表示成功)
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    pthread_t tid;       // 執行緒識別碼 (Thread ID)
    pthread_attr_t attr; // 執行緒屬性設定 (如 Stack size)

    // 防呆機制：檢查是否輸入了正確的參數數量與數值
    if (argc != 2) {
        fprintf(stderr, "用法: ./a.out <整數數值>\n");
        return -1;
    }
    if (atoi(argv[1]) < 0) {
        fprintf(stderr, "%d 必須 >= 0\n", atoi(argv[1]));
        return -1;
    }

    /* 步驟一：取得系統預設的執行緒屬性 */
    pthread_attr_init(&attr);

    /* 步驟二：建立並啟動執行緒
     * 參數依序為：(&識別碼, &屬性, 任務函式指標, 傳給任務的參數)
     */
    pthread_create(&tid, &attr, runner, argv[1]);

    /* 步驟三：同步等待
     * 主程式執行到此處會進入 Blocked 狀態。
     * 直到 tid 該執行緒運算完畢並呼叫 pthread_exit() 後，才會解除阻塞。
     */
    pthread_join(tid, NULL);

    // 印出由 Worker Thread 寫入的全域變數結果
    printf("sum = %d\n", sum);
    
    return 0;
}
```
### 2. 多執行緒陣列管理
當系統需要同時派發大量任務時，系統工程師會宣告 `pthread_t` 陣列來批次建立與回收執行緒。
```=c
#define NUM_THREADS 10
pthread_t workers[NUM_THREADS];

// 1. 批次建立 10 個執行緒 (假設任務與參數已設定)
for (int i = 0; i < NUM_THREADS; i++) {
    pthread_create(&workers[i], NULL, runner, &some_args[i]);
}

// 2. 批次回收 10 個執行緒
// 主執行緒會依序等待 workers[0] 到 workers[9] 結束
for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(workers[i], NULL);
}
```

### 3. 系統底層危機：Race Condition (競爭危害)
如果將程式碼改為「同時啟動 10 個執行緒，並且這 10 個執行緒都在執行 sum += i;」，最後印出來的 sum 數值絕對會是錯的，而且每次執行的結果都不一樣。這個現象在作業系統理論中稱為 **Race Condition (競爭危害)**。

**為什麼會算錯？ (底層組合語言剖析)**
在 C 語言中看起來只有一行的 `sum += i`;，在編譯成機器碼後，CPU 實際上必須分「三個獨立步驟」來執行：
1. **Read (讀取)：** 將全域變數 `sum` 的值從記憶體讀進 CPU 暫存器 (Register)。
2. **Modify (修改)：** 在暫存器內將數值加上 `i`。
3. **Write (寫回)：** 將暫存器內的新數值寫回記憶體的 `sum` 位址。
這種無法在單一 CPU 時脈週期內完成的操作，稱為 Non-atomic operation (非原子操作)。


**災難發生的瞬間 (Context Switch 交錯)：**
假設目前 sum = 100。Thread A 準備加 1，Thread B 準備加 2。
* Thread A 執行了 Read，拿到 100 放在自己的暫存器裡。
* 此時 OS 排程器突然介入，將 Thread A 切換下 CPU (Context Switch)，換 Thread B 上場。
* Thread B 執行了 Read，因為記憶體還沒被更新，它拿到的也是 100。
* Thread B 執行 Modify (100 + 2 = 102) 與 Write。此時記憶體 sum = 102。
* OS 排程器再次介入，換 Thread A 恢復執行。
* Thread A 恢復它先前的狀態，執行 Modify (它的暫存器裡是 100，100 + 1 = 101) 與 Write。
* Thread A 將 101 寫回記憶體。Thread B 剛剛辛苦算出來的 102 被徹底覆蓋與抹除了！

### 4. 系統工程師的標準解法：Mutual Exclusion (互斥鎖)

在開發 ROS 2 的 AMR 專案時，只要有多個節點 (Thread) 同時讀寫同一個資源 (如 TF Tree 或地圖陣列)，都必須嚴格引入同步機制。

最標準的解法是使用 **Mutex (互斥鎖)**。在執行 `sum += i`; 之前，執行緒必須先搶到鎖 (Lock)，確保同一時間只有自己能進入這段「Critical Section (臨界區段)」執行那三個底層步驟。算完寫回記憶體後，再把鎖解開 (Unlock) 讓別人進來。這將是作業系統 Chapter 5 的核心重點。
# Implicit Threading (隱式執行緒與自動化平行)
### 1. 核心動機：為何需要 Implicit Threading？
* **開發痛點：** 手動管理數十個 `pthread_t` 陣列、處理精準的 `join` 同步與預防 `Race Condition`，程式碼會變得極度複雜且難以維護 (Program correctness more difficult)。
* **典範轉移：** 將執行緒的「建立 (Creation)」與「管理 (Management)」職責，從應用程式碼中抽離，轉交給**編譯器 (Compilers) 或執行時期函式庫 (Run-time libraries)** 來負責。
* **主流實作方法：** 投影片接下來探討的 Thread Pools、OpenMP，以及蘋果系統的 GCD (Grand Central Dispatch)，皆是隱式執行緒的具體實作。

### 2. Thread Pools (執行緒池)：效能與資源的守門員
這正是為了解決前述 One-to-One 模型中「Mode Switch 建立成本昂貴」與「數量無限制導致系統崩潰」的標準工程解法。
* **運作機制：** 在 Process 啟動的初始化階段，預先向作業系統申請建立一定數量 (A number of threads) 的 Worker Threads，並把它們放在一個「池子 (Pool)」裡處於閒置/等待狀態 (Await work)。
* **三大架構優勢：**
    1. **極致的響應速度 (Faster)：** 當新任務抵達時，直接喚醒池內現有的 Thread 來處理，完全省去了呼叫 `clone()` 與 Mode Switch 建立新載具的昂貴開銷。
    2. **資源受控 (Bound to pool size)：** 嚴格限制系統中同時活躍的 Thread 數量，防止過多並發導致記憶體耗盡或排程器效能低落。
    3. **任務解耦 (Separating task from mechanics)：** 將「要做什麼任務」與「如何建立執行緒」徹底分開。
* 實務應用：在 ROS 2 架構中，處理 Node 生命週期與 Callback 函數的 `MultiThreadedExecutor`，底層就是一個極度強健的 Thread Pool 實作。

### 3. OpenMP：C/C++ 的編譯器平行化外掛
如果說 Thread Pool 偏向「任務平行化 (Task Parallelism)」，那麼 OpenMP 就是處理「資料平行化 (Data Parallelism)」的最強大武裝。
* **核心定義：** 一套跨平台的 C/C++ 編譯器指令 (Compiler directives) 與 API 集合。只要引入 `<omp.h>` 並在編譯時加上特定旗標，就能輕易啟動多核心運算。
* **無痛平行化 (Parallel Regions)：**
    * 不需要像 Pthreads 那樣手動打包 `void*` 參數或宣告獨立的任務函式。
    * 只要在一段龐大的 `for` 迴圈前加上一行編譯器指令 `#pragma omp parallel for`。
    * **底層魔法：** GCC 編譯器在編譯階段看到這行指令時，會自動偵測當前硬體的 CPU 核心數 (Cores)，在底層自動生成對應數量的執行緒，並將迴圈的迭代次數 (例如 0 到 999) 均勻切割分配給這些核心同時並行處理。
* **實務應用：** 在處理不依賴 GPU 的 2D LiDAR 點雲陣列濾波，或是 slam_toolbox 內部的矩陣運算時，OpenMP 能以最低的程式碼侵入性，榨乾多核心處理器的算力。

### 4. Grand Central Dispatch (GCD) 蘋果派發機制
#### 1. GCD 核心定位與語法擴充
* **生態系專屬：** GCD 是 Apple 為 macOS 與 iOS 作業系統量身打造的底層技術，深度整合在 C、C++ 與 Objective-C 語言的編譯器與 Run-time library 中。
* **Block (區塊) 語法：**
    * 為了實作 GCD，Apple 在 C 語言中擴充了 Block 的語法，使用脫字符號 `^` 作為標記。
    * 範例：`^{ printf("I am a block"); }`
    * **底層意義：** 一個 Block 本質上就是一段「可被封裝並延遲執行的 C 語言匿名函式 (Anonymous Function / Closure)」。系統工程師只需將想要平行處理的運算邏輯寫進 Block 中即可。
* **管理權限移交：** 開發者只需負責「定義 Block」並將其丟進「Dispatch Queue」。剩下的事情：要建立多少個底層 Pthreads 來接客？任務該分配給哪個 Thread？全部由 GCD 在底層全權自動管理 (Manages most of the details of threading)。

#### 2. Dispatch Queues (派發佇列) 的兩大戰略
在將 Block 丟給系統執行時，開發者必須根據任務的性質，選擇放入哪一種佇列。GCD 提供了兩種截然不同的派發策略：
* **Serial Queue (循序佇列)**
    * **運作機制：** 嚴格遵守 FIFO (First-In-First-Out) 規則。佇列中的 Block 會被一個接著一個「排隊」執行。前一個 Block 徹底執行完畢後，下一個 Block 才會開始。
    * **系統預設 (Main Queue)：** 每個 Process 啟動時，預設都會綁定一個 Main Queue。這通常是負責更新使用者介面 (UI) 的專屬通道，必須嚴格保持循序以防畫面撕裂。
    * **用途：** 開發者也可以自己建立額外的 Serial Queues，這非常適合用來**保護共享資源**。只要規定所有要修改某個全域變數的任務，都必須排入同一個 Serial Queue，就能物理上消滅 Race Condition 的發生機率。
* **Concurrent Queue (併發佇列)**
    * **運作機制：** 同樣遵守 FIFO 規則來「取出」Block，但是 GCD 會同時取出多個 Block，並將它們動態分配給底層 Thread Pool 中的多個可用 Threads 同時執行。
    * **效能釋放：** 這是徹底榨乾多核心算力的通道。多個 Block 取出後的執行順序與完成時間，將完全無法預測。
    * **實作範例拆解：**
    ```=c
    // 1. 取得系統內建的全域 Concurrent Queue
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    // 2. 將 Block 非同步 (async) 派發到該佇列中
    dispatch_async(queue, ^{ printf("I am a block."); });
    ```
#### 3. 【架構師觀點：GCD 與 ROS 2 的驚人相似性】
雖然 GCD 是蘋果專利，無法在 Ubuntu 上編譯，但它的「Queue 排隊派發」思維，與 ROS 2 的 `rclcpp::executors` 架構理念如出一轍。

在 ROS 2 中，我們也是將訂閱者 (Subscriber) 的 Callback 函式註冊到 Node 裡面，然後把 Node 丟給 Executor (類似 GCD 的 Dispatch Queue) 去管理。由 Executor 決定要用單執行緒 (Serial) 還是多執行緒 (Concurrent) 來消化這些 Callback 任務。

這種將「業務邏輯 (Block/Callback)」與「排程機制 (Thread Pool)」徹底解耦的設計模式，是現代高併發系統開發的絕對主流。

---

# Threading Issues (多執行緒系統衝突)

### 1. 核心議題總覽 (p.33)
將單執行緒程式改寫為多執行緒架構後，系統工程師必須親自處理以下五個由 OS 底層引發的複雜問題：
* **`fork()` 與 `exec()` 系統呼叫的語意衝突** (這正是 p.34 的重點)。
* **Signal handling (訊號處理)**：當使用者按下 `Ctrl+C` (SIGINT) 時，這個中斷訊號到底該送給 Process 裡的哪一個 Thread？
* **Thread cancellation (執行緒取消)**：如果要在中途強制砍掉一個正在運算的 Worker Thread，該如何確保它不會把記憶體鎖死 (Deadlock)？
* **Thread-local storage (TLS, 執行緒區域儲存)**：如何宣告一個「雖然是全域變數，但每個 Thread 都有自己獨立一份」的特殊變數？
* **Scheduler Activations (排程器啟動)**：這就是前面討論 Many-to-Many 模型時，LWP (虛擬處理器) 用來通知 User Space 的 Upcall 回呼機制。

### 2. 深潛探討：`fork()` 與 `exec()` 的語意危機 (p.34)
在傳統的 UNIX 系統中（如 Chapter 3 所學）：
* `fork()`：複製出一個一模一樣的 Child Process。
* `exec()`：將一個全新的可執行檔，覆蓋掉當前的 Process 記憶體空間。

**當多執行緒遇上 `fork()` 的災難**：
假設有一個 Process 內部正在跑 10 個 Threads，其中 Thread A 突然呼叫了 `fork()`，請問作業系統複製出來的 Child Process 應該長什麼樣子？
這在 UNIX 系統設計中引發了巨大的爭議，因此 POSIX 標準最終決定提供**兩種版本的 `fork()` 語意**，交由開發者依據情境選擇：

1. **Duplicate ALL threads (全複製版本)**：
    * Child Process 誕生時，裡面會直接內建 10 個一模一樣的 Threads，接續原本的狀態繼續跑。
    * **極度危險**：如果原本的 Thread B 剛好把某個 Mutex (互斥鎖) 給鎖上了，複製出來的 Child Process 裡的 Thread B' 也會認為自己擁有那把鎖。這非常容易導致子行程一出生就立刻陷入無解的死結 (Deadlock)。
2. **Duplicate ONLY the calling thread (單一複製版本)**：
    * 這是多數現代 Linux 系統的預設行為。
    * Child Process 誕生時，**裡面只會有唯一一個 Thread**（也就是呼叫 `fork()` 的那個 Thread A 的複製體），原本的其他 9 個 Threads 全部被作業系統無情抹殺。

**當多執行緒遇上 `exec()`：**
這個機制的運作邏輯相對單純。如果 Process 裡的任何一個 Thread 呼叫了 `exec()` (例如 `execvp("ls", ...)` )：
* 作業系統會立刻將這個 Process 裡的**所有 Threads (包含呼叫者本身) 全部強制終止**。
* 然後清空整個 Process 的記憶體，載入新的 `ls` 程式碼，並在裡面重生一個全新的單一 Main Thread 來執行。

---

**【工程決策思考點】**
這兩種 `fork()` 的語意，直接決定了後續程式碼的走向。
如果 Thread A 呼叫 `fork()` 之後的**下一行程式碼立刻就是 `exec()`**，那麼請問作業系統應該採用哪一種版本的 `fork()` 才最節省系統資源？

這兩頁投影片討論了作業系統中用來處理異步事件的核心機制：**Signals (訊號)**，以及它在多執行緒環境下引發的派送難題。

以下為結構化的 HackMD 筆記整理：

---

### Signal Handling (訊號處理機制)

#### 1. 訊號的核心生命週期 (p.35)
在 UNIX 系統中，Signal 是一種軟體中斷，用來通知 Process 發生了某個特定事件（例如使用者按下了 `Ctrl+C`、程式發生了除以零的錯誤、或是記憶體越界 `Segmentation Fault`）。

它的生命週期分為三個嚴格的階段：
1. **Generated (產生)**：某個事件發生，導致作業系統核心或另一個 Process 產生了一個訊號。
2. **Delivered (派送)**：作業系統將這個訊號遞送給目標 Process。
3. **Handled (處理)**：目標 Process 接收到訊號後，必須執行對應的處理動作。處理方式有兩種：
    * **Default Handler (預設處理器)**：每個訊號都有 OS 內建的預設動作。例如 `SIGINT` (Ctrl+C) 的預設動作就是直接強制終止程式；`SIGSEGV` 的預設動作是終止程式並產生 Core Dump。
    * **User-defined Handler (使用者自訂處理器)**：系統工程師可以在程式碼中呼叫 API，將某個訊號的處理權攔截下來 (Override)，改為執行自己寫的 C 語言函式。例如在 ROS 2 節點中攔截 `Ctrl+C`，讓節點在關閉前能優雅地儲存地圖或關閉馬達。

#### 2. 多執行緒環境下的派送難題 (p.36)
在傳統的單執行緒 (Single-threaded) 程式中，訊號的派送毫無懸念：因為只有一個 Thread，OS 當然就是直接把訊號砸在它頭上。

但是，當一個 Process 內部有 10 個活躍的 Threads 時，**作業系統到底該把訊號送給誰？**
這是一個沒有絕對標準答案的系統設計難題，投影片列出了四種可能的派送策略：

* **策略一：Deliver to the thread to which the signal applies (精準打擊)**
    * **適用情境**：如果是某個特定的 Worker Thread 執行了「除以零」的非法運算，引發了 `SIGFPE` (浮點數例外) 訊號。
    * **邏輯**：這個訊號是該 Thread 自己闖的禍 (Synchronous signal)，OS 當然應該把訊號直接送給那個闖禍的 Thread 讓它自己處理崩潰，而不是波及其他無辜的 Threads。
* **策略二：Deliver to every thread in the process (全域廣播)**
    * **適用情境**：例如使用者在終端機按下了 `Ctrl+C` (`SIGINT`)。
    * **邏輯**：這是一個外部的、與任何單一 Thread 都無關的異步事件 (Asynchronous signal)。它的目的是要終止整個 Process，因此 OS 可以選擇將訊號廣播給裡面所有的 Threads，讓它們各自執行清理動作並關閉。
* **策略三：Deliver to certain threads in the process (選擇性派送)**
    * **邏輯**：作業系統或 Thread Library 提供機制，允許程式設計師設定某些 Threads 可以「遮罩 (Mask / Block)」特定的訊號。OS 在派送時，只會挑選那些沒有遮罩該訊號的 Threads 來遞送。
* **策略四：Assign a specific thread to receive all signals (專職處理)**
    * **實務最佳解**：這是現代高併發系統 (包含複雜的 C++ AMR 專案) 最常採用的架構。
    * **邏輯**：系統架構師會刻意在程式啟動時，拉出一個獨立的 Kernel Thread，並將其設定為「專職的訊號處理員」。其他所有負責運算、通訊的 Worker Threads 全部設定為「拒收任何訊號」。這樣就能確保核心業務邏輯絕對不會被軟體中斷給打斷，極大化系統的穩定性。

---

**【架構師觀點連結】**
在開發 ROS 2 系統時，處理 `Ctrl+C` 的關閉流程 (Shutdown) 是極度關鍵的一環。了解上述的「策略四」，就能明白為什麼在 `main` 函式中呼叫 `rclcpp::init()` 之後，通常是由主執行緒 (Main Thread) 負責死守著 `rclcpp::spin()` 監聽系統狀態與訊號，而其他的訂閱者與計時器回呼函式，則是交由 Executor 分配到其他的 Worker Threads 去執行。

---

### Thread Cancellation (執行緒強制終止機制)

#### 1. 核心定義與致命風險 (p.37)
* **定義**：在一個執行緒 (Target thread) 尚未自然執行完畢 (尚未 return 或 exit) 之前，由外部強制將其終止。
* **Pthreads API**：呼叫 `pthread_cancel(tid);` 發出終止請求。
* **系統架構師的夢魘 (為何危險？)**：
    如果在執行緒正在 `malloc` 配置記憶體、寫入檔案、或是**鎖住 Mutex (互斥鎖)** 的瞬間將其強制終止，該執行緒將永遠無法釋放這些資源。這會直接導致嚴重的 Memory Leak (記憶體洩漏) 甚至全域 Deadlock (死結)。

#### 2. 兩種取消策略：暴力 vs. 優雅 (p.37)
為了解決上述的資源鎖死問題，作業系統理論定義了兩種取消方式：

1. **Asynchronous cancellation (非同步/強制取消)**：
   * **機制**：作業系統收到請求後，立刻、瞬間把目標執行緒砍掉。
   * **評價**：極度危險。除非確信該執行緒只是在做純數學運算，絕對沒有碰到任何鎖或系統資源，否則在嚴謹的 C++ 系統開發中應全面禁用此模式。
2. **Deferred cancellation (延遲/協商取消)**：
   * **機制**：呼叫 `pthread_cancel()` 只是在系統底層立下一個「取消請求 (Flag)」。目標執行緒會繼續活著，直到它運行到某個「安全檢查點」時，才會自己乖乖結束。

#### 3. Pthreads 的狀態機與實作細節 (p.38)
在 Pthreads 標準中，預設採用的是安全的 **Deferred (延遲)** 策略。目標執行緒的生死，完全取決於它當下的狀態設定：

* **Cancellation State (取消狀態)**：
  * **Disabled (關閉)**：如果執行緒正在執行極度關鍵的程式碼 (Critical Section)，它可以暫時拒絕被取消。此時外來的取消請求會進入 Pending (擱置) 狀態，直到執行緒重新 Enabled。
  * **Enabled (開啟)**：允許被取消 (預設值)。
* **Cancellation Point (取消點)**：
  * 在 Deferred 模式下，執行緒只有在執行到特定的系統呼叫 (例如 `sleep`, `read`, `write`, `join`) 時，才會順便檢查那個「取消請求 Flag」並自我了斷。這稱為取消點。
  * **手動安插**：如果迴圈裡都是純 CPU 運算，沒有系統呼叫，開發者必須在 `while` 迴圈內手動呼叫 `pthread_testcancel()`，主動製造一個安全檢查點。
* **Cleanup Handler (清理處理器)**：
  * 在執行緒真正死亡前，Pthreads 允許註冊一系列的清理函式 (類似 C++ 的解構子)。當執行緒在取消點決定要終結時，會先執行這些清理動作 (例如把剛才拿到的 Mutex 解鎖)，確保系統狀態安全後才斷氣。

#### 4. Linux 底層實作冷知識
投影片最後提到，在 Linux 系統上，Thread Cancellation 底層其實是透過前一個主題的 **Signals (訊號)** 來實作的。當呼叫 `pthread_cancel` 時，作業系統實際上是發送了一個特殊的即時訊號給目標 Kernel Thread，藉此觸發它去檢查取消旗標。

---

**【進階架構思考】**
在現代 C++ 系統工程中 (如 C++11 以後的 `<thread>` 函式庫)，標準委員會其實**刻意不提供**類似 `pthread_cancel` 這種強制取消的 API。

若要優雅地停止一個無窮迴圈的 Worker Thread，實務上通常會強烈建議配合 C++ 的 RAII (資源獲取即初始化) 特性，並使用哪一種同步變數來當作「停止旗標 (Stop Flag)」供執行緒定期檢查，以達到最安全的 Deferred Cancellation 效果？


---

### TLS 與 Scheduler Activations

#### 1. Thread-Local Storage (TLS, 執行緒區域儲存) - p.39
在多執行緒的記憶體模型中，通常只有兩種極端：要麼是全域共享 (Global Data)，要麼是生命週期極短的區域變數 (Local Variables)。TLS 提供了完美的第三種選擇。

* **核心定義**：宣告為 TLS 的變數，**每一個 Thread 都會擁有自己獨立的一份實體拷貝 (Own copy of data)**。
* **記憶體生命週期對比**：
    * **一般區域變數 (Local)**：一旦函式執行完畢 `return`，變數就會從 Stack 中被銷毀 (Visible only during single invocation)。
    * **TLS 變數**：它的生命週期與 Thread 本身一樣長。即使跨越多個不同函式的呼叫，該 Thread 依然可以讀取到它上一次寫入 TLS 的值 (Visible across function invocations)。其行為非常類似 `static` 變數，但差別在於它**不會**被其他 Thread 看到或污染。
* **C++ 系統實作連結**：
    * 在 C++11 中，只需加上 `thread_local` 關鍵字即可宣告：`thread_local int node_id = 0;`。
    * **實戰情境**：當使用 Thread Pool (例如 ROS 2 的 `MultiThreadedExecutor`) 時，系統工程師無法手動控制 Thread 的建立過程。若需要讓池子裡的每個 Worker Thread 都維護自己專屬的隨機亂數種子 (Random Seed) 或連線狀態，使用 TLS 是唯一且無須加鎖 (Lock-free) 的高效解法。

#### 2. Scheduler Activations (排程器啟動與底層通訊) - p.40
這頁投影片正式回收了前面探討 Many-to-Many (M:M) 模型時，關於「LWP (輕量級行程)」與「系統如何通知 User Space」的底層機制。

* **LWP 的定位確立**：
    * LWP 是安插在 User Thread 與 Kernel Thread 之間的資料結構。
    * 對上層的執行緒函式庫而言，它就是一顆**虛擬處理器 (Virtual Processor)**。函式庫負責把 User Thread 排程到這顆虛擬 CPU 上。
* **Upcall (上行呼叫) 通訊機制**：
    * 在 M:M 模型中，維持正確的 Kernel Thread 數量至關重要。
    * 當某個綁定在 LWP 上的 Kernel Thread 因為執行 Blocking I/O (如等待 2D LiDAR 的 Serial Port 資料) 而被作業系統凍結時，OS 核心必須觸發一個 **Upcall**。
    * **處理流程**：OS 呼叫位於 User Space 執行緒函式庫中的 **Upcall Handler (上行呼叫處理器)**。該處理器得知某個 LWP 卡死後，會立刻將原本排隊中的其他 User Threads 重新調度到其他空閒的 LWP 上，確保整個應用程式的併發效能不會跟著陪葬。

---

