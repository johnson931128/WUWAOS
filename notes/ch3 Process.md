---
title: ch3 Process

---

# Process 核心概念 (Process Concept)
#### 1. 核心定義與本質區別 (Program vs. Process)
這兩者的區別是作業系統最基礎的觀念，可以用「動靜之分」來理解：
* **Program (程式)：被動實體 (Passive Entity)。**
    * **狀態：** 安靜地躺在硬碟裡的藍圖，例如編譯出來的 `wuwa-os.bin` 或 ELF 執行檔。
* **Process (行程)：主動實體 (Active Entity)。**
    * a Program in Execution
    * **狀態：** 當作業系統將 Program 載入主記憶體 (RAM)，並分配 CPU 資源開始循序執行 (Sequential fashion) 時，它就活過來變成 Process 了。
    * 啟動方式：透過終端機輸入指令 (CLI) 或滑鼠雙擊 (GUI) 觸發。

#### 2. 一對多的生命共同體 (1-to-N Relationship)
* **One program can be several processes：** 同一個程式檔案，可以同時在記憶體中生成多個獨立的 Process。
    * **實務舉例：** 在 Ubuntu 開了三個終端機視窗，硬碟裡只有一個 /bin/bash 執行檔 (Program)，但記憶體裡會有三個獨立運作的 `bash` (Processes)，它們各自有自己的 PID 和記憶體空間，互不干擾。

