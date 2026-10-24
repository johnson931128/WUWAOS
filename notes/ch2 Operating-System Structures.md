---
title: ch2 Operating-System Structures

---

## 作業系統服務與命令直譯器深度剖析
![image](https://hackmd.io/_uploads/SyoEznw3bg.png)

進入第二章，視角從第一章的「硬體與資源」轉移到了「軟體與服務」。作業系統本質上就是一個巨大的服務提供者 (Service Provider)。依照課本的邏輯，這份菜單必須一字不漏地詳細拆解。

#### 1. 輔助使用者的六大服務 (User-Helpful Services)
這組服務的唯一目標，是讓開發者與使用者能輕鬆地使用硬體，而不必親自去寫控制晶片的組合語言。

* **User interface (UI / 使用者介面)**：
  系統必須提供操作入口。現代系統通常混合提供三種介面：滑鼠驅動的 **GUI**、行動裝置的 **Touch-screen**，以及工程師最常使用的 **CLI (Command-line interface)**。
* **Program execution (程式執行)**：
  系統必須具備將程式從硬碟載入記憶體 (Load into memory)、開始執行，並妥善處理正常結束或異常崩潰 (Error) 的能力。
* **I/O operations (I/O 操作)**：
  為了效率與系統保護，一般程式**絕對不允許**直接操控實體硬體。如果程式需要讀取雷達感測器或寫入隨身碟，系統必須提供專屬的服務通道來代勞。
* **File-system manipulation (檔案系統操作)**：
  除了基礎的讀、寫、建立、刪除檔案與目錄，系統還必須提供「搜尋」功能，以及最重要的**權限管理 (Permissions management)**，依據檔案所有權來允許或拒絕存取。
* **Communications (通訊)**：
  當多個 Process 需要交換資訊時（例如導航節點要將座標傳給馬達控制節點），系統提供兩種主要途徑：
  1. **Shared memory (共享記憶體)**：兩個 Process 直接讀寫同一塊記憶體區域。
  2. **Message passing (訊息傳遞)**：由作業系統負責將資訊封包在兩個 Process 之間搬運。
* **Error detection (錯誤偵測)**：
  系統必須無時無刻監控硬體（如記憶體錯誤、斷電）、I/O 設備（如網路斷線），以及使用者程式（如算術溢位、存取非法記憶體）。一旦發生錯誤，系統必須採取適當行動（終止程式或回傳錯誤碼）以確保運算的一致性。

#### 2. 維護系統高效運作的三大服務 (System-Efficient Services)
當系統同時執行數百個程式時，作業系統必須轉變為冷酷的資源管理者。

* **Resource allocation (資源分配)**：
  分配 CPU 週期 (透過 CPU 排程器)、分配主記憶體與檔案儲存空間。針對特殊的周邊設備（如 USB），也必須有通用的請求與釋放機制。
* **Logging (日誌與統計)**：
  系統必須精確記錄哪個程式使用了多少資源。這不僅是為了商業計費 (Accounting)，更是系統管理員重新配置系統、優化效能的重要數據來源。
* **Protection and security (保護與安全)**：
  這兩個名詞在定義上必須嚴格區分：
  * **Protection (保護)**：屬於「系統內部」的防禦。確保多個併發執行的 Process 之間不會互相干擾，且所有對系統資源的存取都受到嚴格控制。
  * **Security (安全)**：屬於「抵禦外部」的防禦。要求使用者進行身分驗證 (Authentication/密碼)，並防禦外部 I/O 或網路的非法入侵。

---

#### 3. 深入終端機：2.2.1 Command Interpreters (命令直譯器)
在沒有圖形介面的純文字伺服器或開發環境中，**Command Interpreter** 是使用者與系統溝通的唯一管道。

* **Shell (殼層)**：在擁有多種直譯器可供選擇的系統（如 UNIX/Linux）中，直譯器被統稱為 Shell。常見的包含 Bourne-Again shell (bash)、C shell 等。
* **指令實作的兩大流派 (考試絕對重點)**：
  當使用者在終端機輸入一行刪除檔案的指令時，直譯器有兩種完全不同的底層處理方式：

  * **流派一：內建程式碼 (Built-in approach)**
    直譯器程式內部本身就包含了執行該指令的邏輯。當輸入刪除指令時，直譯器會直接跳轉到自己程式碼的某個區段去呼叫 System call。
    * **缺點**：系統支援的指令越多，直譯器這支程式的體積就會變得越龐大。
  * **流派二：系統程式檔案 (System programs approach / UNIX 哲學)**
    **這是 UNIX 與 Linux 採用的優雅設計**。直譯器本身「完全不懂」指令的意義。當輸入 `rm file.txt` 時，直譯器的唯一動作是：
    1. 在硬碟中尋找一個名為 `rm` 的檔案。
    2. 將這個檔案當作一支獨立的系統程式載入記憶體。
    3. 將 `file.txt` 當作參數傳遞給它並執行。
    * **絕對優勢**：直譯器的程式體積可以維持得極度小巧。若工程師想要為系統新增一個自創的指令，完全不需要去修改直譯器的原始碼，只需要寫一支新程式，丟進系統的特定目錄中即可。

---


---

## 系統呼叫理論與圖片解析

延續第一章探討的「雙重模式 (Dual-Mode)」，當應用程式處於 User Mode 時，是完全沒有權力直接觸碰硬碟的。若要讀取檔案，就必須發起 **System Call (系統呼叫)**。

#### 1. 圖片深度解析 (Figure 2.5 實務拆解)
![image](https://hackmd.io/_uploads/HkiNoTP3bl.png)
課本透過一個極度日常的動作：**「將來源檔案複製到目的檔案 (Copy file)」**，來展示底層系統呼叫的真實運作。這對應到 UNIX/Linux 系統中的 `cp in.txt out.txt` 指令。

觀察 **Figure 2.5** 的藍色區塊，這是一個完整的系統呼叫序列 (System-Call Sequence)。一個簡單的複製動作，作業系統底層必須扛下以下所有苦工：

1.  **取得檔名 (Acquire file names)**：
    如果是互動式介面，程式必須呼叫系統將提示字元寫入螢幕 (`write`)，接著讀取鍵盤輸入 (`read`) 來取得來源與目的檔名。
2.  **開啟來源檔 (Open the input file)**：
    向 OS 請求開啟檔案 (`open`)。此時 OS 必須切換到 Kernel Mode，檢查檔案是否存在、當下執行程式是否具備讀取權限。若發生錯誤，必須發出終止訊號 (`abort`)。
3.  **建立目的檔 (Create output file)**：
    請求在硬碟上建立新檔案 (`creat` 或帶有特定旗標的 `open`)。若發現同名檔案已存在，OS 必須依照指令邏輯，決定是直接覆寫、先刪除再建立，還是回傳錯誤。
4.  **核心讀寫迴圈 (Loop: Read & Write)**：
    這是最消耗硬體資源的階段。程式會不斷呼叫 `read` 從來源檔抓取一小塊資料放入記憶體，接著呼叫 `write` 將該資料寫入目的檔。這個迴圈會持續進行，直到讀取動作回報「已達檔案結尾 (End of File)」。
5.  **關閉與終止 (Close & Terminate)**：
    複製完成後，必須呼叫 `close` 通知 OS 釋放檔案鎖定與記憶體緩衝區。最後呼叫 `exit` 正常結束程式，將控制權徹底交還給命令直譯器 (Shell)。

#### 2. System Call 與 API 的物理差異
實務開發上，工程師極少直接撰寫觸發硬體中斷 (Trap) 的組合語言程式碼。
* **底層介面 (System Call)**：作業系統核心直接提供的介面。例如 Linux 核心提供的 `sys_open`, `sys_read`。
* **應用程式介面 (API)**：高階語言的標準函式庫。例如 C++ 的 `<fstream>` (`std::ifstream`, `std::ofstream`) 或 C 語言的 `<stdio.h>` (`printf`, `fopen`)。
* **運作關係**：當開發者在 C++ 中呼叫 `std::ifstream::read()` 時，C++ 函式庫會在底層自動準備好所有需要的暫存器參數，並精準發動一個 `read()` 系統呼叫，跨越邊界進入作業系統核心。

---

#### 3. 系統底層開發的實務考量

在開發底層微型作業系統 (Micro-OS) 或處理高效能 I/O 程式時，必須意識到 Figure 2.5 迴圈中的效能陷阱。

每次呼叫 `read` 或 `write`，CPU 都必須經歷一次「狀態保存 $\rightarrow$ 切換至 Kernel Mode $\rightarrow$ 參數驗證 $\rightarrow$ 硬體操作 $\rightarrow$ 切換回 User Mode $\rightarrow$ 狀態還原」的完整 Context Switch 過程。

如果實作複製檔案的程式時，每次迴圈只讀寫 **1 個 Byte** 的資料，對於一個 1MB 的檔案，就會觸發高達 **1,000,000 次**的系統呼叫，這會讓系統的 CPU 算力全部浪費在 Mode Bit 切換的純耗損上。因此，實務上必定會宣告一個大小適中的緩衝區 (Buffer，例如 4KB 或 8KB)，讓每次系統呼叫都能搬運更多的資料，藉此將跨界次數壓縮到最低。

---

## API 理論、圖表與底層架構深度剖析

延續上一節探討的系統呼叫，實務上，即使是簡單的程式每秒也會觸發數以千計的系統呼叫。若每次都要手動處理 User/Kernel Mode 切換與中斷向量，開發效率將趨近於零。

#### 1. API 與 RTE 的系統地位
* **API (Application Programming Interface)**：
    定義了一組標準的函數，包含函數名稱、傳入參數與預期回傳值。開發者只需依照規格呼叫即可。
    * **三大主流標準**：Windows 系統的 **Windows API**、UNIX/Linux/macOS 共用的 **POSIX API**，以及運行在虛擬機上的 **Java API**。
    * 在 C 語言環境中，負責提供這些 API 函數的標準函式庫稱為 **`libc`**。
* **RTE (Run-Time Environment / 執行環境)**：
    單靠 API 規格無法執行程式。系統必須提供完整的 RTE，包含編譯器、直譯器、載入器 (Loaders) 以及上述的函式庫，才能將高階語言轉換為實體的系統呼叫。

#### 2. 經典 API 拆解：`read()` 函數
課本以 UNIX/Linux 的標準讀取函數為例（透過終端機輸入 `man read` 可查閱）：
`ssize_t read(int fd, void *buf, size_t count)`
* **`int fd`**：檔案描述子 (File descriptor)，告訴系統要讀取哪一個已經開啟的檔案。
* **`void *buf`**：記憶體緩衝區的指標，告訴系統讀取出來的資料要存放在哪裡。
* **`size_t count`**：整數，限制系統這次最多只能讀取多少個位元組 (Bytes)。
* **回傳值 (Return value)**：若成功，回傳實際讀取的位元組數量；若回傳 `0`，代表已達檔案結尾 (End of file)；若發生錯誤，則回傳 `-1`。

#### 3. 圖表解析：Figure 2.6 (系統呼叫的攔截與查表)
![image](https://hackmd.io/_uploads/BJr7pTDhZe.png)
這張圖揭示了 API 底層的物理流轉過程：
1.  **呼叫發起**：User application 呼叫了高階 API `open()`。
2.  **介面攔截**：呼叫進入位於 User Space 的 **System-call interface (系統呼叫介面)**。
3.  **查表跳轉 (Table Lookup)**：介面內部有一張對應表。它查出 `open()` 對應的系統呼叫編號（假設為 `i`），接著觸發軟體中斷 (Trap)，並將編號 `i` 傳遞給硬體。
4.  **核心執行**：硬體切換至 Kernel Mode。作業系統核心根據編號 `i` 找到真正的 `sys_open()` 實作程式碼並執行。
5.  **狀態回傳**：執行完畢後，將結果（如檔案描述子或錯誤碼）交還給系統呼叫介面，最後回到 User application 繼續執行。

#### 4. 圖表解析：Figure 2.7 (參數傳遞機制)
![image](https://hackmd.io/_uploads/HJLU6TDnbg.png)
當呼叫系統服務（如上述的 `read`）時，必須將 `fd`、`buf` 位址等參數交給作業系統。但 User Mode 與 Kernel Mode 的記憶體是隔離的，傳遞參數有三種物理途徑：
* **機制一：暫存器 (Registers)**
    最簡單且最快的方法。直接把參數塞進 CPU 內部的暫存器。但物理限制在於，CPU 的暫存器數量極少（通常若超過 5 個參數便無法使用此法）。
* **機制二：記憶體區塊 (Block / Table - Figure 2.7 圖示)**
    為了解決暫存器不足的問題。程式將所有參數連續寫入主記憶體的一塊「區塊 (Block)」中。接著，**只把這塊記憶體的「起始位址 (Address X)」放進暫存器中**，傳遞給作業系統。OS 進入 Kernel Mode 後，再循著這個位址去記憶體把參數抓出來。
* **機制三：堆疊 (Stack)**
    程式利用之前學過的 **Push** 指令，將參數依序壓入系統堆疊中；作業系統切換模式後，再使用 **Pop** 指令將參數一一取出。
    *區塊與堆疊機制的絕對優勢在於：它們完全不受限於參數的數量或長度。*

---

## Process Control 行程控制的核心機制
![image](https://hackmd.io/_uploads/S1Kke0D2We.png=50%)

依照課本的架構，System Calls (系統呼叫) 被分為六大主要類別（如 Figure 2.8 所示）：**Process control (行程控制)**、**File management (檔案管理)**、**Device management (裝置管理)**、**Information maintenance (資訊維護)**、**Communications (通訊)** 以及 **Protection (保護)**。

本節針對第一大類 **Process Control (行程控制)** 進行第一階段的詳細拆解，探討一個程式從執行、發生錯誤到建立子行程的底層運作邏輯。

### 1. 執行與終止 (Execution and Termination)
正在執行中的程式 (Running program) 必須具備停止執行的能力，這分為兩種系統呼叫：
* **正常終止：** 呼叫 `end()`。
* **異常終止：** 呼叫 `abort()`。

### 2. 錯誤處理與除錯 (Error Handling and Debugging)
如果程式執行遭遇問題並引發 Error trap (錯誤陷阱)，或者程式主動發出異常終止，系統的標準處理流程如下：
##### 1. Memory dump (記憶體傾印)：
作業系統會將當下記憶體的狀態抓取下來，寫入磁碟中一個特殊的日誌檔 (Log file)。
##### 2. Debugger (除錯器) 介入：
這是一支專門的 System program (系統程式)，程式設計師可以透過它來檢查剛才產生的傾印檔，藉此找出並修正程式碼中的 Bugs (錯誤)。
##### 3. 交還控制權：
無論是正常或異常終止，作業系統最終都必須將控制權交還給呼叫它的 Command interpreter (命令直譯器)。
* 在互動式系統 (Interactive system) 中，直譯器會繼續等待下一個指令。
* 在 GUI 系統中，可能會彈出一個視窗警告使用者發生錯誤，並尋求下一步指示。
### 3. Error Level (錯誤層級) 與自動復原
為了讓系統能具備自動應變能力，程式在異常終止時，可以定義一個 **Error level (錯誤層級)** 參數：
* 層級 `0` 通常代表正常終止。
* 更高的數值代表更嚴重的錯誤。透過這個層級數值，Command interpreter 或是接續執行的程式，就能「自動」判斷該採取什麼復原行動 (Recovery actions)，而不需要人工介入。

### 4. 載入與並發建立 (Loading and Concurrent Creation)
一個正在執行的行程，可能需要呼叫 load() 和 execute() 來啟動另一支程式（例如使用者點擊滑鼠開啟新軟體）。這裡會衍生出一個關鍵的系統架構問題：「當新程式執行結束後，控制權該回到哪裡？」
* **保存現有狀態：** 如果控制權必須回到「原本的舊程式」，作業系統就必須將舊程式的 **Memory image** (記憶體映像) 妥善保存在記憶體中。
* **並發執行 (Concurrent execution)：** 如果新舊兩支程式同時繼續執行，這意味著系統成功進行了多重程式設計 (Multiprogrammed)，這個專屬的動作對應的系統呼叫就是 `create_process()` (建立行程)。

### 5. 屬性控制與事件等待 (Attributes and Event Waiting)
當建立了新的行程後，系統必須能夠控制它：
* **屬性控制：** 透過 `get_process_attributes()` 與 `set_process_attributes()`，可以讀取或重設行程的優先權、最大允許執行時間等參數。
* **強制終止：** 若發現建立的行程執行有誤或不再需要，可呼叫 `terminate_process()` 強制結束它。
* **等待機制：** 行程間經常需要協同工作。一個行程可以呼叫 `wait_time()` 等待一段特定時間，或者呼叫 `wait_event()` 進入休眠，直到另一個行程發出 `signal_event()` (發送事件訊號) 喚醒它。

### 6. 共享資料與鎖定 (Shared Data and Locking)
當兩個或多個行程需要共享同一份資料時，為了確保資料的完整性 (Integrity)，作業系統提供了允許行程 Lock (鎖定) 共享資料的系統呼叫（如 `acquire_lock()` 與 `release_lock()`）。一旦資料被鎖定，其他行程就絕對無法存取，直到鎖被釋放。這正是後續章節（Chapter 6 與 7）將深入探討的並行處理難題。
## 單一任務與多工作業的物理實作對照
當系統記憶體極度微小，根本無法同時容納「命令直譯器」與「新程式」時，系統的運作型態將發生本質上的改變。課本 2.3.3.1 節後半段，直接舉了兩個極端的硬體平台來做對照：
### 1. 資源受限的極端：Arduino (Single-tasking system)
在微控制器 (Microcontroller) 環境中（如 Arduino 或 AMR 底層常使用的 ESP32），硬體資源極度稀缺。
![image](https://hackmd.io/_uploads/rkNmzRPnbg.png)
* **沒有作業系統：** 這類平台通常不具備傳統意義上的作業系統，也沒有圖形介面或終端機。
* **運作邏輯 (對應 Figure 2.9)：**
    1. 硬體通電後，記憶體底層的一小段韌體 **Boot loader (開機載入程式)** 會先被喚醒。
    2. 開發者在 PC 上編譯好名為 **Sketch (草稿碼/應用程式)** 的二進位檔，透過 USB 傳輸到微控制器的 Flash memory 中。
    3. Boot loader 將這個 Sketch 載入主記憶體並開始執行。
* **物理替換：** 因為是 **單一任務系統 (Single-tasking system)**，記憶體同一時間只能容納一支 Sketch。若要執行新程式，就必須重新燒錄，**新程式會完全物理覆寫** (Replace) 掉舊的程式。

### 2. 資源充沛的極端：FreeBSD / UNIX (Multitasking system)
將視角切換到擁有龐大記憶體的伺服器或單板電腦（如 Raspberry Pi）。這裡運行的是完整的 多工作業系統 (Multitasking system)。
![image](https://hackmd.io/_uploads/ryftM0D2Wx.png)
* **共存狀態 (對應 Figure 2.10)：** 記憶體被妥善劃分。作業系統的 Kernel (核心) 永遠常駐在高位址區域，而下方則同時擠滿了 Command interpreter (Shell / 直譯器) 以及多個正在執行的 Processes (行程)。
* **UNIX 行程建立的黃金三角 (系統呼叫)：**
    * 當使用者在 Shell 輸入指令要求執行新程式時，UNIX 系統會發動一連串精妙的系統呼叫：
    1. **`fork()` (分支/複製)：** Shell 首先呼叫 `fork()`。這個動作會直接在記憶體中「複製」出一個與自己一模一樣的子行程。此時，記憶體裡有兩個 Shell。
    2. **`exec()` (執行/替換)：** 剛複製出來的子行程會立刻呼叫 `exec()`，並將使用者指定的新程式檔案載入。這個動作會把子行程原本的 Shell 記憶體映像，徹底替換 (Replace) 成新程式的記憶體映像。
    3. **背景與等待 (Background & Wait)：** 新程式開始執行。此時原本的父行程 (Shell) 可以選擇休眠等待 (呼叫 `wait()`)，或是將新程式放入 **Background (背景)** 執行，讓 Shell 立刻空出來接收使用者的下一個指令。
    4. **`exit()` (離開)：** 新程式執行完畢，呼叫 `exit()` 終止，並回傳 Status code (狀態碼，如 0 代表成功) 給等待中的 Shell。

### 3. 為什麼它被命名為「Shell (殼)」？
![image](https://hackmd.io/_uploads/S1gYXAv2-x.png)
如果把作業系統想像成一顆堅果：
* **Kernel（果核/核心）：** 負責控制 CPU、記憶體與硬碟。它擁有絕對的權力，但只聽得懂冰冷的二進位機器碼與 System Calls（系統呼叫）。
* **Shell（果殼/外殼）：** 包覆在核心外面。人類看不懂二進位碼，核心也聽不懂人類的英文。因此我們需要一個「翻譯官」，這支名為 Shell 的程式碼負責接收人類打出來的英文字串（如 `ls`, `rm`），然後將其翻譯成 Kernel 聽得懂的系統呼叫，交給核心去執行。
* 它同樣是跑在「User Mode（使用者模式）」下的一支軟體。

### 4. Shell 這支程式碼的內部邏輯是什麼？
要自己用 C 語言寫一個最陽春的 Shell，它的主程式結構其實就是一個永無止盡的無窮迴圈 (Infinite Loop)：
1. 讀取 (Read)：在螢幕上印出提示字元（例如 $），然後讓 CPU 停下來，等待使用者用鍵盤輸入一串字。
2. 解析 (Parse)：將使用者輸入的字串切開。例如把 rm file.txt 切成指令 rm 與參數 file.txt。
3. 尋找 (Search)：在硬碟的特定目錄（如 $PATH）裡面，尋找有沒有一支叫做 rm 的執行檔。
4. 委託執行 (Execute)：
    * 如果找不到，就在螢幕上印出 "Command not found"。
    * 如果找到了，Shell 會發動 System Call（呼叫 fork() 複製自己，接著呼叫 exec() 載入 rm 這支程式），請 Kernel 幫忙執行。
5. 等待與印出 (Wait & Print)：Shell 呼叫 wait() 進入休眠，等 rm 執行完畢後，把結果印在螢幕上。接著回到步驟 1，繼續印出 $ 等待下一個指令。

---
## 系統呼叫五大類型詳述 (Types of System Calls)

接續前一節的行程控制，作業系統為了完整管理底層資源，將剩下的系統呼叫精細劃分為以下五大領域。

#### 2.3.3.2 File Management (檔案管理)
程式執行時，無可避免地需要與硬碟中的資料互動。系統呼叫提供了一套標準的檔案生命週期管理：
* **基礎操作**：包含 `create()` (建立) 與 `delete()` (刪除)。檔案建立後，必須 `open()` (開啟) 才能使用，接著進行 `read()` (讀取)、`write()` (寫入) 或 `reposition()` (重新定位，例如跳到檔案結尾)。使用完畢後必須呼叫 `close()` (關閉) 以釋放系統資源。
* **目錄與屬性**：同樣的操作也適用於目錄結構。此外，系統必須提供 `get_file_attributes()` 與 `set_file_attributes()` 來讀取或修改檔案名稱、類型與保護代碼。
* **實務設計差異**：有些作業系統會直接提供一個 `copy()` 系統呼叫；但多數系統（如 UNIX）傾向保持核心輕量，讓「複製檔案」這件事交由系統程式（如 `cp` 指令）透過連續呼叫 `read()` 與 `write()` 來完成。

#### 2.3.3.3 Device Management (裝置管理)
執行中的行程常需要申請額外資源，例如存取主記憶體、磁碟機或特殊的硬體感測器。
* **請求與釋放**：程式必須呼叫 `request()` (請求) 取得裝置的獨佔控制權。若裝置忙碌，行程必須等待。使用完畢後呼叫 `release()` (釋放)。這與檔案的開啟/關閉概念極度相似。
* **一切皆檔案 (UNIX 哲學)**：硬體裝置（如硬碟讀寫頭）與檔案的讀寫行為高度重疊。因此，UNIX 作業系統將這兩者合併為「檔案-裝置結構 (File-device structure)」。在此設計下，I/O 裝置被視為特殊的檔案，可直接套用檔案的 `read()` 與 `write()` 系統呼叫，大幅簡化了系統設計。

#### 2.3.3.4 Information Maintenance (資訊維護)
這類系統呼叫單純作為「使用者程式」與「作業系統」之間的資訊橋樑。
* **系統狀態查詢**：如取得目前的系統時間 `time()`、日期 `date()`、作業系統版本、剩餘記憶體或硬碟空間。
* **底層除錯 (Debugging) 利器**：
    * **`dump()`**：當程式崩潰時，將當下的記憶體狀態傾印出來供事後分析。
    * **`strace`**：Linux 系統中的強大工具，可列出程式執行過程中觸發的所有系統呼叫。
    * **Single step (單步執行)**：微處理器提供的一種 CPU 模式，每執行一行指令就會觸發一次中斷 (Trap)，通常交由 Debugger (除錯器) 捕捉，用以精確追蹤程式邏輯。
* **效能分析 (Time profile)**：系統可利用計時器中斷，定期記錄程式計數器 (Program counter) 的位置，藉此統計程式在各個區塊花費的時間。

#### 2.3.3.5 Communication (通訊)
當多個行程需要協同工作（例如感測器節點將資料傳給導航節點）時，必須仰賴行程間通訊 (Interprocess Communication, IPC)。系統提供兩種核心模型：

* **Model 1: Message-passing (訊息傳遞模型)**
    * **運作邏輯**：行程之間透過交換訊息來溝通。雙方必須先建立連線 (`open_connection()`)。在網路上，這需要知道對方的 **Host name (主機名稱/IP)** 與 **Process name (行程名稱)**，並翻譯成系統識別碼。
    * **Client-Server 架構**：接收端通常是特定的系統背景程式，稱為 **Daemons (守護行程)**。它會呼叫 `wait_for_connection()` 等待。發起通訊的一方稱為 **Client (客戶端)**，接收方稱為 **Server (伺服器)**。連線後透過 `read_message()` 與 `write_message()` 傳輸資料。
    * **優勢**：實作簡單，適合少量資料交換，且非常容易擴展到跨電腦的網路通訊。
* **Model 2: Shared-memory (共享記憶體模型)**
    * **運作邏輯**：呼叫 `shared_memory_create()` 與 `shared_memory_attach()`。作業系統原本會嚴格禁止行程讀取他人的記憶體，此模型會「解除」這項限制，讓多個行程將同一塊實體記憶體映射到各自的空間中。
    * **絕對效能與代價**：由於資料不需要透過作業系統封包搬運，傳輸速度等同於極速的記憶體讀寫速度。但代價是，作業系統不再介入管控，程式開發者必須自行實作「同步機制 (Synchronization)」來防止多個行程同時寫入同一記憶體位址引發的資料毀損。

#### 2.3.3.6 Protection (保護)
在多重程式設計 (Multiprogrammed) 或連網的環境中，保護機制是維持系統秩序的最後防線。
* **資源存取控制**：提供控制系統資源存取權的機制。包含設定與取得檔案權限的 `set_permission()` 與 `get_permission()`。
* **使用者層級管控**：透過 `allow_user()` 與 `deny_user()` 等系統呼叫，精細規定特定使用者是否允許存取某些磁碟或檔案。
* 備註：此處的 Protection 著重於系統「內部」的存取控制，而防禦外部威脅（如網路攻擊）則屬於更龐大的 Security (安全) 範疇。
---
## Linker 與 Loader 底層運作邏輯剖析
* **【專有名詞翻譯對照】**
* **Compiler**：編譯器
* **Relocatable object file**：可重定位物件檔
* **Linker**：連結器
* **Loader**：載入器
* **Binary executable file**：二進位執行檔
* **Relocation**：重定位
* **Dynamically linked libraries (DLLs)**：動態連結函式庫
* **ELF (Executable and Linkable Format)**：可執行與可連結格式
* **Entry point**：進入點
觀察 **Figure 2.11**，這是一個極度標準的 C 語言程式生命週期，將其拆解為三大階段：
![image](https://hackmd.io/_uploads/HJQfDCv2bg.png)

#### 1. 階段一：編譯 (Compile) 與可重定位物件檔
當開發者寫好 `main.c` (原始碼) 並執行編譯指令 (`gcc -c main.c`) 後，Compiler 會將人類閱讀的文字翻譯成機器語言，產出 **`main.o` (Relocatable object file / 可重定位物件檔)**。
* **物理特性**：此時的機器碼還處於「相對位址」狀態。它只知道變數 A 在檔案的第 100 行，但**不知道未來被作業系統放進實體記憶體時，真正的位址在哪裡**。因此稱為「可重定位 (Relocatable)」。

#### 2. 階段二：連結 (Link) 與執行檔的誕生
現代程式通常不會只寫在一個檔案裡，且必定會引用標準函式庫（如處理數學運算的 Math library）。
* **Linker 的介入**：執行 `gcc -o main main.o -lm` 時，Linker 會將 `main.o` 與其他必須的物件檔黏合在一起，解決所有跨檔案的函數呼叫參考，最終產出一個名為 `main` 的 **Binary executable file (二進位執行檔)**。
* **動態連結 (Dynamically Linked Libraries, DLLs)**：
    如果把所有用到的函式庫全部塞進執行檔，檔案體積會變得極度巨大。現代系統的作法是：Linker 只會在執行檔裡塞入一個「標記 (Relocation information)」，告知系統「這支程式需要用到數學函式庫」。直到程式真正要執行時，才把函式庫載入。這項技術在 Windows 稱為 DLL，在 Linux 稱為 Shared Object (`.so`)。

#### 3. 階段三：載入 (Load) 與重定位 (Relocation)
當在 UNIX 終端機輸入 `./main` 時，系統底層會發生以下連鎖反應：
1.  Shell 呼叫 `fork()` 建立一個新的空白行程。
2.  呼叫 `exec()` 喚醒 **Loader (載入器)**。
3.  Loader 將 `main` 這個執行檔從硬碟複製到主記憶體中。
4.  **Relocation (重定位) 的最終落實**：Loader 根據當下記憶體的空閒狀態，為程式的各個段落分配「最終的實體記憶體位址 (Final addresses)」，並將程式碼內的變數位址全部校正。
5.  CPU 尋找檔案格式 (如 ELF) 標示的 **Entry point (進入點)**，開始執行第一道指令。


---
## 作業系統設計與實作理論剖析

當了解了系統呼叫與底層資料結構後，課本 2.7 節將視角拉高，探討作業系統這套龐大軟體在「工程面」的設計哲學。

#### 1. 衝突的設計目標 (2.7.1 Design Goals)
開發一套全新的作業系統時，第一步是確定硬體環境（是桌上型電腦、手機，還是即時控制系統？）。接著會面臨兩大類需求：
* **使用者目標 (User goals)**：使用者希望系統好學、好用、可靠、安全且速度極快。
* **系統目標 (System goals)**：工程師希望系統容易設計、實作與維護，並且運作充滿效率。

這兩組目標常常是衝突的。例如：為了達成使用者的「絕對安全」，系統可能需要加入極度複雜的權限檢查，這就會破壞系統目標中的「容易實作」與「運作效率」。因此，針對企業伺服器設計的 Windows Server，與針對嵌入式硬體設計的 VxWorks (即時作業系統 Real-time OS)，其底層架構會完全不同。

#### 2. 軟體工程的黃金法則：機制與策略 (2.7.2 Mechanisms and Policies)
這是設計作業系統（甚至任何大型軟體）時最重要的觀念：**千萬不要把規定寫死在工具裡**。

* **機制 (Mechanism)**：提供一個「可以達成某件事的工具」，決定 **How (如何做)**。
* **策略 (Policy)**：決定「這個工具要怎麼被使用」，決定 **What (做什麼)**。

**【經典硬體範例】**：
在第一章學過的 CPU Timer (計時器)。
* **機制**：硬體提供了一個倒數計時器，時間到了就會發出中斷 (Interrupt) 把 CPU 控制權搶回來。
* **策略**：系統管理員決定「普通程式只能跑 10 毫秒」、「高優先權程式可以跑 100 毫秒」。

**為什麼要分離？**
如果未來想要改變規定（策略改變：所有程式都只能跑 5 毫秒），只要修改設定檔即可。底層的計時器程式碼（機制）一行都不用改。這種作法讓系統變得非常有彈性。如果把規定和作法混在一起寫，只要改一個小規定，整個底層核心碼就要重新編譯，這在工程上是一場災難。

#### 3. 實作語言的妥協與進化 (2.7.3 Implementation)
早期的作業系統（如早期的 MS-DOS）是純用 **組合語言 (Assembly language)** 刻出來的，因為這樣跑得最快。但現在的系統（如 Linux, Windows, Android）幾乎都是用 **高階語言 (Higher-level languages)** 如 C 或 C++ 開發的。

以 Android 架構為例：
* **最底層核心 (Kernel)**：主要是 C 語言，加上極少數控制硬體的組合語言。
* **系統函式庫 (System libraries)**：使用 C 或 C++。
* **應用程式框架 (Application frameworks)**：提供給開發者的介面，使用 Java 開發。

**放棄純組合語言的原因：**
1. **開發效率與可維護性**：高階語言寫得快、程式碼短、容易看懂，也容易除錯 (Debug)。
2. **跨平台能力 (Portability)**：用 C 語言寫的作業系統，只要換一個編譯器 (Compiler)，就能輕鬆搬到 Intel 晶片或 ARM 晶片上跑。組合語言則完全綁死在單一硬體架構上。
3. **編譯器技術的進步**：現代處理器有極度複雜的管線化 (Pipelining) 技術。現代 Compiler 進行的最佳化分析，寫出來的機器碼往往比資深工程師手寫的組合語言還要精準、高效。

真正的效能瓶頸，通常是「使用了糟糕的資料結構與演算法」，而不是「沒有用組合語言」。

---

## 微核心理論與架構深度剖析

#### 1. 歷史背景：為何需要微核心？
早期的 UNIX 系統採用的是「單體式架構 (Monolithic structure)」，也就是把檔案管理、記憶體管理、硬體驅動程式全部塞進同一個 Kernel 裡。這種設計跑得非常快，但隨著硬體越來越複雜，Kernel 的程式碼膨脹到難以除錯與維護的地步。一旦某個冷門印表機的驅動程式寫錯產生 Bug，整個作業系統就會遭遇 Kernel Panic（藍白當機），全系統直接死機。

為了收拾這個殘局，卡內基美隆大學 (CMU) 開發了 Mach 作業系統，正式引入 **Microkernel (微核心)** 的概念。

#### 2. 🚨 必考圖表拆解：Figure 2.15 物理架構
要精準畫出這張圖，必須掌握「權力下放」的邏輯。
![image](https://hackmd.io/_uploads/H1zxY0vhZl.png)

* **頂層 (User Mode)**：
    原本擁有極高特權的 **File system (檔案系統)** 與 **Device driver (裝置驅動程式)**，全被踢出了特權階級，降級成跟一般 **Application program (應用程式)** 一樣的 User-level programs。它們各自住在獨立的記憶體空間。
* **底層 (Kernel Mode / Microkernel)**：
    核心被「減肥」到極致。微核心內部只剩下三塊絕對無法移除的基石：
    1.  **Interprocess communication (IPC / 行程間通訊)**：負責當所有服務的「郵差」。
    2.  **Memory management (記憶體管理)**。
    3.  **CPU scheduling (CPU 排程)**。
* **通訊流向 (箭頭的意義)**：
    這正是微核心的運作命脈。應用程式與檔案系統**絕對不允許直接溝通**。應用程式必須將請求打包成訊息 (Messages)，往下發送給微核心的 IPC，再由 IPC 將訊息轉交給上方的檔案系統。

#### 3. 系統工程上的 Trade-off (權衡：優點與致命傷)
架構的改變必然帶來一好一壞的物理結果：

* **絕對優勢 (Benefits)**：
    * **高安全性與可靠性 (Security and Reliability)**：這是微核心最強大的武器。因為服務都在 User Mode 運行，如果今天網路卡的 Device driver 崩潰了，它就只是一個普通的程式當機。微核心依舊安然無恙，系統不會全盤崩潰。
    * **易於擴充與移植 (Extensibility & Portability)**：要新增功能，只需在 User Space 寫一支新程式即可，完全不用動到底層 Kernel 的原始碼。
    * *實務案例*：嵌入式系統的即時作業系統 **QNX**，以及 macOS/iOS 的核心組件 **Darwin**。
* **致命缺陷 (Disadvantages)**：
    * **效能負擔 (Performance Overhead)**：在單體式系統中，呼叫檔案系統只是一次簡單的 Function call。但在微核心中，如 Figure 2.15 的箭頭所示，這需要將訊息在不同的位址空間中複製，並引發多次 User Mode $\leftrightarrow$ Kernel Mode 的切換 (Context Switch)。這對 CPU 而言是極度昂貴的純耗損。
    * *實務案例*：微軟的 Windows NT 早期嘗試過微核心架構，但因為效能太差，後來的 Windows XP 等版本又偷偷把很多層次搬回 Kernel Space，回歸較偏向單體式的架構以挽救效能。

---


## 核心模組化架構理論剖析

在經歷了單體式系統的龐大臃腫，以及微核心架構的效能災難後，作業系統工程師最終找到了一個完美的平衡點：**2.8.4 Modules (模組化架構)**。

#### 1. 核心理念：動態連結 (Dynamic Linking)
在這種設計中，作業系統的 Kernel (核心) 被精簡到只剩下最根本的工作，例如 CPU 排程 (CPU scheduling) 與記憶體管理 (Memory management)。
至於其他的非核心服務（例如特定的檔案系統、網路卡驅動程式），全都被製作成獨立的 **Loadable kernel modules (LKMs / 可載入核心模組)**。

這些模組不會在系統建置時被寫死在核心程式碼中。相反地，它們可以在系統啟動時 (Boot time) 或系統正在運作時 (Run time)，被動態地「插入 (Inserted)」或「移除 (Removed)」。

#### 2. 隨插即用的物理實踐 (以 USB 為例)
以 Linux 系統插拔 USB 裝置為例：
當一塊全新的硬體插入電腦時，若 Linux 核心內部沒有該硬體的驅動程式，系統可以直接在硬碟中尋找對應的 LKM 並將其動態掛載進入核心。當 USB 拔除時，也可以在系統運作中動態移除該模組，釋放記憶體。
這意味著開發新硬體驅動時，**無須將機器關機，也無須重新編譯 (Recompiling) 整個作業系統**。

#### 3. 終極的架構比較 (The Best of Both Worlds)
課本特別將 LKM 與前面學過的架構進行了精準的對比：
* **對比層次式架構 (Layered system)**：雖然都有定義好的防護介面，但 LKM 更加靈活。在模組架構中，任何一個模組都可以直接呼叫其他模組，不受嚴格的上下層階級限制。
* **對比微核心 (Microkernel)**：這是 LKM 最強大的優勢。微核心將服務放在 User Space，導致極大的訊息傳遞 (Message passing) 效能負擔。而 LKM 是將模組**直接掛載進 Kernel Space (核心空間)**，因此模組間的溝通等同於核心內部的直接呼叫，完全不需要複製訊息或切換模式，執行效率極高。

**【重點摘要】**
* **Booting (開機啟動)**：硬體通電後，尋找、載入並啟動作業系統核心 (Kernel) 的標準物理程序。
* **韌體介面的演進**：從傳統多階段載入的 **BIOS**，全面進化至速度更快、支援度更廣的 **UEFI**。
* **Linux 啟動實務**：極度仰賴 **GRUB** (開機載入程式) 與 **initramfs** (暫時性記憶體檔案系統) 來載入壓縮的核心與初期驅動程式。
* **最終階段**：當實體檔案系統掛載完畢，並啟動全系統的第一支行程 (如 Linux 的 `systemd` 或 Android 的 `init`) 時，系統才正式進入 **Running (運行)** 狀態。

---

### 📝 步驟一：系統啟動理論與底層運作剖析 (Step 1: Theory & Architecture)

在探討完作業系統的設計與模組化架構後，2.9.2 節回歸到一個最根本的物理問題：「當電腦剛通電，主記憶體一片空白時，硬體怎麼知道 Kernel 在哪裡？」

這個將電腦從沉睡中喚醒的過程，就稱為 **Booting (開機啟動)**。

#### 1. 啟動的標準四步驟 (The 4-Step Boot Process)
多數系統的啟動都會嚴格遵循以下四個階段：
1. **Bootstrap program / Boot loader (開機載入程式)** 啟動，負責去硬碟尋找 Kernel 的位置。
2. 將 Kernel 實體載入主記憶體 (Memory) 中並開始執行。
3. Kernel 接管控制權，初始化所有的硬體（如 CPU 暫存器、裝置控制器）。
4. 掛載 **Root file system (根檔案系統)**，讓系統具備讀寫實體檔案的能力。

#### 2. 底層韌體的演進：BIOS vs. UEFI
尋找 Kernel 的任務，必須交給燒錄在主機板晶片內的程式來執行。
* **BIOS (Basic Input/Output System)**：
  早期的多階段啟動機制。硬體通電後，BIOS 會先執行。因為 BIOS 容量極小，它只能去硬碟最前端的一個固定位置（稱為 **Boot block / 開機區塊**）拉出「第二階段」的 Boot loader。再由這個稍微大一點的 Boot loader 將整個作業系統載入。
* **UEFI (Unified Extensible Firmware Interface)**：
  現代電腦的標準配備。它本身就是一個完整且龐大的 Boot manager，不需要像 BIOS 那樣進行多階段跳躍，因此開機速度大幅提升，且完美支援 64 位元架構與超大容量的硬碟。

#### 3. Linux 的開機魔法：GRUB 與 initramfs
Linux 的開機流程展示了高度的彈性與空間管理技巧：
* **GRUB**：這是 Linux 系統中最著名的 Boot loader。它具備強大的設定檔，允許在開機的瞬間修改核心參數，甚至選擇要啟動哪一個版本的 Kernel。
* **initramfs (暫時性檔案系統)**：
  為了節省硬碟空間與加快開機速度，Linux Kernel 通常會被壓縮。在開機過程中，Boot loader 會在主記憶體內憑空捏造出一個暫時的 RAM file system，稱為 `initramfs`。這裡面裝滿了解壓縮 Kernel 與讀取實體硬碟所需的「初期驅動程式」。
* **切換與運行**：
  當 Kernel 成功啟動並安裝好必要的驅動後，就會把 `initramfs` 丟棄，將視野切換到真正的實體硬碟上。最後，啟動系統的第一支行程 **`systemd`**（這是一切行程的祖先），隨後跳出登入畫面。

#### 4. 行動裝置與救援機制
* **Android 的微小差異**：雖然底層也是 Linux，但 Android 不使用 GRUB，而是使用廠商提供的 **LK (little kernel)**。此外，Android 不會丟棄 `initramfs`，而是直接將其保留作為設備的根檔案系統，最後啟動 `init` 行程。
* **Diagnostics and Recovery (診斷與復原)**：無論是哪種系統，Boot loader 都會提供進入 **Recovery mode (復原模式)** 或 **Single-user mode (單人模式)** 的選項，專門用於硬體診斷或修復損毀的檔案系統。

---
