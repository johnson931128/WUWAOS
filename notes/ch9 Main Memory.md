---
title: ch9 Main Memory

---

# 第九章前導：Virtual Memory (虛擬記憶體)
## 一、 虛擬與實體的脫鉤 (Virtual vs. Physical)
* **基本抽象化 (Basic Abstraction)：** 作業系統的記憶體管理提供了一個最基礎的幻覺——**Virtual Memory** (虛擬記憶體)。
* **位址不相等：** 一個行程 (Process) 在執行時所看到的記憶體位址 (Address space)，並不等於它實際存放在硬體 RAM 上的 Physical address (實體位址)。
* **動態翻譯：** 每當行程試圖存取某個記憶體位址時，系統必須在底層進行一次翻譯，將 Virtual Address (虛擬位址) 轉換成 Physical Address (實體位址)。

### 二、 Address Space (位址空間) 的內部佈局
![image](https://hackmd.io/_uploads/SJ0-dgTCbl.png)
講義展示了單一行程的標準記憶體佈局。從低位址 (Starting Address `0x00...`) 到高位址 (Ending Address `0xFFF...`)，空間被劃分為幾個重要區段 (Segments)：
1. **Text Segment (程式碼區段)：**
    存放編譯後的機器指令。硬體的 Program Counter (PC, 程式計數器) 會隨時指向此區段正在執行的指令。
2. **Data Segment (資料區段)：**
    負責存放全域變數 (Global variables) 與靜態變數。
3. **Heap (堆積區段)：**
    用於程式執行期間的動態記憶體配置 (例如 C 語言的 `malloc`)。此區段會由低位址向高位址成長。
4. **Stack (堆疊區段)：**
    存放區域變數、函式參數與呼叫狀態。硬體的 Stack Pointer (SP, 堆疊指標) 會指向此處，並**由高位址向低位址**反向成長。
    
### 三、 幕後功臣：MMU (記憶體管理單元)
![image](https://hackmd.io/_uploads/ryM5de6CWg.png)
第三張圖表揭示了系統運作的真相：雖然虛擬記憶體在邏輯上看起來是完美連續的一整塊，但經過 **MMU (Memory Management Unit, 記憶體管理單元)** 的翻譯後，這些區段實際上是被打散並塞入 Physical Memory (實體記憶體) 的各個零碎角落。
* 這種「間接存取 (Indirection)」完全由作業系統與硬體 MMU 聯手在底層完成，上層的應用程式毫不知情。

### 四、 導入虛擬機制的巨大效益
為何要大費周章做這層轉換？講義總結了三大效益：
1. **節省空間 (Saves space)：** 
    程式不需要把所有的程式碼與資料一次性全部載入實體記憶體中。系統只需將當下會用到的部分載入即可。
2. **極致的彈性 (Allows flexibility)：**
    透過間接定址，作業系統可以在執行期間 (Run-time) 根據系統狀況，自由在實體記憶體中搬移程式的位置。行程甚至可以定址並使用比實體機器實際安裝容量還要大的記憶體空間。
3. **隔離與保護 (Isolation and protection)：**
    在 MMU 的嚴格把關下，一個行程絕對無法越界存取到另一個行程的記憶體位址，確保系統不會因為單一程式崩潰而全面停擺。唯一的例外是透過 OS 刻意建立的 Shared memory (共享記憶體) 機制。
# 9.1.1 基本硬體 (Basic Hardware)
### 一、 存取速度與記憶體停頓 (Memory Stall)
* **物理限制：** 
    * CPU 在執行機器指令時，只能直接存取內建的暫存器 (Registers) 以及主記憶體 (Main memory)。如果資料存放在硬碟上，必須先將其搬移到記憶體中，CPU 才有辦法進行運算。
* **速度落差與停頓：**
    * **暫存器 (Registers)：** 速度極快，通常在一個 CPU 時脈週期 (Clock cycle) 內就能完成存取。
    * **主記憶體 (Main memory)：** 存取必須透過記憶體匯流排 (Memory bus)，往往需要耗費多個 CPU 時脈週期。當 CPU 找不到所需的資料而必須等待主記憶體傳輸時，就會發生**停頓 (Stall)**。
* **架構解法：**
    * 為了解決這種令人無法忍受的頻繁停頓，硬體設計師會在 CPU 晶片與主記憶體之間加入一層速度極快的緩衝區，也就是快取 (Cache)。快取的管理完全由硬體自動處理，不需要作業系統介入。
![image](https://hackmd.io/_uploads/rJab6ea0bg.png)

### 二、 為什麼需要「硬體級別」的保護？
在多重程式規劃的環境下，系統記憶體同時存放著作業系統本身，以及多個使用者的行程 (Processes)。
為了維持系統正確運作，必須確保：
1. 使用者行程不能存取或竄改作業系統的記憶體區段。
2. 使用者行程之間不能互相讀寫對方的記憶體。

**【效能考量】：** 這種防護機制必須**交由硬體來實作**。如果每次 CPU 存取記憶體，都要呼叫作業系統來檢查「這個位址合不合法」，會產生毀滅性的效能損失 (Performance penalty)。

### 三、 記憶體空間的護城河：Base 與 Limit 暫存器
為了在硬體層面實現光速般的檢查，系統為每個行程配置了兩個專屬的暫存器，用來定義該行程專屬的「邏輯位址空間 (Logical address space)」：
* **基底暫存器 (Base register)：** 記錄該行程合法的「最小實體記憶體位址」。
* **極限暫存器 (Limit register)：** 記錄該行程可存取的「範圍大小 (Size of the range)」。

**【數值範例 (對應 Figure 9.1)】**
![image](https://hackmd.io/_uploads/Byot6e6Cbe.png =50%x)
假設一個行程的 Base register 被設定為 `300040`，Limit register 被設定為 `120900`。
這代表該程式合法存取的實體位址範圍，是從 `300040` 一路延伸到 `420939` (即 `300040` + `120900` - `1`)。

### 四、 硬體位址保護邏輯 (Figure 9.2)
![image](https://hackmd.io/_uploads/rJ4AaeTAWx.png =50%x)
當 CPU 在使用者模式 (User mode) 下執行並產生一個記憶體位址時，硬體會自動且平行地執行以下兩道檢查關卡：
1. **下界檢查：** 檢查 `位址 >= Base`。如果小於基底值，代表程式試圖存取別人的記憶體（通常是作業系統或其他行程），硬體會立刻攔截。
2. **上界檢查：** 檢查 `位址 < (Base + Limit)`。如果大於等於這個總和，代表程式越界了，同樣遭到攔截。

只要任何一關檢查失敗，硬體就會產生一個 **陷入 (Trap)** 訊號通知作業系統，發生了非法的定址錯誤 (Illegal addressing error)。作業系統會將此視為致命錯誤 (Fatal error) 並強制終止該違規程式。

### 五、 權限的最高守門員：特權指令
既然 Base 和 Limit 暫存器決定了行程的生死邊界，那麼誰有資格修改它們？
* **核心模式專屬：** 
    * 這兩個暫存器的載入動作，被設計為特權指令 (Privileged instructions)。
* **防護意義：** 
    * 特權指令只能在作業系統的**核心模式 (Kernel mode)** 下執行。這徹底杜絕了使用者程式（惡意或意外）自己修改保護邊界來竊取資料的可能性。作業系統則擁有不受限制的權限，可以存取所有記憶體空間，以便進行行程載入、錯誤傾印 (Dump) 或系統呼叫參數的傳遞。

# Base 與 Limit 暫存器的供應鏈與管理機制深度解析
### 一、 Base Reg. 與 Limit Reg. 是「誰」提供的？
這個問題需要從「硬體實體」與「資料內容」兩個層面來看：
1. **硬體實體（載體）：**
    這兩個暫存器是由 **CPU 硬體** 提供的。它們是處理器核心內部的一種專用記憶體單元，旨在讓硬體能夠以「時脈週期」等級的速度進行位址檢查，而不必每次都去詢問軟體。
2. **資料內容（供應者）：**
    暫存器裡面的「**數值**」是由 **作業系統 (Operating System) 提供的**。作業系統在將程式載入記憶體時，會決定該行程要放在實體記憶體的哪個起點（Base），以及分配給它多大的空間（Limit）。
    
**【核心機制：特權指令】**
作業系統透過執行 **特權指令 (Privileged instruction)** 來將數值載入暫存器。由於這類指令只能在核心模式下運作，這保證了使用者程式（User program）絕對無法修改這兩個暫存器的內容，從而防止程式自行「擴張」邊界去竊取或破壞其他行程的資料。

### 二、 要在哪裡得知行程的實體位址與範圍大小？
行程本身是無法得知自己的「實體」位址的（它看到的都是從 0 開始的邏輯位址），而系統是透過以下兩個管道來得知與維護這些資訊：

1. **對於 CPU 硬體而言：**
    每當 CPU 產生一個位址請求時，硬體會直接從當下核心內的 **Base register** 讀取「最小實體記憶體位址」，並從 **Limit register** 讀取「範圍大小」。
2. **對於作業系統而言（動態切換）：**
    在多工系統中，作業系統會在內部的資料結構（通常是 PCB, Process Control Block）中記錄每個行程的位址區間。當進行 **Context Switch（內容交換）** 時，OS 會執行以下動作：
    * 將舊行程的 Base/Limit 數值儲存回記憶體。
    * 從記憶體取出新行程的 Base/Limit 資訊。
    * 利用特權指令將新資訊載入到 CPU 的硬體暫存器中。
# 9.1.2 位址綁定 (Address Binding)
### 一、 什麼是位址綁定？
程式通常以二進位執行檔 (Binary executable file) 的形式安靜地躺在磁碟 (Disk) 中。為了讓 CPU 執行，程式必須被搬移到主記憶體中，並置於一個行程的脈絡 (Context of a process) 內。

在這個從磁碟到記憶體的旅程中，程式內部的「位址」會經歷多次的變形與映射。

在程式開發與執行的不同階段，記憶體位址有著截然不同的表現形式：
* **原始碼階段：**
    * 位址是符號 (Symbolic)。例如宣告了一個變數 `count`，對開發者而言，`count` 就是存放資料的位址。
* **編譯階段：**
    * 編譯器 (Compiler) 會將這些符號綁定為**可重定位位址 (Relocatable addresses)**。例如，將 `count` 翻譯成「距離這個模組開頭算起第 14 個位元組 (14 bytes from the beginning of this module)」。
* **連結與載入階段：**
    * 連結器 (Linker) 或載入器 (Loader) 最終會將這些相對位置，綁定成實體記憶體中的**絕對位址 (Absolute addresses)**，例如真實的記憶體位置 `74014`。

### 二、 綁定的三大黃金時機
位址的轉換到底該在什麼時候進行？系統架構上有三種選擇：
1. **編譯時期 (Compile time)**
    **機制：**
        * 當編譯器在編譯時期 (Compile time) 進行了絕對綁定，它會把這包機器指令裡面「所有」提到位址的地方，通通填上寫死的實體記憶體數字（也就是填入 **Absolute address 絕對位址**）。一旦這包指令被塞滿了絕對位址，這「**整包指令**」就徹底失去了彈性，被死死地綁在記憶體的某個特定區塊。因為是整包產物都變得絕對且不可搬移，所以系統工程師將這整包產物統稱為 **Absolute code (絕對程式碼)**。
        * 如果在編譯原始碼的當下，就已經確切知道這個程式未來會被放在記憶體的哪個絕對位置（例如固定放在位址 `R`），編譯器就會直接生成**絕對程式碼** (Absolute code)。
    **致命傷：** 
        * 缺乏彈性。日後只要這個起始位址 `R` 發生改變，整份程式碼就必須重新編譯 (Recompile)。
2. **載入時期 (Load time)**
    **機制：**
        * 如果編譯器在編譯時保留了彈性，它會把指令裡面的位址，**通通寫成相對的偏移量**（也就是 **Relocatable address 可重定位位址**）。因為內部全都是相對距離，所以這「整包指令」獲得了一種超能力，它可以被作業系統整包端起來，隨便丟到實體記憶體的任何一個空位去執行。 因為這份整體產物具備了「**可被重新放置**」的物理特性，所以稱之為 **Relocatable code (可重定位程式碼)**。
        * 如果編譯時不知道最終的記憶體位置，編譯器就會退而求其次，生成可重定位程式碼 (Relocatable code)。真正的絕對位址綁定，會延遲到程式被載入記憶體的那一瞬間才進行。
    **優勢：**
        * 如果記憶體起始位址改變，不需要重新編譯，只需要把程式碼重新載入 (Reload) 一次，並套用新的起始值即可。
3. **執行時期 (Execution time / Run time)**
    **機制：**
        * 如果作業系統允許一個行程在執行過程中，可以隨時從記憶體的某個區段被搬移到另一個區段，那麼綁定動作就必須無條件延遲到執行時期才能進行。
    **硬體依賴：**
        * 這種極致的彈性無法單靠軟體完成，必須仰賴特殊的硬體支援（如後續小節會提到的 MMU 記憶體管理單元）。**這也是絕大多數現代作業系統所採用的標準作法**。
        
### 三、 程式處理的多步驟管線 (對應 Figure 9.3)
![image](https://hackmd.io/_uploads/rJl9vbaAZe.png)
課本透過流程圖 (Figure 9.3) 展示了一個使用者程式從無到有的完整生命週期：

* **編譯時期：** 原始程式碼 (Source program) 經過編譯器轉換為目的檔 (Object file)。
* **載入時期：** 連結器 (Linker) 將多個目的檔合併成單一執行檔 (Executable file)，接著由載入器 (Loader) 將其推入記憶體中。
* **執行時期：** 程式在記憶體中順利運行。此時若有動態連結函式庫 (Dynamically linked libraries)，也會在這個階段才被動態綁定進來。
# 9.1.3 邏輯位址與實體位址空間 (Logical Versus Physical Address Space)
## 一、 定義：兩種不同的位址視角
在電腦系統中，位址的定義取決於你是從「誰」的眼睛看出去：
1. **邏輯位址 (Logical address)：**
    * 由 CPU 在執行程式時所產生出來的位址。在執行時期綁定的系統中，它與**虛擬位址 (Virtual address)** 是同義詞，兩者可以交替使用。
2. **實體位址 (Physical address)：**
    * 由 記憶體單元 (Memory unit) 所看到的位址，也就是真正載入到記憶體位址暫存器 (Memory-address register) 中的硬體絕對位置。

**空間 (Space) 的概念：**
* 一個程式所能產生的所有邏輯位址集合，稱為邏輯位址空間 (Logical address space)；而這些邏輯位址對應到硬體上的所有位置集合，則稱為實體位址空間 (Physical address space)。

**綁定時機的影響：**
* 如果在「編譯時期」或「載入時期」就進行綁定，邏輯位址會直接等於實體位址。唯有在「執行時期綁定」的架構下，邏輯與實體位址才會完全不同。

### 二、 幕後功臣：MMU (記憶體管理單元)
既然 CPU 吐出的是邏輯位址，而記憶體只認得實體位址，中間勢必需要一個強大的翻譯官。這項「執行時期映射 (Run-time mapping)」的工作，是由一個稱為 **MMU (Memory-management unit)** 的硬體裝置來完成。

![image](https://hackmd.io/_uploads/H1_9W960-l.png)
**【動態重定位機制 (Figure 9.5)】**
課本用一個最基礎的 MMU 實作方案來舉例說明：
    1. **重定位暫存器 (Relocation register)：** 
        9.1.1 節提到的基底暫存器 (Base register)，在這裡被泛化稱為重定位暫存器。
    2. **硬體加法器：**
        當使用者程式在 CPU 產生一個邏輯位址（準備送往記憶體）時，MMU 的硬體會瞬間將「邏輯位址」加上「重定位暫存器」裡面的值，轉換成真正的實體位址。
    3. **數值推演：**
        假設重定位暫存器的值是 `14000`。當 CPU 試圖存取邏輯位址 `346` 時，MMU 會動態將其重定位 (Dynamically relocated)，最終實際存取的是實體位址 `14346`。
        
### 三、 程式的「楚門的世界 (The Grand Illusion)」
這一段是整個記憶體管理中最優美、也最核心的概念。

**使用者程式「永遠 (never)」不會接觸到真實的實體位址！**
在程式碼中，你可以建立一個指標 (Pointer) 指向位址 `346`，你可以把它存起來、對它做運算、拿它跟其他位址比大小，對程式而言，這個位址就確確實實是 `346`。

只有在指令真正要「讀取或寫入記憶體 (Load or store)」的最後一微秒，MMU 才會介入，相對基底暫存器把它轉換成 `14346`。

**【系統架構師的視角】**
這套機制完美切分了權責：
* **使用者程式：** 
    * 活在從 `0` 到 `max` 的邏輯空間中，以為自己擁有全世界。
* **作業系統與硬體：** 
    * 負責在底層將這塊 `0` 到 `max` 的邏輯空間，悄悄搬移並映射到實體記憶體的 `R + 0` 到 `R + max` 區間。

這種將邏輯位址與實體位址徹底分離 (Bound to a separate physical address space) 的設計，是作業系統能夠靈活、安全地管理記憶體的核心基礎。
# 講義的補充投影片
### 記憶體管理的需求 (Memory Management Requirements)
作業系統在設計記憶體管理機制時，必須滿足三個極度嚴苛的條件：
1. **保護 (Protection)：**
    必須嚴格限制每個行程 (Process) 能夠存取的實體位址 (Physical addresses)，確保行程之間不會互相踐踏或破壞 (can't stomp on each other)。
2. **快速轉譯 (Fast translation)：**
    無論系統採用多麼複雜的保護與映射機制，記憶體的存取速度都必須維持在極高的水準。
3. **快速內容交換 (Fast context switching)：**
    當 CPU 從一個行程切換到另一個行程時，更新硬體記憶體狀態的開銷 (Overhead) 必須極低。
    
**【核心結論】：** 純靠軟體（作業系統）絕對無法同時滿足這三個條件。有效的記憶體管理必須仰賴硬體支援 (Requires hardware support for efficient implementation)。

### MMU 與 TLB (MMU and TLB)
![image](https://hackmd.io/_uploads/ryXUQ5aCbe.png)
這張投影片揭示了硬體加速的兩大主角：
1. **記憶體管理單元 (Memory Management Unit, MMU)：**
    負責將虛擬位址 (Virtual address) 轉譯為實體位址 (Physical address) 的硬體單元。每一次的記憶體存取 (Each memory reference) 都必須強制通過 MMU 的檢驗與翻譯。
2. **轉譯後備緩衝區 (Translation Lookaside Buffer, TLB)：**
    * **定位：** 
        * 它本質上就是 MMU 轉譯表 (Translations table) 的專屬快取 (Cache)。
    * **重要性：**
        * 就「程式執行的正確性」而言，TLB 並不是必需的 (Not needed for correctness)；但就「效能」而言，它是帶來**顯著效能提升 (Significant performance gain)** 的絕對關鍵。
        * 補充說明：若沒有 TLB，CPU 每次存取資料都得先去記憶體查閱轉譯表，等於存取時間翻倍；有了 TLB，只要命中 (Cache hit)，瞬間就能得到實體位址。

### 記憶體配置的挑戰 (Memory Allocation)
![image](https://hackmd.io/_uploads/ByPTmcpCWl.png)
拋出一個大哉問：「我們該如何將記憶體配置給各個行程？」(How should we allocate memory to processes?)
#### 固定分割 (Fixed Partitions)
這是最早期也最簡單的配置策略。實體記憶體被像切蛋糕一樣，預先切成固定大小的分割區 (Fixed partitions)。
* **硬體需求與邏輯：**
    * 只需要一個基底暫存器 (`Base register`)。公式為：`Physical address = Virtual address (Offset) + Base register`。當切換行程時，OS 負責把該行程的 `Base` 載入硬體。
* **優點 (Advantages)：**
    * 實作極度簡單 (Easy to implement)，且內容交換速度非常快。
* **致命問題 (Problems)：**
    1. **單一尺寸的悲哀 (Partition size)：** 
        固定大小無法適應所有人 (one size does not fit all)。如果有一個極大的行程 (very large processes)，根本塞不進任何一個分割區。
    2. **內部碎裂 (Internal fragmentation)：**
        這是最嚴重的資源浪費。如果一個小行程被塞進一個大分割區，分割區內「沒用到的剩餘空間」將被徹底鎖死，其他行程絕對無法使用 (not available to other processes)。

#### 變動分割 (Variable Partitions)
![image](https://hackmd.io/_uploads/SJXB4c6RZe.png)
為了解決固定分割的呆板，系統演進為自然擴展 (Natural extension) 的變動分割。實**體記憶體被切分為大小可變 (Variable sized)** 的區塊，要多少給多少。
* **硬體需求與防護邏輯：**
    * 此時單靠 `Base` 已經不夠了，必須同時配備 `Base register` (基底) 與 `Limit register` (極限)。
* **保護機制 (Protection)：**
    * 在把 Offset 加上 Base 之前，硬體會先進行檢查。如果虛擬位址超越了 Limit 的範圍（或實體位址 > base + limit），系統會立刻拋出保護錯誤 (Protection fault / Exception fault)。
* **優點 (Advantages)：**
    * 徹底消滅了內部碎裂 (**No internal fragmentation**)。行程需要多少空間就給多少，一滴都不浪費。
* **致命問題 (Problems)：**
    * **外部碎裂 (External fragmentation)：**
        * 隨著行程不斷地載入與卸載 (Job loading and unloading)，實體記憶體中會產生許多散落各處的「空洞 (Empty holes scattered throughout memory)」。這些空洞的總容量或許很大，但因為不連續，導致無法塞入一個稍微大一點的新行程。

# 9.1.4 動態載入 (Dynamic Loading)
在早期的觀念中，一個行程必須將所有程式碼與資料完全塞入實體記憶體中才能執行，這導致行程的大小受到實體 RAM 容量的嚴格限制。為了解決這個瓶頸並提升記憶體空間使用率 (Memory-space utilization)，引入了動態載入 (Dynamic loading) 機制。
* **運作邏輯：**
    在動態載入的架構下，常式 (Routine) 會以可重定位 (Relocatable) 的格式安靜地躺在磁碟中。主程式會先被載入並執行。當某個常式需要呼叫另一個常式時，系統會先檢查目標常式是否已經在記憶體中了；如果沒有，才會呼叫可重定位連結載入器 (Relocatable linking loader) 將其載入，並更新位址表 (Address tables)。
* **最大優勢：**
    **「沒用到的東西，就永遠不會被載入」**。這點對於處理龐大但極少發生的事件（例如錯誤處理常式 Error routines）特別有效。整個程式的總容量或許高達數百 MB，但實際載入記憶體的部分可能只有一小部分，極大化了執行效率。
* **系統支援度：**
    有趣的是，**動態載入不需要作業系統的特別支援**(Does not require special support from the operating system)。妥善設計程式架構以利用此機制的責任，完全落在程式開發者身上；作業系統頂多只提供相關的函式庫 API 來協助實作。
    
# 9.1.5 動態連結與共享函式庫 (Dynamic Linking and Shared Libraries)
有別於載入，這節探討的是程式碼「連結」的時機。
### 一、 靜態與動態的對決
* **靜態連結 (Static linking)：**
    系統函式庫（例如 C 語言標準函式庫）會被視為一般的目的模組 (Object module)，由載入器直接打包、合併進最終的二進位執行檔映像 (Binary program image) 中。這會導致執行檔變得異常肥大，且如果系統上有 100 個程式，主記憶體中就會出現 100 份一模一樣的標準函式庫副本，極度浪費資源。
* **動態連結 (Dynamic linking)：**
    將連結的動作硬生生推遲到**執行時期 (Execution time)** 才進行。執行檔內只會保留微小的參照標記 (Stub)，當程式真正執行並呼叫到函式庫時，載入器才會去尋找並定位動態連結函式庫 (Dynamically linked libraries, DLLs)。
    
### 二、 共享函式庫 (Shared Libraries) 的威力
因為採用了動態連結，DLL 獲得了一個強大的特性：**共用**。
多個不同的行程可以共同存取主記憶體中「唯一」的一份 DLL 副本 (Only one instance of the DLL in main memory)。這就是為什麼在 Windows 與 Linux 系統中，它們又被稱為**共享函式庫** (Shared libraries)，此機制被廣泛且大量地應用在現代作業系統中。

### 三、 版本控管與更新 (Version Information)
* 動態連結允許函式庫進行獨立更新（如漏洞修補 Bug fixes），而不需要將所有依賴該函式庫的程式重新編譯或重新連結 (Relinked)。
* 為了防止程式意外執行到不相容的新版本，系統會在程式與函式庫中都加入**版本資訊** (Version information)。微小的改版維持相同版號，重大架構改變則會遞增版號，確保舊程式可以繼續安全地使用舊版函式庫，新程式則連結到新版。

### 四、 作業系統的絕對必要性
與 9.1.4 節的動態載入不同，動態連結與共享函式庫絕對需要**作業系統的介入與支援**。
在具備記憶體保護機制的系統中，只有作業系統這個最高權限實體 (Entity)，有資格去檢查某個常式是否已經存在於別的行程的記憶體空間內，並且只有 OS 能夠開放權限，允許多個獨立行程去存取同一個記憶體位址 (Access the same memory addresses)。
# 9.2 連續記憶體配置 (Contiguous Memory Allocation)
主記憶體 (Main memory) 是一個極度珍貴的資源，必須同時容納作業系統 (Operating system) 與各式各樣的使用者行程 (User processes)。因此，系統必須以最有效率的方式來配置記憶體。

早期採用的一種經典方法稱為 **連續記憶體配置 (Contiguous memory allocation)**。
* **空間劃分：**
    * 記憶體通常被切分為兩大區塊。一塊專門留給作業系統，另一塊則開放給使用者行程。
* **作業系統的位置：**
    * 作業系統可以被放置在低位址 (Low memory addresses) 或高位址 (High memory addresses)。這取決於硬體架構（例如中斷向量的位置）。不過，現代多數主流作業系統（包含 Linux 與 Windows）都選擇將作業系統放置在**高位址區**。
* **「連續」的定義：**
    * 在這種配置機制下，每個進入記憶體的行程，都必須被裝在一個「單一且連續 (Single section of memory that is contiguous)」的區塊中，緊鄰著下一個行程。

# 9.2.1 記憶體保護 (Memory Protection)
在探討如何把行程塞進記憶體之前，系統必須先建構好堅固的護城河，也就是 記憶體保護 (Memory protection)。為了防止某個行程越界去存取不屬於它的記憶體，系統巧妙地結合了前面章節提過的兩個硬體概念：
1. **重定位暫存器 (Relocation register)：** 存放該行程在**實體記憶體中**的「最小起始位址」。
2. **極限暫存器 (Limit register)：** 存放該行程擁有的「邏輯(virtual)位址範圍 (Range of logical addresses)」。

**【運作邏輯與公式推導】**
硬體 MMU (Memory-Management Unit) 內建了一套防護與加法機制（對應 Figure 9.6）：
![image](https://hackmd.io/_uploads/HyiTYjpRbx.png)
* **步驟一：範圍檢查**
    * 當 CPU 產生一個邏輯位址 (Logical address) 時，硬體會先檢查 `邏輯位址 < 極限暫存器 (Limit)`。若條件不成立，代表程式越界，直接觸發定址錯誤陷阱 (Trap: addressing error)。
* **步驟二：實體映射**
    * 若範圍合法，MMU 接著會執行公式：
    > $$實體位址 (Physical Address) = 邏輯位址 (Logical Address) + 重定位暫存器 (Relocation Register)$$
    * 透過硬體加法器，瞬間推導出真正的記憶體位置，並將其送往記憶體單元。
* **課本數值範例：**
    假設某行程的 Relocation = `100040`，Limit = `74600`。
    這意味著該行程產生的邏輯位址必須嚴格小於 `74600`。當它嘗試存取邏輯位址 `0` 時，會被映射到實體的 `100040`；嘗試存取最大的合法邏輯位址 `74599` 時，會被映射到實體的 `174639`。
    
**【內容交換與 OS 彈性補充】**
* **Context Switch (內容交換)：**
    * 當 CPU 排程器 (CPU scheduler) 決定切換執行另一個行程時，分派器 (Dispatcher) 會負責將新行程專屬的 Relocation 與 Limit 數值載入硬體暫存器中。有了這層硬體把關，作業系統的程式碼與其他使用者的資料就能免於被當前運行的行程竄改。
* **動態改變作業系統大小：**
    * 這套機制帶來了一個極佳的副作用——彈性 (Flexibility)。作業系統核心包含了許多裝置驅動程式 (Device drivers) 的程式碼。如果某個驅動程式當下沒被使用（例如沒有人接上印表機），系統根本不需要把它一直留在記憶體裡佔空間。系統可以將其卸載，釋放空間給別人；直到真正需要時，再將其動態載入。因為所有的使用者行程都是透過重定位暫存器「相對」定址的，作業系統空間的收縮或擴張，完全不會干擾到使用者行程的執行位址。
# 9.2.2 記憶體配置 (Memory Allocation)
### 核心名詞雙語解析
* **Variable-partition scheme (變動分割機制)**
    * Definition: A method of allocating memory where processes are assigned to partitions of varying sizes based on their exact requirements.
    * 解釋：有別於將記憶體切成固定大小，此機制允許記憶體區塊的大小根據行程的實際需求「動態改變」。每個分割區內恰好只包含一個行程。
* **Hole (可用空間 / 記憶體空洞)**
    * Definition: One large block of available memory, or any contiguous block of unallocated memory space.
    * 解釋：在系統初始狀態下，所有供使用者使用的記憶體被視為一個巨大的 Hole。隨著程式執行，Hole 會被切割；當程式離開，相鄰的 Holes 會被合併成更大的 Hole。
* **Dynamic storage-allocation problem (動態儲存體配置問題)**
    * Definition: The computational problem of deciding how to satisfy a memory request of size `n` from a list of free holes.
    * **解釋：** 這是一個經典的演算法難題，探討在面對一堆零碎的可用空間時，作業系統該採取何種策略來滿足新的記憶體申請，以達到最佳的空間利用率與效能。

### 一、 記憶體狀態的動態演變 (Figure 9.7 解析)
![image](https://hackmd.io/_uploads/SkLVnoTC-l.png)
將記憶體配置給行程的最簡單方法之一，就是採用變動分割機制 (Variable-partition scheme)。作業系統會在底層維護一張表格，精準記錄哪些記憶體部分是可用的 (available)，哪些是已被佔用的 (occupied)。
1. 一開始，記憶體被完全利用 (fully utilized)，依序裝載了 process 5, 8, 2。
2. 當 process 8 執行完畢離開後，中間空出了一塊連續的可用空間（產生了一個 Hole）。
3. 接著 process 9 抵達，系統從剛才的 Hole 中切出一塊分配給它。
4. 最後 process 5 也離開了。此時，記憶體中出現了兩個不連續的可用空洞 (**two noncontiguous holes**)。

當一個需要記憶體的新行程抵達時，作業系統會在散落各處的 Holes 集合中進行搜尋 (searches the set for a hole)。
* 如果找到的 Hole 太大，系統會將其**一分為二 (split into two parts)**：
    * 一部分給新行程，另一部分放回可用 Holes 集合中。
* 當行程結束釋放記憶體時，若這個新釋放的空間與其他 Holes 相鄰，系統會將它們**合併 (merged)** 成一個更大的 Hole。
* 若總體記憶體不足以滿足需求，系統可以選擇拒絕該行程，或者將其放入等待佇列 (Wait queue) 中排隊。

### 二、 三大配置策略對決
面對動態儲存體配置問題，業界最常使用的三大策略如下：
1. **First fit (首次適應)**
    * **策略：** 配置第一個空間足夠大的 Hole (Allocate the first hole that is big enough)。
    * **作法：** 從頭開始找，一旦遇到夠大的空間就立刻停止搜尋並配置。
    * **特性：** 因為不需要掃描全部空間，通常**執行速度最快 (generally faster)**。
2. **Best fit (最佳適應)**
    * **策略：** 配置所有空間足夠大的 Holes 中「最小」的那一個 (Allocate the smallest hole that is big enough)。
    * **作法：** 除非清單已經依大小排序，否則必須搜尋整份清單 (must search the entire list)。
    * **特性：** 此策略會產生「最小的剩餘空洞 (smallest leftover hole)」，試圖將空間浪費降到最低。
3. **Worst fit (最差適應)**
    * **策略：** 配置「最大」的 Hole (Allocate the largest hole)。
    * **作法：** 同樣必須搜尋整份清單。
    * **特性：** 反其道而行，它會產生「最大的剩餘空洞 (largest leftover hole)」。這種作法的邏輯是：留下來的超大空洞，或許會比 Best fit 留下的小碎塊更有機會被下一個行程利用。

**【效能總結】**
經過大量模擬測試顯示，在降低時間與儲存空間利用率方面，First fit 與 Best fit 的表現皆優於 Worst fit。雖然 First fit 與 Best fit 在空間利用率上難分高下，但 First fit 的搜尋速度通常更快。
# Buddy System and Next fit
### Bilingual Definitions
為了精確掌握補充講義的技術內涵，針對關鍵專有名詞進行雙語對照與原理解釋：
* **Buddy System (夥伴系統)**
    * Definition: A memory allocation algorithm that divides memory into partitions to try to satisfy a memory request as suitably as possible, with all free blocks being sizes of an integral power of two.
    * 解釋：所有空閒區塊 (Free blocks) 的大小必須是 2 的整數次方 (Integral power of 2)。若無剛好的大小，會將較大的區塊對半切，產生兩個互為「夥伴 (Buddies)」的子區塊。
* **Roving Pointer (漫遊指標)**
    * Definition: A pointer used in the Next Fit algorithm that remembers the position of the last allocated memory block, beginning the next search from there instead of restarting at the beginning.
    * 解釋：Next Fit 的靈魂 。每次找到合適空間後，指標會停留在該處；下一個請求到來時，就從這個指標位置繼續往下找，以環狀方式 (Circular fashion) 遍歷整條記憶體鏈 (Memory chain)。

### 講義補充一：夥伴系統 (Buddy's System) 實戰推演
依據講義提供的範例，進行記憶體切割與配置的推演：
* **初始狀態：** 擁有 512KB 的完整連續記憶體。
* **事件 1：P1 請求 70KB 空間**
    * 因為必須是 2 的次方，70KB 向上進位 (Rounding up) 至 128KB。
    * 系統中初始沒有 128KB 甚至 256KB 的空洞，因此將 512KB 切割為兩個 256KB 的夥伴。
    * 將其中一個 256KB 繼續切割為兩個 128KB 的夥伴。
    * **結果：** 分配一個 128KB 給 P1。系統剩餘一個 128KB 與一個 256KB 空閒區塊。
* **事件 2：P2 請求 35KB 空間**
    * 35KB 向上進位至 64KB。
    * 將前一步剩下的 128KB 再次切割為兩個 64KB 的夥伴。
    * **結果：** 分配一個 64KB 給 P2。剩餘一個 64KB 與一個 256KB 空閒區塊。
* **系統點評：**
    * **優點 (Advantage)：**
        * 釋放速度極快。當一個大小為 $2^k$ 的區塊被釋放時，系統只需檢查它專屬的另一半夥伴是否空閒，若空閒則立即合併，不需像其他演算法掃描整份空洞清單。
    * **缺點 (Disadvantage)：**
        * 記憶體利用率低落。如上述 P2 僅需 35KB，卻被配給 64KB，白白浪費了 29KB 的空間，這是**標準的內部碎裂 (Internal fragmentation)**。此外，若夥伴之間存在已分配的區塊，也可能導致外部碎裂。

### 講義補充二：下次適應演算法 (Next Fit) 實戰推演
Next Fit 旨在解決 First Fit 容易將記憶體前端切成無數小碎塊，導致後續搜尋效率變慢的缺點。
**【演算法執行推演 (Algorithm Steps)】**
* 目前實體記憶體中有三個空閒區塊 (Block sizes)：`[Block 0: 5, Block 1: 10, Block 2: 20]`
* 即將有三個行程 (Process sizes) 依序進入：`[P1: 10, P2: 20, P3: 5]`
* 漫遊指標 (Roving Pointer) 初始位置停留在開頭。

1. **處理 P1 (大小 10)**
    * 從指標位置 Block 0 開始檢查。`Block 0 (5) < 10`，不合適。
    * 往下檢查 Block 1。`Block 1 (10) >= 10`，配置成功。
    * 狀態更新：Block 1 被 P1 佔用。**漫遊指標移動並停留在 Block 1**。
2. **處理 P2 (大小 20)**
    * 從指標位置 Block 1 的「下一個」開始找，也就是 Block 2 。
    * `Block 2 (20) >= 20`，**配置成功**。
    * 狀態更新：Block 2 被 P2 佔用。**漫遊指標停留在 Block 2**。
3. **處理 P3 (大小 5)**
    * 從指標位置 Block 2 的下一個開始找。因為已經到底，觸發環狀機制 (Circular fashion)，**繞回頭從 Block 0 開始** 。
    * `Block 0 (5) >= 5`，配置成功。
    * 狀態更新：Block 0 被 P3 佔用。指標停留在 Block 0。

**系統點評：**
* **優點：** 
    * 平均搜尋時間變短，速度非常快 (Comparatively faster than First Fit)。因為避開了前面可能已經被切得稀碎的區塊，使得記憶體的負載分布更加均勻 (Better load balancing) 。
* **缺點：**
    * 可能會提早把記憶體後段的大型連續空間切碎 (Cut large portion of free parts into small pieces)。若後續來了真正需要超大空間的行程，**將面臨無連續空間可用的外部碎裂問題**。 
# 9.2.3 碎裂問題 (Fragmentation)
### Bilingual Definitions
* **External Fragmentation (外部碎裂)**
    * Definition: Exists when there is enough total memory space to satisfy a request, but the available spaces are not contiguous; storage is fragmented into a large number of small holes.
    * 解釋：無論採用 First-fit 或 Best-fit 演算法，都會面臨此問題。最糟的情況下，可能每兩個行程之間都夾著一個無用的小空洞。
* **Internal Fragmentation (內部碎裂)**
    * Definition: Unused memory that is internal to a partition, resulting from the memory allocated to a process being slightly larger than the requested memory.
    * 解釋：為了避免管理極小空洞（例如僅有 2 bytes 的洞）所耗費的龐大系統開銷，作業系統通常會以「固定大小的區塊 (fixed-sized blocks)」為單位來分配記憶體。當申請的空間不完全等於區塊的倍數時，多給的空間就會浪費在分割區內部。
* **Compaction (緊緻化 / 壓縮)**
    * Definition: A solution to external fragmentation that shuffles the memory contents so as to place all free memory together in one large block.
    * 解釋：將記憶體內的所有行程推向同一端，讓所有散落的空洞聚集在另一端合併成大空洞。

### 一、 外部碎裂的困境與 50% 規則
當行程不斷地被載入 (loaded) 與移除 (removed) 時，原本完整的可用記憶體會逐漸被切碎 (broken into little pieces)。
根據嚴謹的統計分析（50% 規則），外部碎裂絕非微不足道的小問題，它可能會讓系統憑空損失三分之一的記憶體效能。採用何種配置演算法（First-fit 或 Best-fit），或是決定從空洞的哪一端開始配置（頂部或底部），都只能「影響 (affect)」碎裂的程度，無法完全根除。

### 二、 內部碎裂的權衡
相對於外部的散落空洞，內部碎裂發生在分割區裡面。
課本舉了一個極端的例子：假設系統目前有一個 18,464 bytes 的空洞，而下一個行程剛好只要求 18,462 bytes。如果作業系統精準配置，就會留下一個區區 2 bytes 的極小空洞。
* **系統開銷考量：** 在底層維護一個節點來追蹤這 2 bytes 的空洞，其所耗費的記憶體與運算成本，甚至比這 2 bytes 本身還要大 (The overhead to keep track of this hole will be substantially larger than the hole itself)！
* **工程妥協：** 因此，作業系統寧願「多給一點」，把整個 18,464 bytes 直接分配給該行程。行程用不到的那 2 bytes，就成為了無法被回收的內部碎裂。

### 三、 緊緻化 (Compaction) 的條件與代價
為了解決外部碎裂，最直觀的解法就是**緊緻化 (Compaction)**，也就是對記憶體進行大搬風。但這項技術有著嚴格的前提與高昂的代價：
1. **先決條件 (Hardware Requirement)：** 並非所有系統都能執行緊緻化。如果程式的位址綁定是發生在編譯或載入時期 (static and is done at assembly or load time)，記憶體位址已被寫死，就絕對不能搬移。緊緻化**僅能在採用「執行時期動態重定位 (dynamic and is done at execution time)」** 的系統上運作（即依賴上一節提到的 Base/Relocation register）。
2. **效能成本 (Expense)：** 即使硬體支援，搬移龐大的程式與資料，並逐一修改基底暫存器 (base register) 的數值，會耗費極其龐大的系統資源 (This scheme can be expensive)。
3. I/O problem
    * Latch job in memory while it is involved in I/O
    * Do I/O only into OS buffers
    * 做 I/O 的 Memory Block 不能動

# 講義的 Segmentation
### Bilingual Definitions
* **Segmentation (分段機制)**
    * Definition: A memory-management scheme that supports the user view of memory, where a program is treated as a collection of segments of varying lengths.
    * 解釋：將程式切割成多個具有邏輯意義、且長度不一的區段（例如 Stack、Main program）。這讓程式的各個部分可以被打散放到實體記憶體中。
* **Segment Table (分段表)**
    * Definition: A table that maps two-dimensional logical addresses to physical addresses; each entry has a base (starting physical address) and a limit (length of the segment).
    * 解釋：硬體 MMU 進行位址轉換的關鍵查表清單。講義手寫筆記特別補充：這張表其實存放在實體 Memory 中，實務上為了加速，會搭配 TLB (Translation Lookaside Buffer) 當作快取來節省記憶體操作次數。
* **STBR & STLR**
    * Definition: **Segment-table base register (STBR)** points to the table's location in memory. **Segment-table length register (STLR)** indicates the number of segments used by a program.
    * 解釋：STBR 告訴硬體分段表在哪裡；STLR 則定義了該程式有幾個分段（作為第一道安全防線，分段編號 $s$ 必須小於 STLR）。

### 一、 為什麼需要分段？ (User's View of a Program)
![image](https://hackmd.io/_uploads/HyDnthaRWe.png)
寫程式時，你絕對不會把程式想成「一個從位址 0 到 100000 的連續位元組陣列」。你會把它想成一堆獨立的模組：這裡有主程式 (main program)、那邊有副程式 (subroutine)、還有全域變數 (global variables)、陣列 (arrays) 以及不斷伸縮的堆疊 (stack)。
分段機制的偉大之處，就在於它支援了這種**使用者視角 (Supports user view of memory)**。
### 二、 二維的記憶體尋址 (Logical View & Architecture)
因為每個分段都是獨立的，所以 CPU 發出的邏輯位址 (Logical address) 變成了一個二元組 (Two tuple): `<segment-number, offset>`。
* **Segment-number ($s$)：** 講義手寫註記「放哪」。代表你要找的是哪一個分段（例如：我要找 Stack 段）。
* **Offset ($d$)：** 講義手寫註記「離基準多少」。代表你要存取該分段內的第幾個 Byte。
* （講義補充範例：在早期 x86 架構中，常常看到的 CS:IP，就是 Code Segment (程式碼段編號) : Instruction Pointer (偏移量) 的具體實踐）。

### 三、 硬體轉譯管線 (Segmentation Hardware)
![image](https://hackmd.io/_uploads/H1W5cna0-x.png)
當 CPU 發出 `<s, d>` 時，底層硬體（類似 MMU 的結構）會執行極度嚴格的兩道防護與轉譯：
1. **查表與合法性驗證：** 拿著編號 $s$ 去查 **Segment table** (分段表)。
2. **越界檢查：** 取出該分段的長度限制 (`limit`)。硬體會檢查你的偏移量 $d$ 是否小於 `limit`。如果不是 ($d \ge limit$)，直接觸發定址錯誤 (Trap: addressing error)，防止你讀取到別人的資料。
3. **實體映射：** 如果合法，硬體將該分段的起始實體位址 (`base`) 加上偏移量 $d$ ($physical\_address = base + offset$)，算出最終的實體記憶體位址。

### 四、 保護與碎裂的副作用 (Protection & Allocation Problems)
* **細緻的保護與共享 (Protection & Code Sharing)：**
    因為程式被切成了有意義的邏輯區塊，保護機制變得非常直覺。在分段表中，每個分段除了 base 和 limit，還可以附加驗證位元 (Validation bit) 以及 讀/寫/執行權限 (read/write/execute privileges)。例如，你可以把整個「Code Segment」標記為「唯讀 (Read-only)」並讓多個行程共享 (Code sharing occurs at segment level)。
* **動態儲存體配置問題 (Dynamic storage-allocation problem)：**
    這是分段機制最大的痛點。每個分段的長度都不一樣 (Segments vary in length)，當它們被載入實體記憶體時，本質上就是在玩上一節提到的「變動分割 (Variable Partitions)」遊戲。因此，**分段機制同樣會遭遇嚴重的外部碎裂 (External fragmentation) 問題**。
    
# 經典 Segmentation (分段機制) 實戰應用題

這種題型在作業系統 (Operating Systems) 考試中極為常見，主要測驗對底層硬體管線的運算熟悉度！

**【題目設定】**
假設某個系統採用分段機制 (Segmentation)，且系統的**分段表長度暫存器 (Segment-Table Length Register, STLR)** 的值為 **5**（這代表合法的分段編號為 0 到 4）。
目前記憶體中的**分段表 (Segment Table)** 內容如下：

| Segment Number (分段編號 $s$) | Limit (長度限制) | Base (起始實體位址) |
| --- | --- | --- |
| **0** | 600 | 219 |
| **1** | 14 | 2300 |
| **2** | 100 | 90 |
| **3** | 580 | 1327 |
| **4** | 96 | 1952 |

請計算以下五個由 CPU 發出的**邏輯位址 (Logical Address)** 二元組 `<s, d>`。
若存取合法，請推導出對應的**實體位址 (Physical Address)**；若不合法，請指明觸發了何種錯誤 (Trap)。

1. `<0, 430>`
2. `<1, 14>`
3. `<2, 50>`
4. `<3, 600>`
5. `<5, 12>`

---

### 一步一步：推導與解題過程

**【步驟一：確認核心公式與判定規則】**
在進行計算前，先釐清硬體轉譯的數學邏輯與公式推導：

* **規則 1 (段號檢查)**：$s < STLR$
* *推導過程*：STLR 記錄了該程式擁有的分段總數量。如果請求的段號 $s$ 超出或等於這個總數，代表程式試圖存取根本不存在、或不屬於自己的分段記憶體，必須立刻攔截。


* **規則 2 (偏移量檢查)**：$d < Limit$
* *推導過程*：Limit 記錄了該分段的長度大小。在從 0 開始索引的記憶體系統中，最大合法的位移量只能到達 $Limit - 1$。如果 $d \ge Limit$，代表存取越界了。


* **公式 (實體位址計算)**：$Physical Address = Base + d$
* *推導過程*：Base 是該分段被載入實體記憶體中的「絕對起點」。由於分段「內部」是一塊連續的空間，因此只要把起點加上程式要求的偏移距離 ($d$)，就能精準對應到真實的硬體記憶體位置。



**【步驟二：逐題破解】**

**第 1 題：`<0, 430>**`

* **段號檢查**：$s = 0$。$0 < 5$ (STLR)，通過第一關。
* **查表**：取得 Segment 0 的 Limit = 600，Base = 219。
* **偏移量檢查**：$d = 430$。$430 < 600$，通過第二關。
* **計算**：$219 (Base) + 430 (d) =$ **`649`**。
* **最終答案**：合法存取，實體位址為 **649**。

**第 2 題：`<1, 14>**`

* **段號檢查**：$s = 1$。$1 < 5$ (STLR)，通過第一關。
* **查表**：取得 Segment 1 的 Limit = 14，Base = 2300。
* **偏移量檢查**：$d = 14$。$14 < 14$，**條件不成立！**
* **最終答案**：不合法！觸發 **定址越界錯誤 (Trap: Addressing error)**。偏移量必須嚴格小於 Limit，最大只能存取到偏移量 13。

**第 3 題：`<2, 50>**`

* **段號檢查**：$s = 2$。$2 < 5$ (STLR)，通過第一關。
* **查表**：取得 Segment 2 的 Limit = 100，Base = 90。
* **偏移量檢查**：$d = 50$。$50 < 100$，通過第二關。
* **計算**：$90 (Base) + 50 (d) =$ **`140`**。
* **最終答案**：合法存取，實體位址為 **140**。

**第 4 題：`<3, 600>**`

* **段號檢查**：$s = 3$。$3 < 5$ (STLR)，通過第一關。
* **查表**：取得 Segment 3 的 Limit = 580，Base = 1327。
* **偏移量檢查**：$d = 600$。$600 < 580$，**條件不成立！**
* **最終答案**：不合法！觸發 **定址越界錯誤 (Trap: Addressing error)**。

**第 5 題：`<5, 12>**`

* **段號檢查**：$s = 5$。$5 < 5$ (STLR)，**條件不成立！**
* **最終答案**：不合法！觸發 **非法分段錯誤 (Trap: Illegal segment)**。合法的最大段號只能到 4，連查表的機會都沒有就被硬體直接攔截了。
# 9.3 分頁機制與 9.3.1 基本方法 (Part 1) 深度解析
為了解決連續記憶體配置所帶來的外部碎裂與緊緻化難題，現代作業系統（從大型伺服器到行動裝置）幾乎全面採用了**分頁 (Paging)** 機制。這項技術允許一個行程的實體位址空間是**非連續的** (noncontiguous)。

### 一、 兩大核心名詞 (Frames vs. Pages)
分頁的基本實作方法，是將記憶體進行標準化、固定大小的切割：
1. **Frames (實體頁框)：** 實體記憶體 (Physical memory) 被劃分為固定大小的區塊。
2. **Pages (邏輯分頁)：** 邏輯記憶體 (Logical memory) 也被劃分為與 Frame 完全相同大小的區塊。

當行程準備執行時，作業系統會將該行程的多個 Pages，任意塞進實體記憶體中目前閒置的 Frames 裡面。由於兩者大小一致，剛好可以完美嵌合。這種設計讓邏輯位址空間與實體位址空間徹底脫鉤。

### 二、 硬體轉譯管線 (Paging Hardware)
![image](https://hackmd.io/_uploads/By9HSpT0-x.png)
在分頁架構下，CPU 產生的每一個邏輯位址都會被硬體自動拆分為兩個部分：
* **Page number ($p$, 分頁編號)：** 作為索引值 (index)，用來查詢該行程專屬的 Page table (分頁表)。
* **Page offset ($d$, 分頁位移)：** 表示資料在該分頁/頁框內部的相對位置。

**【轉譯三步驟 (對應 Figure 9.8)】**
![image](https://hackmd.io/_uploads/rkBc46pRZe.png)
1. MMU 從邏輯位址中萃取出分頁編號 $p$，並去查分頁表。
2. 從分頁表中找出對應的實體頁框編號 $f$ (frame number)。
3. 將原本的 $p$ 替換成 $f$，而位移量 $d$ 保持不變 ($d$ does not change)。將 $f$ 與 $d$ 組合起來，即為最終的實體記憶體位址。

### 三、 數學模型與位址結構 (Mathematical Model)
為了讓硬體能以極快的速度完成「拆解 $p$ 與 $d$」的動作，分頁大小 (Page size) 永遠會被設計成 2 的次方 (a power of 2)。通常介於 4 KB 到 1 GB 之間。

**【公式推導】**
若邏輯位址空間的大小為 $2^m$，且分頁大小為 $2^n$ 位元組 (bytes)：
* 邏輯位址的總長度為 $m$ 個位元 (bits)。
* 低位元的 $n$ bits 會直接被當作分頁位移 $d$。
* 高位元的 $m-n$ bits 則用來表示分頁編號 $p$。透過這種 2 的次方的設計，硬體不需要做任何除法或餘數運算，只要進行簡單的位元遮罩 (Bit masking) 就能瞬間分離出 $p$ 與 $d$。

### 四、 碎裂問題的轉移 (The Fragmentation Trade-off)
採用分頁機制後，最大的勝利是再也沒有外部碎裂 (no external fragmentation)。任何閒置的 Frame 都能被分配給需要的行程，因為不再需要連續的巨大空間。
然而，代價是產生了內部碎裂 (internal fragmentation)。
因為記憶體是「以 Frame 為單位 (allocated as units)」整塊配發的。如果行程所需的空間無法剛好被分頁大小整除，最後一個配發的 Frame 就裝不滿。
* **極端案例：** 最差的情況下 (worst case)，某個行程剛好需要 $x$ 個分頁再多出 1 byte。系統還是必須完整配發 $x+1$ 個 Frames 給它，導致最後一個 Frame 幾乎整塊浪費掉。
* **數值範例：** 若分頁大小為 2,048 bytes，行程大小為 72,766 bytes。計算可得需要 35 個分頁外加 1,086 bytes。系統會配發 36 個 Frames，導致產生 $2,048 - 1,086 = 962$ bytes 的內部碎裂。
# 9.3.1 基本方法 (Basic Method) 深度解析 - 第二部分
### 一、 分頁大小的工程權衡 (The Page Size Trade-off)
分頁大小 (Page size) 是由硬體決定的，但這牽涉到一個複雜的工程妥協：
* **小分頁的優勢：** 如果分頁很小，根據統計，每個行程平均只會浪費「半個分頁」的內部碎裂 (internal fragmentation)。
* **小分頁的致命傷：** 分頁越小，代表一個程式需要被切成更多塊，這會導致**分頁表 (Page table) 變得極度龐大**，每個表格條目 (page-table entry) 都會消耗額外的記憶體開銷 (overhead)。
* **大分頁的優勢：** 隨著記憶體容量與資料集不斷變大，大分頁不僅能縮小分頁表，還能提升磁碟 I/O 傳輸的效率。
* **業界現況：** 現代系統的預設分頁大小通常為 4 KB 或 8 KB。某些架構甚至支援多種分頁大小，例如 x86-64 系統上的 Linux 支援預設的 4 KB 以及更大的 Huge pages (巨型分頁)。

**【實體定址空間的擴張】**
課本舉例，在一個 32 位元的 CPU 上，每個分頁表條目通常佔 4 bytes。這個 32 位元的條目可以指向 $2^{32}$ 個不同的實體頁框。如果每個頁框是 4 KB ($2^{12}$ bytes)，那麼這個系統的實體記憶體定址能力將高達 $2^{44}$ bytes（也就是 16 TB）。這證明了在分頁系統中，實**體記憶體的大小，可以完全不同於程式邏輯位址的最大限制**。

### 二、 記憶體配置與視角隔離 (Allocation & Separation of Views)
![image](https://hackmd.io/_uploads/BJU6Ia6R-g.png =50%x)
當一個行程準備執行時，系統會計算它需要幾個分頁 (pages)。
* **分配過程 (對應 Figure 9.11)：** 作業系統會去檢查 空閒頁框清單 (free-frame list)。如果行程需要 $n$ 個分頁，清單中就必須至少有 $n$ 個空閒頁框。系統會將程式的第 0 頁載入第一個分配到的頁框，將頁框編號填入分頁表，接著載入第 1 頁，依此類推。
* **楚門的世界：** 這是分頁機制最重要的一環——清楚分離了程式設計師的視角與真實的實體記憶體 (clear separation between the programmer's view of memory and the actual physical memory)。程式碼以為自己獨佔了一塊連續的巨大空間，但實際上，它被無情地打散在實體記憶體的各個角落，中間甚至夾雜著其他程式的資料。
* **硬體級保護：** 這個對應關係被作業系統嚴格掌控，且對程式設計師完全隱藏 (hidden from the programmer)。由於程式只能透過自己的分頁表來定址，它絕對無法存取不屬於它的記憶體空間。

### 三、 作業系統的幕後管理 (OS Management)
由於硬體只負責「轉譯」，剩下的管理重擔全落在作業系統肩上。
* **頁框表 (Frame Table)：** 為了解決「哪些頁框是空的、哪些被誰佔用」的問題，作業系統維持著一張全域的資料結構，稱為 **頁框表 (frame table)**。實體記憶體中的每一個頁框，在這裡都有一個對應的狀態記錄。
* **分頁表副本與內容交換 (Context Switch Overhead)：** 除了維護全域頁框表，作業系統還必須為每一個行程保留一份分頁表的副本 (a copy of the page table for each process)。
    * **用途 1：** 當程式呼叫系統呼叫 (System call) 進行 I/O 操作並傳遞緩衝區位址時，OS 必須手動利用這份副本，將邏輯位址轉譯為實體位址，才能正確寫入硬體。
    * **用途 2：** 當 CPU 分派器 (Dispatcher) 切換行程時，必須將這份副本載入到硬體的分頁暫存器中。這也意味著，分頁機制無可避免地增加了內容交換的時間成本 (increases the context-switch time)。

# 9.3.2 硬體支援 (Hardware Support) 深度解析

分頁表的管理與存放位置，是作業系統與硬體架構師必須面對的一大挑戰。因為每個行程都有自己專屬的分頁表 (per-process data structures)，在進行內容交換 (Context switch) 時，必須一併切換這些資訊。
### 核心名詞雙語解析 (Bilingual Definitions)

* **PTBR (Page-table base register / 分頁表基底暫存器)**
    * *Definition*: A hardware register that points to the page table stored in main memory.
    * *解釋*：當分頁表過於龐大無法塞入 CPU 暫存器時，分頁表會被移至主記憶體，此時硬體只需利用 PTBR 記住分頁表的「實體記憶體起點」即可。
* Page-table length register (PTLR) indicates size of the page table
* **TLB (Translation look-aside buffer / 轉譯後備緩衝區)**
    * *Definition*: A special, small, fast-lookup hardware cache associative memory used to store recently translated logical-to-physical address mappings.
    * *解釋*：建置於 CPU 管線內的極速快取。負責攔截邏輯位址，若內部有快取紀錄，就能瞬間吐出實體頁框編號，省去前往主記憶體查表的漫長等待。
* **ASID (Address-space identifier / 位址空間識別碼)**
    * *Definition*: An identifier stored in each TLB entry that uniquely identifies a process to provide address-space protection.
    * *解釋*：相當於 TLB 快取條目的「身分證」。它讓硬體知道這筆快取是屬於哪一個行程的，避免發生 A 行程讀取到 B 行程快取資料的致命錯誤。
* **EAT (Effective memory-access time / 有效記憶體存取時間)**
    * *Definition*: A weighted average time to access memory, considering the probability of a TLB hit versus a TLB miss.
    * *解釋*：將「TLB 命中時的極快速度」與「TLB 未命中時的極慢速度」，依照發生機率（命中率）進行加權平均，得出的真實系統效能指標。

### 一、 分頁表該放哪裡？

早期系統的分頁表較小（例如 256 個條目），可以直接實作為 CPU 內部的專屬高速暫存器 (dedicated high-speed hardware registers)。轉譯極快，但內容交換時必須把所有暫存器全部替換，非常耗時。

現代 CPU 支援巨大的分頁表（例如 $2^{20}$ 個條目），不可能塞進暫存器。因此，**分頁表被移往主記憶體存放**，硬體層級只保留一個 **PTBR (分頁表基底暫存器)** 指向該表格。

* *優點*：內容交換速度極快，只要修改一個 PTBR 暫存器的值即可。
* *致命缺點*：引發了災難性的**兩次記憶體存取問題 (two memory accesses)**。存取一筆資料，必須先去記憶體讀取分頁表，算出實體位址後，再去記憶體讀取真正的資料。這會讓記憶體存取速度直接慢一倍 (slowed by a factor of 2)。

### 二、 效能救星：TLB (轉譯後備緩衝區)

為了解決上述的兩次存取延遲，硬體導入了 **TLB (Translation Look-Aside Buffer)**。
TLB 是一種具備關聯性 (associative) 的高速記憶體，通常只包含 32 到 1024 個條目。硬體會拿著分頁編號 (Page number) 同時與 TLB 內所有的鍵值進行平行比對。

![image](https://hackmd.io/_uploads/rJrIFpa0We.png)

**【硬體轉譯流程 (參考 Figure 9.12)】**
1. CPU 產生邏輯位址，MMU 優先檢查 TLB。
2. **TLB Hit (命中)**：如果分頁編號在 TLB 中，直接取得實體頁框編號。因為與 CPU 管線高度整合，**幾乎沒有任何效能懲罰 (no performance penalty)**。
3. **TLB Miss (未命中)**：如果 TLB 找不到，硬體只好乖乖去主記憶體查分頁表。取得頁框編號後，不僅去存取資料，還會順手把這筆新的映射關係**寫入 TLB** 中，以利下次快速存取。
*(註：若 TLB 已滿，會觸發替換機制，常見策略如 LRU 最久未使用。作業系統的核心程式碼通常會被 wired down，鎖死在 TLB 中不被替換。)*

### 三、 ASID：跨行程的快取管理

當發生內容交換時，原本 TLB 裡面的快取資料是屬於「舊行程」的。如果新行程直接使用，會轉譯出錯誤的實體位址。

* **無 ASID 的系統**：每次內容交換，必須**徹底清空 (flushed / erased)** 整個 TLB。這會導致新行程剛開始執行時，頻繁遭遇 TLB Miss，嚴重拖慢效能。
* **有 ASID 的系統**：TLB 的每個條目都會綁定一個 **ASID (位址空間識別碼)**。MMU 會檢查 TLB 條目的 ASID 是否與當下執行的行程相符。這允許 TLB 同時存放多個不同行程的快取，免去了每次切換都要清空的昂貴代價，同時確保了位址空間的保護。

### 四、 效能指標：有效存取時間 (EAT)

TLB 能找對資料的機率稱為**命中率 (Hit ratio)**。透過命中率，可以精準計算出系統的平均效能，即**有效記憶體存取時間 (Effective Memory-Access Time, EAT)**。

課本提供了一個經典的推導模型：
假設存取一次主記憶體需要 10 奈秒 (nanoseconds)。

* **Hit Time (命中時間)**：10 奈秒 (直接去記憶體拿資料，TLB 查表時間極快趨近於 0)。
* **Miss Time (未命中時間)**：20 奈秒 (10 奈秒查分頁表 + 10 奈秒拿資料)。

若命中率為 80%：
$EAT = 0.80 \times 10 + 0.20 \times 20 = 12 \text{ ns}$ (比原生的 10ns 慢了 20%)。

若命中率提升至現實中常見的 99%：
$EAT = 0.99 \times 10 + 0.01 \times 20 = 10.1 \text{ ns}$ (效能損失僅剩 1%！)。

*(補充：現代高階 CPU 如 Intel Core i7 甚至實作了多層次的 TLB (L1 instruction, L1 data, L2)，這使得未命中的懲罰計算變得更加複雜，動輒需要耗費數百個週期去記憶體中漫遊尋找，再次凸顯了硬體設計對作業系統效能的決定性影響。)*

---

# 講義補充
---

### 核心名詞雙語解析 (Bilingual Definitions)

* **Page Table Entries (PTEs / 分頁表條目)**
    * *Definition*: The individual records within a page table that contain the mapping from a logical page to a physical frame, along with metadata for access control and page status.
    * *解釋*：分頁表中的每一列資料，除了實體頁框編號外，還夾帶了許多管理記憶體狀態的輔助資訊。


* **Effective Access Time (EAT / 有效存取時間)**
    * *Definition*: A performance metric calculating the average time required to access memory, factoring in the probabilities of TLB hits and misses along with their respective hardware delays.
    * *解釋*：衡量系統真實記憶體存取速度的核心指標。將「命中時的高速」與「未命中時的低速」進行機率加權平均。



---

### 分頁表條目 (Page Table Entries, PTEs) 剖析
![image](https://hackmd.io/_uploads/S11-X0pCZl.png)

分頁表並非單純的數字對照表，每個 **PTE (分頁表條目)** 都包含了一組控制映射狀態的位元 (Control mapping bits)：

1. **Modify (M) bit (修改位元 / 髒位元 Dirty bit)**：標示該分頁被載入記憶體後，是否曾經被寫入 (written) 修改過。
2. **Reference (R) bit (參照位元)**：標示該分頁近期是否被讀取或寫入過 (accessed)。這在後續章節的分頁替換演算法中極為重要。
3. **Valid (V) bit (有效位元)**：標示該 PTE 目前是否可以合法使用 (can be used)。
4. **Protection (Prot) bits (保護位元)**：定義該分頁允許的操作權限，例如讀取 (Read)、寫入 (Write) 或執行 (Execute)。
5. **Page Frame Number (PFN / 實體頁框編號)**：決定該邏輯分頁實際對應到的物理頁框位置。

⚠️ **【考試必殺技 (Exam Note)】**
講義底部特別用粉紅色字體標註：**在處理考試或作業時，若要計算分頁表等資料結構的「總大小 (Size of structures)」，通常會直接忽略 (ignore counting) M、R、V、Prot 等輔助位元，僅計算 PFN 所佔的空間**。除非題目有特別指示，否則請遵循此預設規則。

---

### 十六進位位址轉換實戰 (Paging Example Revisited)
![image](https://hackmd.io/_uploads/HktbmA6A-g.png)

講義提供了一個非常直觀的十六進位 (Hexadecimal) 位址轉換範例。因為十六進位的 1 個字元剛好代表 4 個二進位位元 (bits)，在硬體切割時非常乾淨俐落。

* **虛擬位址 (Virtual Address)**：`0xBAADF00D`
* **硬體拆解**：
* **Page number (分頁編號)**：擷取前面的 `0xBAADF`。
* **Offset (偏移量)**：擷取後面的 `0xF00D`。（這暗示了偏移量佔 16 bits，即該系統的分頁大小為 $2^{16} = 64$ KB）。


* **查表轉譯 (Page Table Lookup)**：
拿著 `0xBAADF` 去查分頁表，發現它對應到的 **Page frame (頁框編號)** 是 `0x900DF`。
* **實體位址 (Physical Address) 組合**：
將查到的頁框編號與原本不變的偏移量直接拼接：`0x900DF` + `0xF00D` $\rightarrow$ **`0x900DFF00D`**。

---

### 1. 理論推導與單位正規化公式

依據講義 `image_07791a.png`，定義以下變數：

* **$\varepsilon$**：Associative Lookup (TLB 搜尋時間)。
* **$\alpha$**：Hit ratio (TLB 命中率)。
* 假設單次記憶體存取時間為 **1 個單位 (unit)**。

**公式推導**：

* 若 **Hit (命中)**：耗時 = TLB 搜尋 + 1 次記憶體存取 = $(1 + \varepsilon)$。發生機率為 $\alpha$。
* 若 **Miss (未命中)**：耗時 = TLB 搜尋 + 1 次記憶體存取 (查分頁表) + 1 次記憶體存取 (取資料) = $(2 + \varepsilon)$。發生機率為 $(1 - \alpha)$。
*(手寫筆記補充了 2 代表：page 一次，最後一次)*。

將期望值相加得出正規化公式：


$$EAT = (1 + \varepsilon)\alpha + (2 + \varepsilon)(1 - \alpha) = 2 + \varepsilon - \alpha$$

### 2. 教授講義中的「陷阱」範例
==直接：命中機率 $\times$ 命中耗時 + 未命中機率 $\times$ 未命中耗時==
在同一張投影片中，提供了一個範例：$\alpha = 80\%, \varepsilon = 20ns$, 記憶體存取 = 100ns。
講義上寫的算式為：$EAT = 0.80 \times 100 + 0.20 \times 200 = 120ns$。
**⚠️ 注意**：這個算式**完全忽略了 TLB 搜尋時間 ($\varepsilon = 20ns$)**！這可能是為了簡化計算，或是假設 TLB 搜尋與其他硬體管線重疊而不計入延遲。

### 3. 嚴謹的實戰計算題 (Example)

然而，在最後一頁講義 `image_0775f0.png` 中的實戰題，**必須將 TLB 時間算進去**。這是考試時最標準的解題模型。

**【題目設定】**

* 記憶體存取時間 ($t_m$) = 5 us = 5000 ns。
* TLB 搜尋時間 ($t_{TLB}$) = 500 ns = 0.5 us。

**【題型一：如果完全沒有 TLB？】**
每次存取都必須查分頁表 + 取資料。

* 耗時 = $2 \times t_m = 2 \times 5 = \mathbf{10\ \mu s}$。

**【題型二：如果有 50% 的 TLB 命中率？】**

* **Hit Time** = TLB 搜尋 + 取資料 = $0.5 + 5 = 5.5\ \mu s$。
* **Miss Time** = TLB 搜尋 + 查分頁表 + 取資料 = $0.5 + 5 + 5 = 10.5\ \mu s$。
* $$EAT = (0.50 \times 5.5) + (0.50 \times 10.5) = 2.75 + 5.25 = \mathbf{8\ \mu s}$$



**【題型三：如果有 90% 的 TLB 命中率？】**

* Hit Time 與 Miss Time 同上。
* $$EAT = (0.90 \times 5.5) + (0.10 \times 10.5) = 4.95 + 1.05 = \mathbf{6\ \mu s}$$



**總結**：在遇到 EAT 計算題時，請嚴格按照題型二、三的「**$EAT = (Hit\ Ratio \times Hit\ Time) + (Miss\ Ratio \times Miss\ Time)$**」基礎邏輯進行推演，將每一個硬體動作（TLB 搜尋、分頁表存取、實體資料存取）的時間確實相加，即可確保計算萬無一失。
# 9.3.3 保護機制 (Memory Protection) 深度解析
### 核心名詞雙語解析 (Bilingual Definitions)
* **Protection bits (保護位元)**
    * Definition: Bits associated with each frame, kept in the page table, defining access rights like read-write, read-only, or execute-only.
    * 解釋：附加在分頁表條目中的權限標籤。當程式試圖對一個標示為「唯讀」的分頁進行寫入時，會引發記憶體保護違規 (memory-protection violation) 的硬體陷阱。
* **Valid-invalid bit (有效-無效位元)**
    * Definition: An additional bit attached to each entry in the page table. valid indicates the page is in the process's logical address space; invalid indicates that the page is not in the process's logical address space.
    * 解釋：最基礎的越界防護機制。硬體會檢查這個位元，只要是 invalid (無效)，就代表程式試圖存取別人的記憶體，作業系統會立刻介入攔截。
* **PTLR (Page-table length register / 分頁表長度暫存器)**
    * Definition: A hardware register indicating the size of the page table to prevent wasting memory on unused page-table entries.
    * 解釋：如果一個行程只用了極少的記憶體，幫它建立一整張涵蓋全部位址範圍的分頁表非常浪費。PTLR 讓分頁表可以「只建立真正有在用的長度」，查表前若邏輯位址大於 PTLR 標示的範圍，就直接判定錯誤。

### 一、 瞬間完成的權限審查
每當 MMU 透過分頁表將邏輯位址轉譯為實體位址時，它會「同時 (At the same time)」檢查該條目上的保護位元。
這種設計可以提供極度細緻的控制 (finer level of protection)，例如將某個分頁設定為只能讀取 (read-only)、只能執行 (execute-only)，或是各種權限的組合。任何非法的存取嘗試（例如對唯讀資料段寫入），都會直接產生硬體陷阱 (hardware trap) 交由作業系統處置。

### 二、 Figure 9.13：防護機制與它的「盲區」
![image](https://hackmd.io/_uploads/SyWgIR6R-x.png)
探討 Figure 9.13 所展示的經典系統漏洞 (Vulnerability) 情境。此現象與前述的**內部碎裂 (Internal Fragmentation)** 密不可分，完美點出硬體分頁防護的死角。
**【系統環境與情境設定 (System Setup)】**
* **定址空間 (Address Space)：** 14-bit，代表系統的邏輯位址 (Logical Address) 可用範圍為 0 到 16383。
* **分頁大小 (Page Size)：** 2 KB (2,048 Bytes)。
* **行程大小 (Process Size)：** 程式的真實資料量僅有 10,468 Bytes。也就是說，真正有意義的合法邊界只到 10468。

**【記憶體配發與防護標記 (Allocation & Protection Status)】**
1. **無條件進位的配發：** $10468 \div 2048 \approx 5.11$。由於記憶體必須以「分頁」為最小單位整塊給予，系統別無選擇，只能配發 6 個分頁 (Page 0 至 Page 5)。
2. 合法授權：分頁表中的 Page 0 到 Page 5 皆被標記為 `Valid (v)`。
3. 非法阻擋：剩餘未使用的 Page 6 與 Page 7 則被標記為 ``Invalid (i)``。
**【盲區出現 (The Problem)】**
* 成功攔截案例：若程式失控跑去存取位址 13000 (落在 Page 6)，硬體 MMU 查表發現狀態為 i，便會觸發陷阱 (Trap) 完美攔截越界行為。
* 盲區觸發案例：若程式因為 Bug 企圖存取位址 12000，狀況便截然不同！經計算，12000 屬於 Page 5，而 Page 5 的狀態標記是 v (合法)。此時，硬體會毫不猶豫地直接放行 (Access Granted)。
* 核心結論 (Conclusion)：即便程式的真實資料只到 10468，但從 10469 一直到 Page 5 的盡頭 12287，這段充滿無用垃圾的內部碎裂 (Internal Fragmentation) 空間，在硬體的判定下竟然皆屬「合法範圍」。這犀利地揭露了一項事實：依賴有效位元 (Valid-invalid bit) 的防護機制，其管控粒度 (Granularity) 只能精準到「分頁」，絕對無法做到 Byte 級別的微觀防禦。

**真正的危機：隱性錯誤 (Silent Failure)**
這就是課本所說的「問題」所在。
程式讀取到的 11000 位置，其實是內部碎裂 (Internal Fragmentation) 產生的垃圾空間。裡面裝的可能是全是零，也可能是上一個用過這塊記憶體留下來的無意義垃圾值 (Garbage data)。
因為硬體「放行」了這次存取，程式不會當機，它會把這個垃圾值拿去繼續做後續的數學運算或邏輯判斷。這會導致整個系統產出錯誤的結果，而工程師在除錯 (Debug) 時會痛苦萬分，因為根本不知道是在哪一步讀到了錯誤的資料。

**結論與解決方案**
因此，這個盲區的問題在於：硬體的防護網洞太大了，漏接了程式內部的越界錯誤。

為了解決這個 2KB 內部碎裂帶來的防護盲區，高階系統的硬體才會額外提供 PTLR (Page-table length register, 分頁表長度暫存器) 等輔助機制，試圖將防護邊界拉回更精準的尺度。
# 9.3.4 共享分頁 (Shared Pages)
### 核心名詞雙語解析 (Bilingual Definitions)
* **Reentrant Code (可重入程式碼)**
    * Definition: Non-self-modifying code that never changes during execution.
    * 解釋：在執行期間絕對不會修改自身的純程式碼。因為內容永遠不變，所以可以讓兩個以上的行程在「同一時間」安全地執行同一份實體程式碼。
* **Shared Code (共享程式碼)**
    * Definition: One copy of read-only code shared among processes.
    * 解釋：常駐在系統中被大量呼叫的工具（如標準 C 函式庫 libc、編譯器、視窗系統等），只需在實體記憶體保留一份唯讀副本。
* **Private Code and Data (私有程式碼與資料)**
    * Definition: A separate copy of code and data kept by each process for its own execution context.
    * 解釋：即使程式碼是共用的，每個行程執行時產生的變數、資料與暫存器狀態都是獨一無二的，這些私有資料會被映射到實體記憶體中獨立的分頁。

### 一、 龐大的空間節省 (A Significant Saving)
將程式打散成分頁後，帶來了一個極具價值的副作用：**記憶體共享 (Sharing of memory)**。這項技術在多重行程環境 (Multiple processes) 中至關重要。
課本提出了一個極具說服力的數學範例：
假設系統上有 40 個使用者行程，大家都需要用到標準 C 函式庫 (libc)。
* 如果不共享 (No sharing)：每個行程都把 2 MB 的 `libc` 複製一份到自己的空間。總共需要消耗 $40 \times 2 = \mathbf{80\ MB}$ 的實體記憶體。
* **如果共享 (With sharing)：** 實體記憶體中只保留唯一一份 2 MB 的 libc 副本。這 40 個行程的分頁表 (Page table)，通通指向同一組實體頁框。總耗費瞬間降為 $\mathbf{2\ MB}$，省下了高達 78 MB 的珍貴空間！

### 二、 講義註記破解：為什麼要特別寫上「病毒」？
教授在 Read-only (reentrant) code 旁邊手寫了醒目的「病毒」二字。這點出了一個核心資安議題。
因為這份程式碼是所有行程「共用」的，如果某個惡意程式（病毒）成功竄改了實體記憶體中 libc 的內容，等於瞬間感染或破壞了系統上所有正在執行該函式庫的 40 個行程！
因此，課本與講義皆嚴格強調：共享程式碼的唯讀特性 (Read-only nature)，絕對不能只仰賴程式本身的正確性，必須由作業系統 (Operating System) 透過底層硬體強制執行 (Enforce this property)。一旦有行程試圖寫入共享分頁，必須立刻觸發 Trap 攔截。

### 三、 Figure 9.14：公私分明的記憶體映射
![image](https://hackmd.io/_uploads/Hypl2R6CWl.png)
課本透過 Figure 9.14 展示了共享架構的運作邏輯：
* **共同指向：** Process $P_1$、$P_2$、$P_3$ 的邏輯位址中，都有 libc 1 到 libc 4 這四個分頁。查閱它們的分頁表，會發現大家都指向相同的實體頁框 3, 4, 6, 1。
* **私有資料：** 而在 libc 底下的 ... 分頁，代表的是各行程專屬的私有資料 (Private data)。這些私有資料的分頁表條目，就會各自指向實體記憶體中完全不重疊的空閒頁框。這確保了即使大家共用同一套邏輯，算出來的私密資料也絕對不會互相干擾。

### 四、 延伸應用：IPC (跨行程通訊)
除了共用唯讀的程式碼，如果作業系統允許行程之間共享「可讀寫 (Read-write)」的分頁，那麼這塊共享記憶體就成了一種極速的跨行程通訊 (Interprocess communication, IPC) 管道。這部分呼應了作業系統先前章節探討過的 IPC 機制。

# 9.4 分頁表結構 (Structure of the Page Table)
### 核心名詞雙語解析 (Bilingual Definitions)
* **Hierarchical Paging (階層式分頁)**
    * Definition: A technique to handle excessively large page tables by dividing the page table itself into smaller pieces, effectively paging the page table.
    * 解釋：當分頁表大到無法一次塞進連續記憶體時，我們用分頁機制的魔法來對付分頁表自己，把它切成好幾塊。
* **Forward-mapped page table (前向映射分頁表)**
    * Definition: Another term for the two-level paging scheme, because address translation works from the outer page table inward.
    * 解釋：階層式分頁的別稱。因為位址的轉譯是「由外而內」一路找進去的，如同沿著指標不斷往前追溯。

### 一、 為什麼需要改變結構？（肥大的分頁表）
現代電腦系統支援極大的邏輯位址空間（通常是 $2^{32}$ 到 $2^{64}$）。
我們來做一個基礎的數學計算：
* 假設一個 32 位元的邏輯位址空間，分頁大小為 4 KB ($2^{12}$ Bytes)。
* 分頁表會有多少個條目？ $2^{32} \div 2^{12} = \mathbf{2^{20}}$ (超過 100 萬個條目)。
* 如果每個條目佔用 4 Bytes，那麼每個行程光是「分頁表本身」，就需要高達 4 MB 的實體記憶體！

作業系統絕對不想為了存放一張對照表，而被迫在主記憶體中硬擠出 4 MB 的「連續」空間。最簡單的解法，就是把這張巨大的分頁表切成小塊。

### 二、 9.4.1 階層式分頁 (Hierarchical Paging) 的運作邏輯
以最常見的**雙層分頁演算法 (two-level paging algorithm) 為例**，我們把原本一長串的 20 bits 分頁編號，再切一刀。
![image](https://hackmd.io/_uploads/SkGLaCTCZl.png)
**【位址拆解 (32-bit 架構)】**
* $p_1$ (10 bits)：用來查詢「外部分頁表 (Outer page table)」。
* $p_2$ (10 bits)：用來查詢「內部分頁表 (Inner page table)」。
* $d$ (12 bits)：分頁內的實體偏移量 (Page offset)。


**【轉譯管線 (對應 Figure 9.16)】**
![image](https://hackmd.io/_uploads/H1JnT0pAWl.png)
1. CPU 吐出邏輯位址，硬體先抽取出 $p_1$。
2. 拿著 $p_1$ 去查外部分頁表，這個表裡存的不是資料的實體位址，而是「內部分頁表在哪裡」的指標。
3. 找到對應的內部分頁表後，拿著 $p_2$ 去查表，**這時才會得到真正的實體頁框編號** (Frame number)。
4. 最後，將頁框編號與位移量 $d$ 組合，成功取得實體記憶體位址。

這種作法不僅免除了連續 4 MB 的空間需求，如果程式只用到一小部分記憶體，作業系統甚至可以選擇「不建立」那些沒用到的內部分頁表，極大幅度地節省了記憶體。

### 三、 64 位元架構的噩夢 (The 64-bit Problem)
雙層分頁在 32 位元下是完美的，但當我們進入 64 位元 的世界，這套邏輯瞬間崩塌。
**【極限推導】**
如果繼續維持 4 KB ($2^{12}$) 的分頁大小，外加雙層分頁設計：
* 內部分頁表佔用 10 bits ($p_2$)。
* 偏移量佔用 12 bits ($d$)。
* 剩下的 42 bits ($p_1$) 全部塞給外部分頁表。

悲劇發生了：這張外部分頁表將會包含 $2^{42}$ 個條目，總大小高達 $2^{44}$ Bytes (也就是 16 TB！)。為了存放這張表，我們可能要把這張表繼續往下切成三層、四層甚至更多層。

課本點出一個致命現實：如果使用 64 位元的 UltraSPARC 架構，為了把表格切到夠小，系統將會需要高達 7 層的分頁 (seven levels of paging)。這代表 CPU 每存取一筆資料，就要在記憶體裡面查表 7 次！這種過高的記憶體存取次數 (prohibitive number of memory accesses)，會讓系統效能直接跌入谷底。

因此，對於 64 位元架構而言，傳統的階層式分頁被認為是不切實際的 (inappropriate)，必須尋求其他結構（如後續小節的 Hashed 或 Inverted page tables）。