#### 3. Process 的實體記憶體佈局 (Memory Layout)
![image](https://hackmd.io/_uploads/S1sY0zOiWe.png)
當一個 Process 在記憶體中跑起來時，OS 會嚴格且有條理地將它的記憶體切分為四個主要區塊 (Sections)。這直接對應到 C/C++ 程式碼的底層運作：
1. **Text Section (程式碼區段)：**
    * 存放編譯好的機器語言指令 (Machine code)。
    * 這是唯讀的，防止程式執行時不小心竄改自己的邏輯。
2. **Data Section (資料區段)：**
    * 存放程式生命週期內一直存在的變數。
    * 包含：**全域變數 (Global variables)** 與 **靜態變數 (Static variables)**。
3. **Heap (堆積區)：**
    * 專門處理**執行期間 (Run time)** 動態配置的記憶體。
    * C 語言的 ``malloc()`` 、`free()`，或是 C++ 的 `new`、`delete` 所申請的空間都在這裡。空間大小會動態增長。
4. **Stack (堆疊區)：**
    * 專門用來存放暫時性資料 (Temporary data)。
    * 包含：函式傳遞的參數 (Function parameters)、函式執行完的返回位址 (Return addresses)，以及在函式內宣告的區域變數 (Local variables)。空間大小也會動態增減。
5. 除了上述四個記憶體區塊，Process 的「當前活動狀態」是由 CPU 內部的 Program Counter (程式計數器, PC) 與 Processor Registers (處理器暫存器) 來記錄的。PC 永遠指著下一行要執行的指令位址。
# Process States (行程狀態與轉換)
![image](https://hackmd.io/_uploads/r1nee7_j-e.png)
#### 1. 五大基本狀態 (The 5 States)
在分時系統中，為了讓單一 CPU 能夠處理多個 Process，OS 會為每個 Process 標記狀態，作為管理的依據：
* **New (新建)：** Process 正在被建立，OS 正在為其配置 PCB 與基本資源，尚未進入主記憶體排隊。
* **Ready (就緒)：** 萬事俱備，只欠 CPU。Process 已經載入主記憶體，並在 ==Ready Queue== 中排隊等待排程器 (Scheduler) 點名。
* **Running (執行)：** Process 成功獲得 CPU 資源，正在執行指令。(單核心 CPU 任何瞬間只有一個 Process 處於此狀態)。
* **Waiting / Blocked (等待/阻塞)：** Process 執行到一半，需要等待外部事件完成 (最常見的是 I/O 操作，如等待硬碟讀取、網路封包或 LiDAR 數據)。此時 OS 會剝奪其 CPU 執行權，將其放入專屬的 ==Device Queue==。
* **Terminated (終止)：** Process 執行完畢或被強制終止。OS 會回收其記憶體，但 PCB 可能短暫保留等待父行程讀取 (殭屍狀態)。
#### 2. 狀態轉換機制 (State Transitions)
圖中的箭頭代表了作業系統介入 (==Context Switch==) 的時機：
* **new $\rightarrow$ ready** (admitted)：OS 評估資源充足，正式接納。
* **ready $\rightarrow$ running** (scheduler dispatch)：OS 排程器選中該 Process 分配 CPU。
* **running $\rightarrow$ ready** (interrupt)：通常是因為時間配額用盡 (Time slice expired)，硬體 Timer 發出中斷，OS 強制踢下 CPU，防止單一程式霸佔系統。
* **running $\rightarrow$ waiting** (I/O or event wait)：Process 主動發出 I/O 請求，CPU 無法繼續運算而暫停。
* **waiting $\rightarrow$ ready** (I/O or event completion)：硬體發出中斷通知 I/O 完成，OS 將 Process 喚醒並移回 Ready Queue 重新排隊。
#### 3. 💡 系統架構深思：為什麼 I/O 完成後只能回 Ready，不能直升 Running？
如果在 ROS2 系統中，等待網路封包的 I/O Process 一旦收到資料就立刻搶佔 CPU，會引發嚴重的系統問題。OS 強制要求回 Ready Queue 排隊的底層考量有三：
1. **避免飢餓 (Starvation)：** 防止頻繁觸發 I/O 的低優先權任務，無止盡地打斷正在執行 CPU 密集型運算 (如 VSLAM) 的任務。
2. **降低 Context Switch 成本 (Thrashing)：** 切換 CPU 狀態成本極高，避免頻繁的 I/O 中斷導致 CPU 算力全浪費在「存檔/讀檔」上。
3. **權限回歸排程器 (Scheduler)：** 將「誰該上 CPU」的決定權統一交還給 Scheduler，由演算法綜觀全局決定最佳執行順序，而不是任由硬體中斷盲目插隊。
# Process Control Block (PCB)
![image](https://hackmd.io/_uploads/SkXibQdi-e.png)
#### 1. 核心定義
* **本質：** ==作業系統用來管理==、==追蹤== Process 的資料結構 (在核心記憶體中)。每個 Process 都有自己專屬的一份 PCB。
* **別名：** Task Control Block (TCB)。
#### 2. PCB 包含的關鍵欄位 (與系統架構的呼應)
PCB 裡面儲存的資訊，完美對應了 OS 管理 Process 所需的所有要素：
* **Process state (行程狀態)**
    * **呼應主題：** 👉 五大狀態流轉 (New, Ready, Running...)
    * 紀錄這個 Process 當下到底處於什麼狀態，方便排程器管理，OS透過此欄位決定Process進哪個Queue。
* **Program counter (程式計數器, PC)**
    * **呼應主題：** 👉 Process 記憶體佈局 (Text Section) 與後續的 Context Switch
    * 紀錄該 Process 下一次被 CPU 喚醒時，要從哪一行指令的記憶體位址繼續往下執行。
* **CPU registers (CPU 暫存器)**
    * **呼應主題：** 👉 Context Switch (環境切換)
    * 當 Process 被踢下 CPU 時，原本在硬體暫存器裡算到一半的資料 (如累加器、堆疊指標等) 都要先「存檔」到這裡。下次輪到它時再「讀檔」回去。
* **CPU scheduling information (排程資訊)**
    * **呼應主題：** 👉 佇列管理 (Ready Queue)
    * 包含 Process 的優先權 (Priority)、以及用來串接各個佇列 (Queues) 的指標 (Pointers)。
* **Memory-management information (記憶體管理資訊)**
    * **呼應主題：** 👉 Process 記憶體佈局 (Text, Data, Heap, Stack)
    * 紀錄這個 Process 可以合法使用的實體記憶體範圍 (Base/Limit registers) 或分頁表 (Page tables)，作為安全隔離的邊界。
* **I/O status & Accounting (I/O 狀態與計帳資訊)**
    * 紀錄它佔用了哪些硬體設備、打開了哪些檔案 (List of open files，對應到 C++ 的 File Descriptors)，以及已經消耗了多少 CPU 時間。

# Context Switch
![image](https://hackmd.io/_uploads/SkcfxPYoWx.png)
### 假設CPU要把Process A換成Process B，OS會進行以下動作：
1. **暫停Process A。**
2. **把硬體CPU的register中的值、PC等當前狀態，儲存(SAVE)到Process A的PCB中。**
3. **從Process B的PCB中，載入(Restore)之前存起來的狀態到硬體CPU暫存器中。**
4. **讓CPU開始執行Process B。**
這個切換是純粹的overhead，在切換當下系統沒有在做任何有用的運算。因此PCB的結構設計與Context Switch程式碼必須設計的極致高效，直接用assembly操作暫存器存取等。

### 會發生 Context Switch 的箭頭標註
1. **Running $\rightarrow$ Ready (Interrupt / 時間配額用盡)**
    * **底層動作：** OS 強制介入，把當前 Process 的 CPU 暫存器數值與 Program Counter **「存檔 (Save State)」** 回它的 PCB。接著挑選下一個 Process。
2. **Running $\rightarrow$ Waiting (I/O or event wait / 主動等待 I/O)**
    * **底層動作：** Process 主動交出 CPU。OS 同樣會進行 「存檔 (Save State)」，把它丟進 Device Queue，然後挑選下一個 Process。
3. **Running $\rightarrow$ Terminated (Exit / 執行結束)**
    * **底層動作：** 雖然不用「存檔」了（因為行程要被銷毀），但 CPU 空出來了，**OS 必須從 Ready Queue 中挑選並 「讀檔 (Restore State)」** 下一個 Process 上來執行，這依然是一個 Context Switch 的過程。
4. **Ready $\rightarrow$ Running (Scheduler dispatch / 排程器分派)**
    * **底層動作：** 這是 Context Switch 的下半場。OS 將選中的 Process 的 PCB 內的暫存器數值，**「讀檔 (Restore State)」** 寫入實體 CPU 暫存器中，正式讓它接管 CPU。

### ❌ 「不會」發生 Context Switch 的箭頭 (極度容易被誤導的考點)
請特別注意這條箭頭，這是考試和系統設計最常踩的陷阱：
* **Waiting $\rightarrow$ Ready (I/O completion / I/O 完成)為什麼沒有？**
    * 當硬碟讀取完畢發出中斷時，==OS 只是把該 Process 的 PCB 從 Device Queue 「拔出來，改插進」 Ready Queue 裡面排隊==。
    * 這個動作是 OS 在背景操作的資料結構 (Linked List) 更新，**該 Process 本身並沒有立刻被放上 CPU 執行**，所以不需要切換 CPU 暫存器，也就沒有 Context Switch！(除非 OS 排程器立刻決定搶佔當前的 Running Process，那是另一條箭頭的事了)

### Context Switch 的時間軸與中斷機制
##### 1. 權力轉移的觸發點 (Interrupt or System Call)
一個正在 CPU 上狂奔的 Process ($P_0$) 是不會憑空把執行權交出來的。圖中指出，要把控制權從 Process 逼回給 OS，只有兩條路徑：
* **interrupt (硬體中斷)：** 非自願的。例如系統 Timer 計時器發出「時間配額已滿」的滴答聲，或是你鍵盤敲擊、LiDAR 感測器資料抵達的硬體訊號。這會強行打斷 CPU 當前的運算。
* **System Call (系統呼叫)：** 自願的。例如 $P_0$ 執行到了 `read()` 想要讀取檔案，或是呼叫了 `sleep()`。這時它主動觸發==軟體中斷== (Software trap)，拜託 OS 核心幫忙做事。
##### 2. 中斷返回與重返榮耀 (The Return Journey)
注意圖中 OS 執行完 `reload state from PCB_1` 之後，有一條箭頭指回 $P_1$ 讓它開始執行 (executing)。這個動作在底層是如何發生的？
* `iret` **(Interrupt Return) 指令：** 在 x86 架構 (WSL2 底層) 中，OS 核心在完成 PCB 讀檔後，最後一定會呼叫一個特殊的組合語言指令（如 `iret` 或 `sysret`）。
* **底層動作：** 這個指令會瞬間把剛剛從 $PCB_1$ 讀出來的 **Program Counter (PC)** 塞進硬體的 Instruction Pointer 暫存器中，==同時將 CPU 的特權等級從 Kernel Mode 降級回 User Mode==。下一個 Clock 週期，CPU 就會毫無違和感地從 $P_1$ 上次斷掉的那行 C++ 程式碼繼續咬下去。
##### 3. 視覺化的「純耗損 (Pure Overhead)」
* 圖中 OS 在做 `save` 和 `reload` 的那段時間，可以看到左右兩邊的 $P_0$ 和 $P_1$ 都是處於 `idle` (閒置) 的。這條垂直的空窗期，視覺化了之前所說的：「Context switch 時間內，系統沒有在做任何有用的應用程式運算」
# Process Scheduling & Queues (排程與佇列架構)
#### 1. 排程的兩大終極目標
作業系統的核心排程器 (Process Scheduler) 存在的主要目的，是為了解決兩個看似矛盾的工程需求：
* **最大化 CPU 使用率 (Maximize CPU use)：** 針對 Batch system (批次處理系統)，確保昂貴的 CPU 永遠有事做，不要停下來等慢速的 I/O 設備。
* **分時共享 (Time sharing)：** 針對 User programs (使用者應用)，必須頻繁且極速地切換 CPU 執行權，讓每個 Process（例如你在 WSL2 裡同時開的 VSCode 和編譯器）都覺得自己獨佔了整台電腦，保持極高的人機互動性。

#### 2. 底層實作：佇列分類與 Linked List 指標操作
![image](https://hackmd.io/_uploads/Hk80bPKj-g.png)
為了管理成千上萬個 Process，OS 維護了多種 Scheduling queues (排程佇列)。
* **佇列的分類：**
    * **Job queue：** 系統中所有 Process 的總名單 (包含在 RAM 裡和還在硬碟裡的)。
    * **Ready queue：** 已經載入主記憶體 (RAM)，正在排隊等 CPU 的 Process 名單。
    * **Device queues (I/O 佇列)：** 卡在 Waiting 狀態，正在等待特定硬體設備 (如磁帶、硬碟、終端機) 回應的 Process 名單。每個硬體都有自己專屬的 Queue。
* **C/C++ 實作視角：**
    * Queue 裡面放的絕對不是 Process 龐大的記憶體資料，而是之前學過的 ==PCB== (行程控制表)！
    * 在 C 語言的底層實作中，這就是一個標準的 Linked List (鏈結串列)。作業系統只會維護一個 `head`(頭指標) 和一個 `tail` (尾指標)。當 Process 在不同狀態間切換時，OS 只是在修改指標 (Pointers) 的指向，而不是搬移幾十 MB 的記憶體。這就是為什麼 Context Switch 能夠在微秒級別完成，也是你在算力受限的樹莓派上開發時必須具備的「傳指標大於傳實體」的系統工程思維。

#### 3. 狀態機與佇列流轉圖 (Queueing Diagram)
![image](https://hackmd.io/_uploads/SydnfPYj-l.png)
1. **I/O request $\rightarrow$ I/O queue：** Process 主動要求讀取磁碟或感測器，被丟進硬體專屬的 Queue 等待。完成後流回 Ready queue。
2. **Time slice expired (時間配額用盡)：** 硬體計時器 (Timer) 中斷觸發，OS 強制剝奪 CPU 執行權，Process 被直接踢回 Ready queue 的尾端重新排隊。
3. **Fork a child：** 呼叫 `fork()` 產生子行程後，Parent process 呼叫 `wait()`，進入等待狀態，直到 Child 執行完畢才流回 Ready queue。
4. **Wait for an interrupt：** Process 進入休眠，等待某個特定的外部軟硬體中斷訊號喚醒。

# Schedulers (排程器家族)
在深入排程器之前，必須先定義系統中運行的程式種類。這會直接影響排程策略：
#### 1. 行程的兩大運算特性 (Process Profiles)
* **I/O-bound process (I/O 密集型)：**
    * **特徵：** 花費==大量時間在等待 I/O==，CPU 運算時間極短。(Many short CPU bursts)
    * **實務舉例：** ROS2 中負責接收 2D LiDAR 序列埠資料的 Node，或是負責網路通訊的 Node。
* **CPU-bound process (CPU 密集型)：**
    * **特徵：** 花費==大量時間在狂算數學==，極少進行 I/O 操作。(Few very long CPU bursts)

#### 2. 三大排程器分類與職責 (The 3 Schedulers)
1. **Short-term scheduler (短期 / CPU 排程器)**
    * **任務：** 從 Ready Queue 挑選下一個要放上 CPU 執行的 Process。
    * **執行頻率：** 極度頻繁 (毫秒級別)。
    * **設計重點：** 因為一直被呼叫，它的演算法必須超級快 (Must be fast)，否則會吃掉太多 Context Switch 的 Overhead。在現代系統 (包含你的 WSL2 Ubuntu) 中，這通常是最活躍、甚至唯一存在的排程器。

2. **Long-term scheduler (長期 / Job 排程器)**
    * **任務：** 決定要把==硬碟裡的哪些程式==，載入到實體記憶體 (RAM) 的 Ready Queue 變成 Process。
    * **執行頻率：** 極低 (秒或分鐘級別)，速度可以比較慢。
    * **兩大核心 KPI：**
        1. **控制多工程度 (Degree of multiprogramming)：** 嚴格控管 RAM 裡面同時有幾個 Process 在跑。
        2. **追求完美的 Process mix (行程混搭比例)：** 如果全部挑 CPU-bound，I/O 設備會閒置；如果全挑 I/O-bound，CPU 會閒置。長期排程器必須完美混搭兩者，讓 CPU 和 I/O 硬體同時達到 100% 滿載。

3. **Medium-term scheduler (中期排程器 / 記憶體救星)**
* ![image](https://hackmd.io/_uploads/SJ_2VvFiZl.png)
    * **觸發時機：** 當系統 RAM 快要爆滿，必須緊急降低「多工程度」時介入。
    * **核心機制：Swapping (置換)**
        * **Swap out (換出)：** 把記憶體中==暫時沒有要執行的 Process== (例如卡在 I/O Queue 很久的) 連同 PCB，==整包搬到速度較慢的硬碟 (Disk) 上存放==，釋放寶貴的 RAM 給更緊急的程式用。
        * **Swap in (換入)：** 當該 Process 準備好繼續執行，且 RAM 有空位時，再把它從硬碟讀回 Ready Queue。
        * **實務連結：** Linux 系統安裝時劃分的 Swap Partition (置換空間) 就是專門給中期排程器放這些資料的腹地。
---

# Process Creation (行程建立與家族樹)
### 1. 行程的繁衍模型 (Tree of Processes)
在作業系統中，除了開機時的第一個行程外，沒有任何 Process 是憑空出現的。
* **基本關係：** 一個現存的 Parent process (父行程) 可以透過系統呼叫創造出多個 Children processes (子行程)。
* **拓撲結構：** 因為子行程可以繼續創造孫行程，整個系統的 Process 最終會形成一個龐大的樹狀圖 (Tree)。
* **唯一識別碼：** 為了精準管理這棵樹上的每一個節點，作業系統會賦予每個 Process 一個獨一無二的整數編號，稱為 PID (Process Identifier)。
### 2. Linux 的行程創建：init (或 systemd)
![image](https://hackmd.io/_uploads/Bkp_FwKobl.png)
從講義 p.24 的 Linux 行程樹狀圖可以清楚看到：
* `init` **(pid = 1)：** 它是所有 User-space 行程的絕對祖先。系統開機載入 Kernel 後，第一個執行起來的應用程式就是它。
* **日常應用的族譜：** 當登入系統開啟終端機時，就走了一趟 `login` $\rightarrow$ `bash` 的分支。你在終端機裡下達的每一個指令（例如開 `vim` 或執行 `ls`），都是由 `bash` 這個 `Parent` 繼續生出來的 Child processes。
### 3. 父子之間的設計選擇 (Design Options)
![image](https://hackmd.io/_uploads/S1LY9DFjZx.png)
當 Parent 決定生下 Child 時，作業系統允許開發者針對以下兩個維度進行設定：
* **維度一：資源共享 (Resource sharing options)**
    * **Share all：** 父子共用所有的記憶體與開啟的檔案。
    * **Share subset：** 子行程只繼承父行程指定的「部分」資源。
    * **Share none：** 完全不共享。子行程擁有自己獨立且被隔離的資源空間 (這是 Linux 預設最常見的做法)。
* **維度二：執行順序 (Execution options)**
    * **Concurrent (並行)：** 生完小孩後，父子兩人同時在 CPU 上競爭執行權，各忙各的。
    * **Wait (等待)：** Parent 生完小孩後，自己主動進入 Waiting 狀態，停在原地不動，直到 Child 執行結束並終止後，Parent 才會被喚醒繼續執行。































# Process Creation & Termination (行程建立與終止機制)
### 1. UNIX 行程建立的兩階段哲學 (fork & exec)
![image](https://hackmd.io/_uploads/rkAVivKj-l.png)
系統會為 Child Process 分配一塊全新的實體記憶體，然後把 Parent Process 的所有內容原封不動地複製過去。這包含：
* `fork()`**：細胞分裂般的完美複製**
    * **動作：** OS 會為 Child 建立一個全新的 PCB，並賦予全新的 PID。
    * 程式碼段 (Text/Code Segment)：執行的機器碼。
    * 資料段 (Data Segment)：全域變數與靜態變數。
    * 堆積 (Heap)：動態配置的記憶體 (如 malloc 或 new 的資料)。
    * 堆疊 (Stack)：區域變數與函數呼叫紀錄。
    * **記憶體處理 (Address space)：** Child 會「完美繼承」Parent 當下所有的記憶體藍圖 (包含 Text, Data, Heap, Stack 與 Program Counter)。
    * PCB (Process Control Block) 是作業系統用來追蹤每一個行程的「身分證與狀態表」。當執行 fork() 時，作業系統會為 Child 建立一個全新且獨立的 PCB。
        * 這個新的 PCB 會繼承父行程絕大多數的屬性（例如開啟的檔案描述子、環境變數等），但有幾個絕對不同的地方：Process ID (PID)：子行程會獲得一個全系統唯一的全新 PID。
        * Parent Process ID (PPID)：子行程的 PPID 會記錄為父行程的 PID。
        * CPU 執行狀態 (Program Counter & Registers)：雖然子行程的 Program Counter 也是指向 fork() 之後的下一行指令，但作業系統會透過 回傳值 (Return Value) 來讓父子行程知道自己的身分。
    * 💡 **效能關鍵 (Copy-on-Write, CoW)：** 實體 RAM 並不會在 `fork()` 的瞬間被完整拷貝。OS 會先讓父子共用同一塊實體記憶體（標記為唯讀）。直到其中一方試圖「修改」變數時，OS 才會真正複製那一小塊分頁 (Page)。這讓 `fork()` 的執行速度極快。
* **`exec()` 家族 (如 `execlp`)：徹底的變異**
    * **動作：** 載入一個全新的外部執行檔 (例如 `/bin/ls`)。
    * **記憶體處理：** 這個新程式會殘酷地完全覆寫 Child 剛從 Parent 那裡繼承來的記憶體空間。呼叫 exec() 之後的 C 程式碼絕對不會被執行，因為 Child 的靈魂已經換成別人了。
    * 當在 C 程式碼中呼叫 `exec()` 時，作業系統核心會做以下動作：
        1. **清空原本的腦袋：** OS 會把你當前 Process 記憶體裡面的 Text Section (程式碼區段)、Data Section (全域變數)、Heap 和 Stack 全部清空丟掉。
        2. **塞入全新的靈魂：** OS 去硬碟裡找出指定的那個新執行檔（例如 `/bin/ls`），把它的機器碼載入到剛剛被清空的 Text Section 裡面。
        3. **重置指標：** OS 將 Program Counter (PC) 歸零，指向這個新程式（`/bin/ls`）的第一行機器碼。
        4. **結論：** 因為原本 C 語言編譯出來的機器碼（包含寫在 `exec()` 下面的所有 `printf` 或邏輯），已經在實體記憶體中被徹底抹除並覆蓋掉了。CPU 根本找不到那些指令，當然就不可能執行。

### C 語言 Fork-Exec-Wait 實作模型
```=c
#include <sys/types.h>
#include <sys/wait.h> /* 解決 wait() 未宣告的編譯警告 */
#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid;

    /* * 【Fork-Exec-Wait 多路復用模型】
     * fork() 呼叫的瞬間，作業系統會利用 CoW (Copy-on-Write) 機制複製當前 Process 的記憶體。
     * 奇特之處：呼叫一次，回傳兩次！實體記憶體中從此有了兩個獨立的 pid 變數。
     */
    pid = fork();

    if (pid < 0) {
        /* * 【分支一：系統錯誤】
         * 回傳負數代表建立 Child 失敗。
         * 常見原因：實體記憶體不足，或系統已達到 Process 數量的硬體上限。
         */
        fprintf(stderr, "Fork Failed");
        return 1;
    } 
    else if (pid == 0) {
        /* * 【分支二：子行程 (Child) 專屬路線】
         * Child 自己記憶體中的 pid 變數會收到 0。
         * 接著呼叫 execlp()，載入全新的外部執行檔 (此處為系統的 ls 指令)。
         * * ⚠️ 致命細節：只要 execlp 成功，Child 原本的 Text Section 會被 /bin/ls 
         * 的機器碼徹底覆寫。因此，寫在這個函數下方的任何 C 程式碼都絕對不會被執行！
         */
        execlp("/bin/ls", "ls", NULL);
        
        /* 只有當 execlp 失敗 (如找不到檔案) 時，記憶體才不會被覆寫，才會執行到這行 */
        fprintf(stderr, "Exec Failed! Command not found.\n");
        return 1; 
    } 
    else {
        /* * 【分支三：父行程 (Parent) 專屬路線】
         * Parent 自己記憶體中的 pid 變數會收到「大於 0 的新 Child PID」。
         * * 為了防止 Child 變成殭屍行程 (Zombie)，Parent 在此呼叫 wait(NULL)，
         * 主動交出 CPU 進入 Waiting 狀態，停在原地等待 Child 執行結束。
         */
        wait(NULL);
        
        /* 當 Child 結束並發出 SIGCHLD 訊號後，Parent 才會被 OS 喚醒執行此行 */
        printf("Child Complete\n");
    }

    return 0;
}
```
### 3. 行程終止與資源回收 (Process Termination)
當 Process 的生命走到盡頭，系統必須妥善處理它的「後事」：
* **正常壽終正寢 (`exit()`)**
    * Process 執行完最後一行程式碼，主動呼叫 `exit()`。
    * OS 會立刻回收它佔用的實體資源 (記憶體、檔案指標等)。
    * OS 會保留它的「死因/狀態碼 (Status data)」，透過 `wait()` 傳遞回報給 Parent。
* **被強制撲殺 (`abort()`)**
    * Parent 擁有生殺大權，可以隨時呼叫 `abort()` 終結 Child。
    * **撲殺的常見原因：**
        1. **Child 消耗了過多的系統資源 (失控)。**
        2. **Child 正在計算的任務已經不需要了。**
        3. **==連鎖終止 (Cascading termination)==：** Parent 自己準備要結束了，而 OS 架構不允許失去 Parent 的 Child 繼續孤獨存活，因此將其底下所有子孫一併終結。

### 4. Process Termination 異常狀態與系統回收機制
1. **連鎖終止 (Cascading Termination)**
    * **核心定義：** 某些作業系統架構不允許子行程在父行程死亡後繼續獨立存活。
    * **底層機制：** 一旦父行程被終止（不論是正常結束 `exit` 或異常崩潰 `abort`），作業系統會主動介入，像砍樹一樣將其底下所有的子行程 (Children)、孫行程 (Grandchildren) 全部無情地強制終結。
    * **設計目的：** 確保系統中不會殘留失去控制、無法被管理的背景惡意或無效程式。
2. **父行程的等待與收屍 (`wait()` 系統呼叫)**
    * 在 UNIX/Linux 系統中，父行程有義務追蹤子行程的生命週期。
    * **API 呼叫：** 透過執行 ``pid = wait(&status)``;，父行程會交出 CPU 進入等待狀態。
    * **雙重回傳資訊：**
        1. **回傳值 (`pid`)：** 作業系統會回傳剛死亡的子行程的真實 PID。
        2. **狀態碼 (`&status`)：** 作業系統會把子行程的「死因」（例如 `return 0` 正常結束，或是被哪個硬體中斷給殺死）寫入這個指標指向的記憶體位址，供父行程後續進行錯誤處理。

### 5. 作業系統最經典的兩大異常狀態 (除錯核心)
這兩個異常狀態的根本差異在於 **「誰先死」以及「有沒有呼叫 wait()」**：
1. **🧟‍♂️ Zombie (殭屍行程)**
    * **發生條件：** 子行程先死（呼叫了 `exit`），但 父行程還活著，卻遲遲沒有呼叫 `wait()` 來幫忙收屍。
    * **底層架構狀態：** 子行程的實體記憶體（Text, Data, Heap, Stack）雖然已被 OS 成功回收，但為了讓父行程未來還能查詢死因，OS 無法將其 PCB (行程控制表) 從系統的 Process Table 中刪除。這個卡在系統清單中、佔用著一個 PID 名額卻無實體記憶體的空殼，就稱為 Zombie。若殭屍太多，系統的 PID 會被耗盡，導致無法再建立新程式。
2. **👶 Orphan (孤兒行程)**
    * **發生條件：** 父行程先死（且未呼叫 `wait` 就結束了），但 子行程卻還活著 繼續在背景狂跑演算法。
    * **底層架構狀態：** 子行程瞬間失去父行程。在 Linux 架構中，OS 不會放任孤兒不管，而是會觸發「收養機制」。系統的創世神 init 行程 (PID=1) 會主動成為這些孤兒的新父行程。當這些孤兒未來執行結束時，init 會負責任地幫忙呼叫 `wait()`，防止系統中充滿殭屍。
# Interprocess Communication (IPC) 行程間通訊
### 1. 行程的獨立與合作 (Independent vs. Cooperating)
在作業系統中，Process 依據是否與外界互動，可分為兩大類：
* **Independent (獨立行程)：** 完全在自己的記憶體沙盒中執行，不影響其他行程，也不受他人影響。雖然安全，但無法完成複雜的系統級任務。
* **Cooperating (合作行程)：** 打破沙盒邊界，需要與其他 Process 共享資料與狀態。

==為什麼要承擔風險讓 Process 合作？ (四大優勢)==
1. **Information sharing (資訊共享)：** 多個程式需要存取同一份資料（例如：多個節點同時讀取同一份地圖資料）。
2. **Computation speedup (加速運算)：** 將龐大任務拆解成多個子任務，交由多核心 CPU 平行處理（Parallel processing）以榨乾硬體算力。
3. **Modularity (模組化)：** 這是現代系統工程的圭臬！將肥大的系統拆解為單一職責的 Process。例如在自走車專案中，將系統拆分為負責感測器 (2D 雷達)、大腦決策層 (狀態機)、小腦導航層 (Nav2/Smac Planner) 的獨立節點，實現高內聚低耦合。
4. **Convenience (便利性)：** 允許單一使用者同時執行多個任務（如一邊編譯 C++ 程式碼，一邊透過 WSL2 執行模擬環境）。
### 2. IPC 兩大通訊模型剖析 (Communications Models)
![image](https://hackmd.io/_uploads/r19TfuKsbe.png)
既然 Process 之間有著嚴格的記憶體保護機制，它們要如何「合法且安全」地交換資料？OS 提供了兩種截然不同的底層實作模型：
1. **模型 A：Message Passing (訊息傳遞)**
    * **運作機制 (核心中介)：** 如講義圖 (a) 所示，Process A 和 B 絕對不共用任何變數。所有資料必須打包成訊息 ($m_0, m_1, m_n$)，並透過呼叫系統 API (==System Calls==) 交給 OS Kernel。Kernel 會將訊息放入內部的 Message Queue (訊息佇列)，再由接收方將其領出。
    * **架構優勢：**
        * **極度安全：** 因為記憶體實體隔離，不存在 Process A 不小心寫壞 Process B 記憶體的問題。
        * **分散式系統首選：** 完美支援跨網路通訊。開發者只需定義好資料格式 (例如 YAML 或特定的 `.msg` 檔)，發送方根本不需要知道接收方在哪裡。
    * **架構劣勢 (Overhead 高)：** 速度較慢，每一次通訊都需要在 User Space 與 Kernel Space 之間進行切換 (==Context Switch==) 以及多次的記憶體拷貝。
2. **模型 B：Shared Memory (共享記憶體)**
    * **運作機制 (核心退場)：** 如講義圖 (b) 所示，OS Kernel 僅在初期介入，於實體 RAM 中開闢一塊「中立區」，並將其同時映射 (Map) 到 Process A 與 Process B 的虛擬記憶體空間中。一旦建立完成，後續的通訊**完全由使用者行程自行控制，OS 絕對不插手**。
    * **架構優勢 (效能極致)：**
        * **最快速度 (Fastest IPC)：** 資料傳輸等同於直接讀寫 C++ 陣列/指標，達到真正的「零拷貝 (Zero-Copy)」。這對於在算力受限的硬體 (如樹莓派) 上傳輸巨量資料（如高頻率的 LiDAR 點雲或影像）是唯一的效能解法。
        * **架構劣勢 (開發風險極高)：**
            * **同步問題 (Synchronization)：** 因為 OS 放手不管，若 Process A 寫到一半 Process B 就跑來讀，或者兩者同時寫入，資料將發生嚴重的 ==Race Condition== (競爭危害) 而損毀。
            * 開發系統時，工程師必須運用高階技巧 (如 Mutex 互斥鎖、Semaphore 號誌) 來親自建立交通管制機制。
# Producer-Consumer Problem (理論與架構篇)
### 1. 核心定義與情境
* **Producer (生產者)：** 不斷產生資料的 Process（實務舉例：AMR 上的 LiDAR 感測器不斷吐出點雲資料）。
* **Consumer (消費者)：** 不斷拿資料去處理的 Process（實務舉例：SLAM 演算法拿點雲去建圖）。
* **核心痛點：** 兩者的「運作速度通常不一致」。必須在中間建立一個 Buffer (緩衝區) 來做速度匹配。
    * **Unbounded-buffer (無窮緩衝區)：** 理論模型，記憶體無限大，生產者永不阻塞。
    * **Bounded-buffer (有限緩衝區)：** 現實唯一解，陣列大小固定。滿載時生產者必須等待，全空時消費者必須等待。

### 2. Bounded-Buffer 的系統架構與數學推導
在 Shared Memory 模型下，通常使用 Circular Array (環形陣列) 來實作有限緩衝區。
* **基礎資料結構：** 大小為 BUFFER_SIZE 的陣列，搭配兩個指標：
    * `in` 指標：生產者專用，指向下一個要寫入資料的空格。
    * `out` 指標：消費者專用，指向下一個要被讀取資料的位置。
* **同餘運算 (Modulo Arithmetic)：**
    * 為了讓陣列頭尾相連形成「環」，指標推進的數學公式必須是：
        * $Next\_Index = (Current\_Index + 1) \bmod BUFFER\_SIZE$
* **空與滿的終極辨識邏輯 (核心 Trade-off)：**
    * **空 (Empty)：** 當 $in == out$。
    * **滿 (Full)：** 若陣列全滿，in 繞一圈會剛好疊在 out 上，導致 $in == out$，這會與「空」的條件產生致命衝突。
    * **數學妥協解法：** 為了明確區分空與滿，必須永遠犧牲一個陣列格子不放資料。將滿載條件定義為「`in` 再往前走一步就會撞到 `out`」：$((in + 1) \bmod BUFFER\_SIZE) == out$

### 3. Producer (生產者) 實作邏輯
生產者的任務是將資料不斷塞入 Buffer。此處最關鍵的在於「滿載」時的防護機制。
```=c
item next_produced;
while (true) {
    /* 假設此處有程式碼負責產生資料，並存入 next_produced */

    /*
     * 【核心防護機制：檢查是否滿載】
     * 利用同餘運算判斷：若 in 往前走一步會撞到 out，代表 Buffer 已滿。
     * ⚠️ 致命缺陷 (Busy Waiting)：注意結尾的單獨分號 `;`。
     * 當 Buffer 滿載時，Process 會卡在這個 while 迴圈中瘋狂空轉，
     * 不斷消耗 CPU 算力來檢查一個尚未改變的條件。
     */
    while (((in + 1) % BUFFER_SIZE) == out)
        ; /* do nothing */

    /* 【寫入資料與更新指標】
     * 確定有空間後，將資料寫入，並將 in 指標往前推進一格。
     */
    buffer[in] = next_produced;
    in = (in + 1) % BUFFER_SIZE;
}
```
### 4. Consumer (消費者) 實作邏輯
消費者的任務是從 Buffer 提取資料。
```=c
item next_consumed;
while (true) {
    /*
     * 【核心防護機制：檢查是否為空】
     * 當 in == out 時，代表 Buffer 內無資料可取。
     * 同樣存在 Busy Waiting (忙碌等待) 的效能毒瘤。
     */
    while (in == out)
        ; /* do nothing */

    /* 【讀取資料與更新指標】
     * 取出資料，並將 out 指標往前推進一格。
     */
    next_consumed = buffer[out];
    out = (out + 1) % BUFFER_SIZE;

    /* 假設此處有程式碼負責處理 next_consumed 內的資料 */
}
```
### 5. 系統架構師視角：Busy Waiting (忙碌等待) 的災難
* **邏輯正確，效能零分：** 雖然這段程式碼在邏輯上能成功運作，但在實務的作業系統或算力受限的嵌入式設備（如部署 AMR 專案的 Raspberry Pi）中是不可接受的。
* **系統狀態的錯誤應用：** 當 Buffer 為空時，Consumer 的 while 空轉會讓 OS 誤以為該 Process 「正在努力運算」，導致其持續霸佔 CPU (處於 Running 狀態)，使 CPU 使用率飆升至 100% 並產生高熱。
* **正確解法 (Sleep & Wakeup)：** 必須呼叫 OS 提供的同步機制 (Synchronization)。當發現空/滿時，Process 應主動呼叫系統 API (如 `sleep()`)，讓 OS 將其狀態強制切換為 **Waiting / Blocked**，交出 CPU。直到另一方放入/取出資料後，再透過 `wakeup()` 將其喚醒並排入 Ready Queue。

# Message Passing 通訊連結建立
在訊息傳遞模型中，Process 之間必須先建立 Communication Link (通訊連結)。針對「信要寄給誰」，作業系統提供了兩種邏輯設計：

### 1. Direct Communication (直接通訊)
* **核心概念：** 指名道姓 (Explicit naming)。寄件者與收件者必須精確知道對方的身分證字號 (PID)。
* **API 實作：**
    * **`send(P, message)`：** 發送訊息給 Process P。
    * **`receive(Q, message)`：** 只接收來自 Process Q 的訊息。
* **通訊連結特性：**
    * 自動建立，不需要事先宣告。
    * 嚴格的一對一 (1-to-1)：一條 Link 永遠只綁定一對 Process。
* **❌ 架構師視角 (致命缺陷)：**
    * **極度高耦合 (Hard-coding)：** 如果在系統運作中，負責處理感測器的 Process 崩潰重啟了，作業系統會配發一個全新的 PID。此時，所有寫死舊 PID 的通訊程式碼將全數失效，必須重新編譯。這完全違背了現代軟體工程的模組化精神。

### 2. Indirect Communication (間接通訊)
* **核心概念：** 透過信箱交接 (Mailboxes / Ports)。信件不直接寄給人，而是寄到一個具有唯一 ID 的信箱。
* **API 實作：**
    * `send(A, message)`：將訊息丟入信箱 A。
    * `receive(A, message)`：從信箱 A 拿出訊息。
* **通訊連結特性：**
    * 雙方必須「共享」同一個信箱，Link 才會成立。
    * **支援多對多 (N-to-N)：** 一條 Link (信箱) 可以關聯多個發送者與接收者。
* **✅ 架構師視角 (ROS2 的底層靈魂)：**
    * 這種架構達成了完美的**解耦 (Decoupling)**。在開發 ROS 2 Jazzy 與 Nav2 的 AMR 專案時，節點之間互相通訊完全不需要知道對方的存在。Publisher 將 2D LiDAR 點雲發送到 `/scan` 這個 Topic (這就是一個巨大的 Mailbox)；Subscriber 只要訂閱 `/scan` 就能收到資料。這正是 Indirect Communication 最經典的實務應用。

### 3. 共享信箱的競爭難題 (Mailbox Sharing)
* **問題情境：** 既然支援多對多，假設 $P_1$ 寄了一封信到信箱 A，而 $P_2$ 和 $P_3$ 同時都在監聽信箱 A，**這封信到底該給誰**？
* **作業系統的解決方案 (Solutions)：**
    1. **限制數量：** 強制規定一個信箱最多只能有兩個 Process 連線 (退化成類似直接通訊)。
    2. **互斥執行 (Mutual Exclusion)：** 系統在底層上鎖，確保同一時間只有一個 Process 能夠執行 receive() 動作，誰先搶到鎖誰就拿走信。
    3. **系統任意挑選 (Arbitrary selection)：** 由 OS 隨機挑選一個幸運兒接收，事後再發送通知告訴寄件者「信最後被誰拿走了」。(在現代分散式系統中，這常被發展為 Load Balancing 負載均衡的輪詢機制)。

### 4. Message Passing 同步機制與緩衝區
##### 1. 訊息傳遞的時序同步 (Synchronization)
在呼叫 `send()` 或 `receive()` 系統 API 時，作業系統提供了兩種截然不同的執行緒阻塞 (Blocking) 策略：
* **Blocking (同步 Synchronous)：**
    * **Blocking send：** 發送者把信交給 OS 後，必須原地卡住 (進入 Waiting 狀態)，直到確認接收者真的把信收走了，發送者才能繼續執行下一行 C++ 程式碼。
    * **Blocking receive：** 接收者去信箱拿信，如果信箱是空的，接收者必須原地卡住睡著，直到有新信件抵達才會被 OS 喚醒。
* **Non-blocking (非同步 Asynchronous)：**
    * **Non-blocking send：** 發送者把信丟進信箱後，**立刻回頭繼續執行後續程式碼**。不在乎信件是否被收走。(這是 ROS2 Publisher 的預設底層行為)。
    * **Non-blocking receive：** 接收者去信箱拿信，如果有信就拿走；如果信箱是空的，OS 不會讓接收者睡著，而是立刻回傳一個 **Null message (空訊息或錯誤碼)**，讓接收者可以先去處理其他任務，稍後再來檢查。

##### 2. 架構反思：為何 Producer-Consumer 變得極度簡單 (Trivial)？
回顧先前在 Shared Memory 中，為了解決生產者與消費者問題，必須手刻 Circular Array、計算 `%` 同餘數學，甚至還要面臨 Busy Waiting 榨乾 CPU 的災難。
但在 Message Passing 架構下：
```=c
/* Producer */
message next_produced;
while (true) {
    /* 產生資料 */
    send(next_produced); // OS 接手處理所有底層邏輯！
}

/* Consumer */
message next_consumed;
while (true) {
    receive(next_consumed); // 若無資料，OS 自動讓此 Process 休眠
    /* 處理資料 */
}
```
底層系統觀點：之所以被稱為「Trivial (微不足道/極度簡單)」，是因為那些複雜的指標管理、空與滿的邊界檢查、以及 CPU 的 Sleep/Wakeup 狀態切換，**全部都被封裝進 OS Kernel 提供的 `send` 與 `receive` 系統呼叫中了**。開發者只需專注於資料本身的處理。

##### 3. 緩衝區容量的底層實作 (Buffering)
承接上述的 `send` 與 `receive`，OS 內部的信箱 (Message Queue) 容量設計會直接決定系統的運作行為：
1. **Zero capacity (零容量)：**
    * 信箱容量為 0，無法暫存任何訊息。
    * **系統現象：** 強制要求 `send` 和 `receive` 必須同時發生。發送者必須死等接收者，這在作業系統中稱為 **Rendezvous (交會點)**。
2. **Bounded capacity (有限容量)：**
    * 信箱長度固定為 $n$ 筆訊息。
    * **系統現象：** 若信箱未滿，發送者丟了就走 (Non-blocking)；若信箱已滿，發送者就必須被迫轉為 Blocking 狀態等待。
    * **實務連結：** 在開發 ROS 2 時，YAML 檔或程式碼中設定的 QoS (Quality of Service) 參數 `depth: 10`，就是在底層建立一個 Bounded capacity 為 10 的訊息佇列。
3. **Unbounded capacity (無限容量)：**
    * 理論模型，信箱永遠裝不滿。
    * **系統現象：** 發送者永遠不需要等待 (Never waits)。現實中受限於實體 RAM 限制，一旦耗盡將導致 OOM (Out of Memory) 系統崩潰。
# Client-Server Systems 與 Socket 通訊架構
### 1. 分散式通訊概覽
當系統架構從「單一電腦內的 Process IPC」擴展到「跨越網路的 Client-Server 架構」時，作業系統提供了以下幾種主流的通訊基礎設施：
* **Sockets：** 最底層、最通用的網路通訊端點。
* **RPC (Remote Procedure Calls)：** 遠端程序呼叫，將網路通訊包裝得像呼叫本地 C++ 函式一樣簡單。
* **Pipes：** 管線 (通常用於具有親緣關係的行程或本地 IPC)。
### 2. Socket (通訊端點) 的核心定義與定址
* **核心定義：** Socket 是網路通訊的「端點 (Endpoint)」。任何一段網路通訊，都必定是由「一對 (A pair of)」Sockets 連結而成。
* **二維定址空間 (IP Address + Port)：**
    * 要在全球網路上精準找到另一個 Process，必須依賴這兩個元素的組合：
    1. **IP 位址 (IP Address)：** 負責在茫茫網海中找到「哪一台實體主機 (Host)」。例如：`161.25.19.8`。
    2. **連接埠 (Port)：** 負責在該主機內，找到「哪一個特定的 Process」。一台伺服器裡可能同時跑著 Web 伺服器、資料庫和 ROS2 節點，Port 就像是分機號碼。
    3. **表示法：** `IP:Port`。例如講義圖上的 `146.86.5.20:1625`。
### 3. 系統保留字與特殊定址規範
* **Well-known Ports (知名連接埠)：** 編號 小於 1024 的 Port 保留給標準系統服務使用，一般 User Process 無權隨意佔用（需 Root 權限）。
    * 例如：Port `80` (HTTP 網頁服務)、Port `22` (SSH 遠端連線)。
* **Loopback Address (本機迴路位址)：** `127.0.0.1` (或稱 `localhost`)。
    * **實務極度重要：** 當指定的 IP 是 `127.0.0.1` 時，網路封包不會離開實體網卡流向外部網路，而是在作業系統核心層直接繞回來 (Loop back) 交給本機的另一個 Process。
    * **開發場景：** 在 WSL2 環境下測試 ROS2 節點通訊，或是本地除錯 Web Server 時，預設都是走這個極速的 Loopback 通道。

### 4. 傳輸層協定分類 (Types of Sockets)
在建立 Socket 時，必須宣告這個端點要走哪一種傳輸風格：
* **TCP (Connection-oriented 面向連線)：**
    * **特性：** 可靠、保證抵達、保證順序。傳輸前必須先進行「三向交握」建立連線。
    * **代價：** 速度較慢，Overhead 較高。
* **UDP (Connectionless 無連線)：**
    * **特性：** 不可靠、射後不理 (Fire and forget)、無保證順序。
    * **優勢：** 速度極快。
    * **實務連結：** ROS2 傳遞高頻率的感測器資料 (如 LiDAR 點雲、影像串流)，通常底層會優先採用 UDP，因為偶爾掉一兩幀資料無傷大雅，但「低延遲 (Low Latency)」才是 AMR 避障的救命關鍵。
* **MulticastSocket (多播)：** 允許將一份資料同時廣播給多個指定的接收者。

### 5. Remote Procedure Calls (RPC) 理論與架構
1. RPC 的核心設計哲學：極致的抽象化 (Abstraction)
    * **痛點：** 前面學過的 Socket 通訊雖然強大，但開發者必須親自處理底層的 IP、Port、封包切割、字串轉換等繁瑣細節，容易出錯且程式碼耦合度極高。
    * **RPC 的解法：** 將網路通訊「偽裝」成呼叫本地端的 C++ 函數。開發者只需要寫一行 `result = call_remote_math_function(x, y)`，底層的網路傳輸細節全部由系統代勞，達到徹底的解耦。
2. **隱藏網路細節的功臣：Stubs (代理/存根) 與 Marshalling**
為了解決跨網路的變數傳遞，RPC 架構在 Client 端與 Server 端各安插了一個代理人 (Proxy)，稱為 Stub。

### 6. 隱藏網路細節的功臣：Stubs (代理/存根) 與 Marshalling
為了解決跨網路的變數傳遞，RPC 架構在 Client 端與 Server 端各安插了一個代理人 (Proxy)，稱為 Stub。
* **Client-side stub (客戶端代理)：**
    * 攔截應用程式的函數呼叫。
    * **執行 Marshalling (參數打包)：** 將 C++ 記憶體中的變數 (如整數、字串、結構體) 序列化 (Serialize) 成能在網路上傳輸的位元組串流 (Byte stream)。
    * 負責尋找伺服器並透過 Socket 將封包送出。
* **Server-side stub (伺服器端代理)：**
    * 接收網路封包，執行 **Unmarshalling (參數解包)**。
    * 將解包後的參數餵給 Server 端真正的本地函數去執行。
    * 執行完畢後，再將結果 Marshalling 循原路傳回。
* **💡 實務連結：** 在 ROS 2 中，當定義完 `.srv` 檔案並編譯後，系統自動生成的那些 C++ 標頭檔，其實就是在自動產生這些 Stub 程式碼，省去手寫序列化的麻煩。

### 7. 異質系統的跨平台挑戰：XDR 與 Endianness
當 Client 是一台 ARM 架構的樹莓派，而 Server 是一台 x86 架構的桌上型電腦時，直接傳遞記憶體二進位資料會引發災難。
* **Endianness (位元組順序) 問題：**
    * **Little-endian (小端序)：** 低位元組存在低位址 (多數 x86 系統)。
    * **Big-endian (大端序)：** 高位元組存在低位址 (網路傳輸標準格式)。
* **解決方案：** 透過 **XDR** (External Data Representation, 外部資料表示法) 這種中立格式。Stub 在打包參數時，會統一將資料轉換成系統無關的 XDR 格式，接收端再依據自己的硬體架構將 XDR 轉回原生格式，完美解決跨平台架構 (Architecture) 差異。

### 8. 服務發現機制：Matchmaker / Rendezvous
* **問題：** Server 上可能同時跑了幾十個不同的 RPC 服務，Client 怎麼知道「計算數學」的服務綁定在哪個 Port 上？
* **解決方案：** 作業系統提供了一個 Matchmaker (媒合器/守護行程 daemon)。這是一個運行在固定已知 Port (Well-known port) 上的中介服務。Client 必須先問 Matchmaker：「請問 RPC 服務 X 在哪個 Port？」拿到確切的 Port 號碼後，Client 才會真正與該服務建立 Socket 連線。

### 9. RPC 執行時序與封包流轉 (Execution of RPC)
![image](https://hackmd.io/_uploads/ByoM7KiiWe.png)
在 RPC 架構中，當 Client 端的程式碼嘗試呼叫一個遠端函數 (Procedure X) 時，底層的作業系統核心 (Kernel) 與網路會進行兩大階段、共四次封包來回的交握：
##### 階段一：服務發現 (Service Discovery) - 尋找正確的插座
既然不知道 Procedure X 在伺服器的哪個 Port 上，Kernel 必須先去「問路」。
1. **封包 1 (Client $\rightarrow$ Server Matchmaker)：**
    * Client 端的 Kernel 攔截了使用者的函數呼叫，並將其打包成一個特殊封包。
    * 目的地是伺服器上一個 「固定且眾所皆知的 Port」 (即 Matchmaker 所在的 Port)。
    * 內容：「請問要呼叫 RPC X，應該要去哪個 Port？」
2. **封包 2 (Server Matchmaker $\rightarrow$ Client)：**
    * 伺服器端的 Matchmaker 收到詢問，去查表尋找 RPC X 目前綁定在哪裡。
    * 找到後，將正確的 Port $P$ 包裝成回覆封包，傳回給 Client 端的 Kernel。
3. **封包 3 (Client $\rightarrow$ Server Daemon)：**
    * Client 端的 Kernel 將剛剛拿到的 Port $P$ 填入真實的 RPC 訊息標頭中。
    * 帶著參數 (Contents) 的封包被精準送往伺服器上的 Port $P$。
    * 在 Port $P$ 監聽的 Daemon (背景守護行程) 收到封包，解包後交給真正的伺服器程式進行運算。
4. **封包 4 (Server Daemon $\rightarrow$ Client)：**
    * 伺服器運算完畢，將結果 (``<output>``) 打包。
    * 透過網路傳回給 Client 端的 Kernel。
    * Kernel 再把結果「無縫接軌」地交還給 User 應用程式。對 User 來說，感覺就像是剛剛呼叫了一個稍微慢一點的本地端函數而已。
# Pipe 系統機制深度剖析

延續上一節探討的 `fork()` 複製魔法，當父子行程各自擁有獨立的記憶體空間後，如果它們需要「交換資料」，就必須仰賴作業系統提供的通訊機制。**Pipe (管線)** 就是其中最直覺、最基礎的實作方式。

#### 1. 實作管線的四大設計考量
在設計一條管線時，系統工程師必須面對四個物理限制問題：
1. **方向性**：是單向 (Unidirectional) 還是雙向 (Bidirectional) 通訊？
2. **雙向傳輸模式**：若是雙向，是**半雙工 (Half duplex)**（同一時間只能單向流動，像無線電對講機），還是**全雙工 (Full duplex)**（可同時雙向流動，像講電話）？
3. **關係限制**：通訊的兩個行程是否必須具備特定關係（例如父子行程）？
4. **網路能力**：只能在同一台實體機器內通訊，還是可以跨網路通訊？

根據這四個考量，課本將管線分為兩大類：

---

#### 2. Ordinary Pipes (一般管線 / 在 Windows 稱 Anonymous Pipes)

這是最傳統的生產者－消費者 (Producer-Consumer) 模型。
* **方向性**：嚴格的單向通訊。如果需要雙向對話，開發者必須建立「兩條」管線。
* **關係限制**：**必須存在父子關係**。這條管線無法被行程外部的其他程式看見或存取。

**【UNIX 底層實作邏輯 (結合 Figure 3.20 與 3.21)】**
![image](https://hackmd.io/_uploads/r1kkl3O2Zx.png)
![image](https://hackmd.io/_uploads/BJUJeh_2bx.png)
![image](https://hackmd.io/_uploads/HyYGl2Oh-e.png)

這段實作完美結合了 C 語言、作業系統核心與剛才學過的 `fork()`：

1. **建立管線**：父行程先呼叫 `pipe(int fd[])`。作業系統會在 Kernel 裡開闢一塊記憶體當作管線，並回傳兩個檔案描述子 (File descriptors)：
   * `fd[0]`：管線的**讀取端 (Read end)**。
   * `fd[1]`：管線的**寫入端 (Write end)**。
   *(記憶小訣竅：0 類似 Standard Input，1 類似 Standard Output)*
2. **繼承與複製**：接著父行程呼叫 `fork()` 建立子行程。如同上一節所述，子行程會完美繼承父行程開啟的所有檔案，因此**子行程也同時擁有了這條管線的 `fd[0]` 與 `fd[1]`**。
3. **關閉未使用的端點 (極度重要)**：
   * 為了確保單向通訊的乾淨與安全，假設父行程要傳訊息給子行程。
   * 父行程必須主動 `close(fd[0])` (關閉自己的讀取端)。
   * 子行程必須主動 `close(fd[1])` (關閉自己的寫入端)。
   * **關閉端點的物理意義**：這能確保當寫入方關閉管線時，讀取方能正確收到 `EOF (End-of-File)` 訊號（`read()` 函數會回傳 0），進而結束等待。如果不關閉，讀取方會以為管線還開著，導致程式永遠卡死在等待狀態。

#### 3. Named Pipes (具名管線)

一般管線雖然簡單，但「必須是父子關係」這點在開發複雜的大型系統時非常不方便。若有兩支獨立開發、各自執行的程式想要交換資料，就需要 **Named Pipes**。

* **無血緣限制**：作業系統會給這條管線一個真實存在的「名稱」（在檔案系統中看起來就像一個特殊檔案），任何知道這個名稱的行程，都可以連線進來進行讀寫。
* **UNIX 與 Windows 的系統差異**：
  * **UNIX (稱為 FIFOs)**：雖然打破了關係限制，但依然只支援**半雙工 (Half-duplex)** 傳輸，且所有行程必須在**同一台實體機器**上。若要跨機器，開發者必須改用 Sockets (通訊端)。
  * **Windows**：微軟提供了更強大的機制。Windows 的 Named Pipes 支援**全雙工 (Full-duplex)** 雙向同時傳輸，而且允許通訊的行程位在**不同的實體機器**上，可以直接透過網路交換資料。

---
