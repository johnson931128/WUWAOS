---
title: ch6 Synchronization Tools

---

# 6.1 Background
### 一、 背景回顧：並發與平行的資料危機
在先前的章節中，系統已經引入了 CPU 排程 (CPU Scheduling) 的概念，排程器會在不同的行程之間快速切換，提供 **Concurrent execution**(並發執行) 的幻覺。這意味著一個行程可能執行到一半，指令流 (Instruction stream) 就被強制中斷，CPU 被交接給另一個行程使用。此外，在多核心系統中，甚至會出現 **Parallel execution** (平行執行)，兩個行程在不同的實體核心上「同時」執行。

本章 (Chapter 6) 的核心目的，就是要解釋：當這些被隨機中斷、交錯執行的行程，試圖**存取同一塊共享資料** (Shared data) 時，會如何破壞資料的完整性 (Integrity)。

### 二、實作修改：Bounded Buffer (有限緩衝區) 與 `count` 變數
為了具體說明資料崩潰的過程，課本重新引用了第 3 章的「生產者－消費者問題 (Producer-Consumer Problem)」。
* **舊版設計缺陷：** 原本的解法為了解決指標重疊的問題，最多只能允許緩衝區存放 `BUFFER_SIZE - 1` 個物品，浪費了一個空間。
* **新版改良方案：** 為了解決這個缺陷，系統工程師決定加入一個共享的整數變數 `count`，並將其初始值設為 `0`。
    * 每當生產者成功放入一個物品，就將 `count` 加 1 (`count++`)。
    * 每當消費者成功取出一個物品，就將 `count` 減 1 (`count--`)。

【生產者程式碼實作 (Producer Process)】
```c
while (true) {
    /* 產生一個新物品，暫存在 next_produced 中 */
    
    // 若緩衝區已滿，則陷入忙碌等待 (Busy waiting)，什麼都不做
    while (count == BUFFER_SIZE)
        ; /* do nothing */
    
    // 將新物品放入緩衝區的 in 索引位置
    buffer[in] = next_produced;
    // 更新 in 的索引值 (使用餘數運算達成環狀佇列效果)
    in = (in + 1) % BUFFER_SIZE;
    // 成功放入物品，將總數加 1
    count++;
}
```
* **程式碼解釋：** 生產者負責不斷製造物品。寫入物品前，必須先檢查 count 是否已達上限。寫入後，針對共享變數執行 `count++`。

【消費者程式碼實作 (Consumer Process)】
```c
while (true) {
    // 若緩衝區為空，則陷入忙碌等待，什麼都不做
    while (count == 0)
        ; /* do nothing */
    
    // 從緩衝區的 out 索引位置取出物品
    next_consumed = buffer[out];
    // 更新 out 的索引值
    out = (out + 1) % BUFFER_SIZE;
    // 成功取出物品，將總數減 1
    count--;
    
    /* 處理或消耗剛取出的 next_consumed 物品 */
}
```
* **程式碼解釋：** 消費者負責消耗物品。讀取前需確保 count 不為零。取出物品後，針對共享變數執行 `count--`。

### 三、災難的開端：機器語言的底層拆解
上述兩段程式碼若「分開獨立執行 (Execute separately)」，邏輯絕對正確。但若將它們放入多工系統中「並發執行 (Execute concurrently)」，將會產生致命錯誤。

假設目前緩衝區內有 5 個物品 (`count = 5`)。生產者與消費者幾乎在同一個時間點分別執行 `count++` 與 `count--`。邏輯上，一增一減抵銷後，結果應該維持為 5。但實際上，結果可能是 4、5 或 6！

原因在於，高階語言的一行指令，在底層機器語言 (Machine Language) 會被拆解為三個獨立的步驟：

* **`count++` 的底層拆解：**
```cpp
register1 = count           // 步驟 1：把共享記憶體中的 count 值讀進 CPU 私有暫存器 1
register1 = register1 + 1   // 步驟 2：在 CPU 內部將暫存器的值加 1
count = register1           // 步驟 3：將計算好的暫存器值寫回共享記憶體的 count
```
* **`count--` 的底層拆解：**
```cpp
register2 = count           // 步驟 1：把共享記憶體中的 count 值讀進 CPU 私有暫存器 2
register2 = register2 - 1   // 步驟 2：在 CPU 內部將暫存器的值減 1
count = register2           // 步驟 3：將計算好的暫存器值寫回共享記憶體的 count
```
*(備註：`register1` 與 `register2` 是 CPU 內部的暫存器。即使它們在物理上對應到同一個暫存器，在 Context Switch 切換時，作業系統的中斷處理常式 (Interrupt Handler) 也會將其數值各自備份與還原，因此可以將其視為兩個獨立的工作空間。)*

### 四、Race Condition (競態條件) 的誕生
![image](https://hackmd.io/_uploads/By9zevna-e.png)
作業系統排程器隨時可能中斷行程。若指令交錯 (Interleaved) 的順序如下（即圖片中的 $T_0$ 到 $T_5$），錯誤便發生了：
* **狀態預設：** 初始 `count = 5`。
* **$T_0$ (生產者)：** 執行 `register1 = count` $\rightarrow$ 此時生產者的 `register1` 記住的是 5。
* **$T_1$ (生產者)：** 執行 `register1 = register1 + 1` $\rightarrow$ 此時生產者的 `register1` 變成 6。
    * 【排程器強制中斷 (Context Switch)】：生產者被暫停，換消費者上陣。
* **$T_2$ (消費者)：** 執行 `register2 = count` $\rightarrow$ 注意！因為生產者還沒把 6 寫回記憶體，此時記憶體裡的 `count` 還是 5。所以消費者的 `register2` 記住的是 5。
* **$T_3$ (消費者)：** 執行 `register2 = register2 - 1` $\rightarrow$ 此時消費者的 `register2` 變成 4。
    * 【排程器再次切換】：消費者被暫停，換生產者上陣。
* **$T_4$ (生產者)：** 執行 `count = register1` $\rightarrow$ 生產者將手上的 6 寫入記憶體的 `count`
* **$T_5$ (消費者)：** 執行 `count = register2` $\rightarrow$ 消費者將手上的 4 寫入記憶體的 `count`，無情地覆蓋掉了生產者的 6！

### 五、核心定義與本章展望
* **Race Condition (競態條件)：** 如上述推演，當多個行程並發存取與操作共享資料，且最終的執行結果完全取決於存取發生的「特定順序 (Particular order)」時，這種極度不穩定的狀態就稱為 Race Condition。
* **解決方案宣告：** 為了防範這種災難，系統必須提供保證：「同一個時間點，絕對只能有一個行程可以操作共享變數 `count`」。這要求行程之間必須在某種程度上進行同步 (Synchronized)。

這正是多核心 (Multicore) 與多執行緒 (Multithreaded) 應用程式開發中最具挑戰性的部分。本章後續的所有內容，將全數奉獻給 Process synchronization (行程同步) 與 Coordination (協調) 的底層機制與演算法設計。
### 六、Process 與 Thread 的共享資源危機
1. **情境一：兩個獨立的 Processes (行程)**
行程之間在實體記憶體中是被作業系統嚴格物理隔離的。若生產者與消費者是兩個完全獨立的 Processes，要讓它們都能夠讀取並修改同一個 `count` 變數與 `buffer` 陣列，系統必須事先建立 **Shared Memory** (共享記憶體) 通道。兩者必須將這塊共享區域映射到各自的位址空間，才能進而引發 Race Condition (競態條件)。

2. **情境二：兩個 Threads (執行緒) —— 最致命的災難溫床**
隨著多核心 (Multicore) 系統普及，現代軟體開發更常將生產者與消費者設計為**同一個 Process 底下的兩個 Threads (執行緒)**。
    * 在此架構下，執行緒天生就會「無條件共享」同一個資料區段 (Data Section) 與全域變數。
    * 若將 `count` 宣告為程式中的全域變數，生產者執行緒與消費者執行緒完全不需要任何額外設定，就能直接對其進行讀寫。這導致多執行緒應用程式 (Multithreaded applications) 在平行運算時，極度容易因為開發者的疏忽而引發資料不一致的毀滅性錯誤。

3. **結論**
無論教科書上使用的名詞是 Processes 還是 Threads，只要同時滿足以下兩個物理條件，這套資料崩潰的劇本就會上演：
    * 多個實體處於非同步的並發執行 (Asynchronous concurrent execution) 狀態。
    * 允許多個實體在缺乏同步防護機制的狀況下，同時存取並操作同一塊記憶體變數。

---
# 6.2 The Critical-Section Problem
### 一、系統架構與區塊劃分
假設系統中存在 $n$ 個行程 $\{P_0, P_1, \dots, P_{n-1}\}$。每個行程的程式碼中，都包含一段被稱為 **Critical Section** (臨界區間) 的特殊區塊，行程會在此區塊內存取或修改與其他行程共享的資料。

系統設計的最大原則：**當一個行程正在執行其臨界區間時**，**絕對不允許其他任何行程進入它們的臨界區間（即沒有兩個行程能同時在臨界區間內執行）**。

為落實這項協定，行程的程式碼結構被嚴格劃分為四個物理階段：
![image](https://hackmd.io/_uploads/BJ6GBvhaWl.png)
1. **Entry section (進入區間)：** 在觸碰共享資料前，必須先在此處執行「請求許可」的邏輯。
2. **Critical section (臨界區間)：** 真正操作共享資料的核心防護區域。
3. **Exit section (離開區間)：** 離開時，必須在此處「釋放許可」，讓其他等待的行程知曉。
4. **Remainder section (剩餘區間)：** 完全不涉及共享資料的其他一般應用程式邏輯。

### 二、完美解法的「三大鐵律」 (Three Requirements)
要徹底解決臨界區間問題，設計出的演算法必須毫無瑕疵地滿足以下三個條件：
1. **Mutual exclusion (互斥)：**
    若行程 $P_i$ 正在其臨界區間內執行，則其他所有的行程皆不得進入其各自的臨界區間。此為保護資料不被交錯破壞的最基本底線。
2. **Progress (進行)：**
    若目前沒有任何行程在臨界區間內，且有部分行程正在等候進入，則**「只有那些不在剩餘區間 (Remainder section) 執行的行程」**有資格參與決定下一個誰能進入，且這項決定不能被無限期推遲 (Postponed indefinitely)**。
    * **白話文：** 不能發生死結 (Deadlock)。不想進去的人，絕對不能佔著茅坑不拉屎、卡住想進去的人。
3. **Bounded waiting (有限等待)：**
    當一個行程提出進入臨界區間的請求後，到該請求被核准之前，其他行程被允許進入其臨界區間的「次數必須有一個上限 (Bound / Limit)」。
    * **白話文：** 不能發生飢餓 (Starvation)。每個排隊的行程最終都必須保證能進得去，不能被其他優先權更高的行程無限期插隊。

### 三、作業系統核心的競態條件與防禦
![image](https://hackmd.io/_uploads/rkdiUv3aWg.png)
不僅一般應用程式會遇到此問題，作業系統核心 (Kernel) 本身也充滿了競態條件 (Race Conditions)。例如，當兩個行程同時呼叫 `fork()` 建立子行程時，若未做好互斥管控，可能會同時讀取並分配到同一個 `next_available_pid`，導致兩個不同的行程獲得完全相同的 行程識別碼 (PID)，如 Figure 6.2 所示。
針對核心層級的資料防護，有兩種架構流派：
* **Nonpreemptive kernels (非搶占式核心)：**
    行程在 Kernel mode 執行時絕對不會被強制中斷，直到它離開核心模式、發生阻塞或主動交出 CPU 為止。這本質上免疫了核心資料結構的競態條件，因為同時間只會有一個行程活躍於核心中。
* **Preemptive kernels (搶占式核心)：**
    允許在 Kernel mode 執行的行程被更高優先權的任務搶占。雖然此設計在多處理器 (SMP) 架構下極度困難，必須佈滿防護鎖，但因其具備極高的反應速度 (More responsive)，且能確保即時任務 (Real-time process) 不會被底層系統呼叫卡死，因此成為現代高效能與即時作業系統的標準配備。
    
### 四、演算法實務推演：交替演算法 (Alternating Algorithm)
工程師在早期嘗試設計同步協定時，提出了一種適用於兩個行程 ($P_i$ 與 $P_j$) 的解決方案，稱為「交替演算法」。此演算法利用一個全域共享變數 `int turn`; 來當作進入臨界區間的「通行證」。
* 【Process $P_i$ 程式碼深度解析】
```cpp
// 系統初始化時，假設 turn = i 或 j
do {
    /* * Entry Section (進入區間)
     * 不斷檢查 turn 是否被對手 j 拿走。
     * 若 turn == j，代表目前不是自己的回合，行程會在此處執行無意義的空迴圈，
     * 這在硬體底層被稱為「忙碌等待 (Busy Waiting)」或「自旋鎖 (Spinlock)」。
     */
    while (turn == j)
        ; /* do nothing */

    /* * Critical Section (臨界區間)
     * 成功突破 while 迴圈，代表 turn == i。
     * 在此處可以安全地存取或修改共享資料。
     */

    /* * Exit Section (離開區間)
     * 執行完畢，將通行證強制交給對手 j。
     */
    turn = j;

    /* * Remainder Section (剩餘區間)
     * 執行與共享資料無關的其他運算邏輯。
     */
} while (true);
```
* 【Process $P_j$ 程式碼結構】
```cpp
do {
    // Entry Section：等待回合變為 j
    while (turn == i)
        ; /* do nothing */

    // Critical Section
    /* 操作共享資料 */

    // Exit Section：將回合交還給 i
    turn = i;

    // Remainder Section
    /* 執行其他邏輯 */
} while (true);
```
* 【架構師評估：此演算法及格嗎？】
    1. **是否滿足互斥 (Mutual Exclusion)？**
        是的。 因為全域變數 `turn` 的數值在同一個物理時間點上，只能是 $i$ 或是 $j$，絕對不可能同時滿足兩個條件。因此 $P_i$ 和 $P_j$ 絕對不可能同時闖入臨界區間。
    2. **是否滿足進行 (Progress)？**
        否，存在致命缺陷。 此演算法的設計強制兩人必須「嚴格交替 (Strict Alternation)」。
        * **災難推演：** 假設目前 `turn == i`，行程 $P_i$ 進入了臨界區間，出來後將 `turn` 設為 $j$，然後去執行它極度漫長的「剩餘區間 (Remainder Section)」。
        * 此時 $P_j$ 順利進入臨界區間，出來後將 `turn` 設回 $i$。
        * 若 $P_j$ 處理速度極快，又想再次進入臨界區間，它會在進入區間 `while (turn == i)` 被死死卡住。
        * 即使此時臨界區間是完全空閒的，但因為 $P_i$ 還在慢慢執行它的剩餘區間，遲遲不回來把 `turn` 改成 $j$，$P_j$ 就永遠進不去。這嚴重違反了 Progress 的定義（**不在剩餘區間的行程才能參與決策**），因此交替演算法在軟體工程上是一個失敗的設計。
### 五、第一次改良：旗標演算法 (Flag Algorithm)
既然用單一變數 `turn` 會導致強制輪流，工程師決定改用一個布林陣列 `flag[2]`。
* `flag[i]` = `TRUE` 代表行程 $P_i$ 「想要」進入臨界區間。
* `flag[i]` = `FALSE` 代表行程 $P_i$ 目前不想進入。
```cpp
do {
    // 1. 舉手表達意願
    flag[i] = TRUE; 
    
    // 2. 檢查對手是否也舉手了。如果對手也想進去，我就在這邊卡住 (忙碌等待)
    while (flag[j]) ; 
    
    // --- 進入臨界區間 (Critical Section) ---
    
    // 3. 離開時，放下手 (取消意願)
    flag[i] = FALSE; 
    
    // --- 進入剩餘區間 (Remainder Section) ---
} while (1);
```
這套演算法雖然成功達成了「互斥 (Mutual Exclusion)」，不會讓兩個人同時進去，但投影片拋出了一個致命的 Hint：「如果兩個 `flag` 都為 `true` 呢？」
* **災難推演 (死結 Deadlock)：** 假設 $P_i$ 剛執行完 `flag[i]` = `TRUE`;，這瞬間發生了 Context Switch (內容交換)。接著 $P_j$ 上陣，也執行了 `flag[j]` = `TRUE`;。
* 此時兩人同時來到 `while` 迴圈檢查對方。
* $P_i$ 發現 `flag[j]` 是 TRUE，卡住；$P_j$ 發現 `flag[i]` 也是 `TRUE`，也卡住。兩人都互相等待對方放下手，結果就是誰都進不去，系統永遠卡死。這嚴重違反了 進行 (Progress) 的條件。

### 六、完美解法：綜合演算法 (Peterson's Solution)
為了打破死結，工程師將前面兩種失敗的演算法（交替演算法的 turn 與旗標演算法的 flag）結合在一起，誕生了極度優雅的綜合演算法。
* **變數定義：** `flag` 陣列依然用來記錄「是否想進入」，而 `turn` 變數用來指出在雙方都想進入的衝突時刻，系統「允許誰先進入」。

```cpp
do {
    // 1. 舉手表達意願：我想進去！
    flag[i] = TRUE; 
    
    // 2. 核心精神 (孔融讓梨)：把優先權讓給對方
    turn = j; 
    
    // 3. 雙重檢查機制：
    // 如果「對方也想進去 (flag[j] == TRUE)」 且 「現在是對方的回合 (turn == j)」，我就等。
    // 反過來說，只要對方不想進去，或者現在是我的回合，我就能突破迴圈！
    while (flag[j] && turn == j) ; 
    
    // --- 進入臨界區間 (Critical Section) ---
    
    // 4. 離開時，放下手 (取消意願)
    flag[i] = FALSE; 
    
    // --- 進入剩餘區間 (Remainder Section) ---
} while (1);
```
這個演算法同時滿足了三大鐵律：
1. **互斥 (Mutual Exclusion)：** 若兩人同時想進去，`turn` 變數最終只會是一個值（不是 $i$ 就是 $j$），這保證了 `while` 迴圈一定會卡住其中一個人，只有一人能突破。
2. **進行 (Progress) 解決死結：** 回顧剛才的死結情境，若兩人都把 `flag` 設為 TRUE，接著他們會爭相去修改 `turn` 的值。無論誰先修改，`turn`的最終結果一定能讓其中一人打破 `while` 迴圈的條件順利進入，絕對不會互相卡死。
3. **有限等待 (Bounded Waiting)：** 當 $P_i$ 在等待時，即便 $P_j$ 執行完畢離開後立刻又想進去，$P_j$ 在執行 `turn = i`; 時會強制把優先權交還給 $P_i$，確保 $P_i$ 最多只會被插隊一次，不會發生飢餓。

---
# 6.3 Peterson’s Solution
### 一、 演算法架構與變數定義
Peterson's Solution 是一個被嚴格限制在「兩個行程 ($P_0$ 與 $P_1$)」之間交替執行的演算法。為了方便推演，課本約定：當探討 $P_i$ 時，它的對手就是 $P_j$ (其中 $j = 1 - i$)。
此演算法依賴兩個共享資料結構：
1. `int turn;`
表示「現在是誰的回合」。如果 `turn == i`，代表 $P_i$ 被允許進入臨界區間。
2. `boolean flag[2];`
這是一個陣列，用來表示某個行程是否「準備好/有意願」進入臨界區間。如果 `flag[i]` 為 true，代表 $P_i$ 想進去。

```cpp
while (true) {
    flag[i] = true;   // 1. 舉手：宣告自己想進入臨界區間
    turn = j;         // 2. 禮讓：將回合（優先權）交給對手 j
    
    // 3. 雙重檢查：如果「對手也想進去」且「現在是對手的回合」，我就陷入忙碌等待
    while (flag[j] && turn == j)
        ; 
        
    /* critical section (臨界區間) */
    
    flag[i] = false;  // 4. 離開時，放下手：宣告自己不想進去了
    
    /* remainder section (剩餘區間) */
}
```
**核心邏輯：** 當 $P_i$ 想要進入時，它會先把 `turn` 設為 `j`。如果兩個行程「同時」嘗試進入，它們會幾乎同時修改 `turn` 的值。但記憶體的物理特性保證，這兩個賦值動作總會有一個先後順序，最後一個寫入的值會保留下來，另一個則被覆蓋。這個最終存活下來的 `turn` 值，就決定了誰能打破僵局、率先進入臨界區間。

### 二、 三大鐵律的數學證明
1. **證明互斥 (Mutual Exclusion)：**
若 $P_0$ 和 $P_1$ 要能「同時」在臨界區間內，代表它們都成功突破了 while 迴圈。此時 `flag[0] == true` 且 `flag[1] == true`。但要突破迴圈，`turn` 的值不能是對手的。也就是說，$P_0$ 需要 `turn == 0`，$P_1$ 需要 `turn == 1`。然而，`turn` 是一個整數變數，它不可能同時既是 `0` 又是 `1`。因此，必定有一個行程被卡在 `while` 迴圈中，互斥條件成立。

2. **證明進行 (Progress) 與有限等待 (Bounded Waiting)：**
$P_i$ 被卡住的「唯一可能」，是 `flag[j] == true` 且 `turn == j`。
* 如果對手 $P_j$ 根本不想進去 (`flag[j] == false`)，$P_i$ 就能長驅直入（滿足 Progress）。
* 如果兩人都想進去，當 $P_j$ 執行完畢離開臨界區間時，它會把 `flag[j]` 設為 `false`，這會瞬間釋放被卡住的 $P_i$。
*  即使 $P_j$ 離開後又立刻「反悔」想再次進入，它在執行 entry section 時，必須無條件執行 `turn = i` 把優先權交還給 $P_i$。因此 $P_i$ 最多只會被插隊一次（滿足 Bounded Waiting）。


### 三、 現代硬體的背叛：指令重排 (Instruction Reordering)
這是本節最重要的實務考點。雖然 Peterson's Solution 在數學理論上完美無瑕，但課本無情地宣告：**它在現代計算機架構中**，**是沒有保證能正確運作的** (no guarantees that Peterson's solution will work correctly)。

* 【問題的根源：效能榨取】
為了追求極致效能，現代的處理器 (Processors) 或編譯器 (Compilers) 擁有「重新排序 (Reorder)」指令流的權力。只要兩個指令之間「沒有資料相依性 (no data dependencies)」，硬體就有可能把它們順序對調執行。
```cpp
// 共享變數
boolean flag = false;
int x = 0;

// Thread 1
while (!flag) ;  // 等待 flag 變成 true
print x;

// Thread 2
x = 100;
flag = true;
```
* **預期行為：** Thread 2 設好 `x=100` 後拉起 `flag`。Thread 1 看到 `flag` 被拉起，印出 `100`。
* **硬體重排災難：** 對於 Thread 2 來說，`x = 100` 和 `flag = true` 這兩行程式碼操作的是完全不同的變數，毫無相依性。處理器為了最佳化，可能會先把 `flag` 設為 `tru`e，才去寫入 `x = 100`。若在此瞬間 Thread 1 突破迴圈，它印出的 `x` 將會是錯誤的 `0`。

**現在我們把「指令重排」套用到 Peterson's Solution 最致命的前兩行程式碼：**
![image](https://hackmd.io/_uploads/H10Oa_np-x.png)
```cpp
flag[i] = true;
turn = j;
```
這兩行分別修改 `flag` 陣列和 `turn` 變數，兩者沒有資料相依性。因此，現代硬體極有可能把它們對調執行！
1. $P_0$ 被重排，先執行了 `turn = 1`，但還沒來得及把 `flag[0]` 設為 `true`。
2. 此時 Context Switch，$P_1$ 上陣。
3. $P_1$ 正常執行，`flag[1] = true`，然後 `turn = 0`。
4. $P_1$ 進入 `while` 檢查：因為 $P_0$ 的 `flag[0]` 還是 `false`，$P_1$ 順利闖入臨界區間！
5. Context Switch 回到 $P_0$。$P_0$ 補上 `flag[0]` = `true`。
6. $P_0$ 進入 while 檢查：雖然 `flag[1]` 是 `true`，但是剛才 $P_1$ 已經把 `turn` 改成 0 了！因為 `turn == 0`（不是對手的回合），$P_0$ 也順利闖入了臨界區間！
**結果：互斥 (Mutual Exclusion) 徹底破功，兩個行程同時在臨界區間內。**

---

# 補充：麵包店演算法
前一節的 Peterson's Solution 雖然優雅，但只能解決兩人的紛爭。當系統中有 $n$ 個行程（例如幾十個執行緒在搶奪同一個資料庫鎖）時，工程師 Leslie Lamport 發明了這個直觀且強大的演算法。

### 一、 變數定義與抽牌機制
這套演算法需要兩個長度為 $n$ 的共享陣列：
1. `boolean choosing[n];`
初始值皆為 `FALSE`。用來記錄某個行程是否「正在抽號碼牌」。
2. `int number[n];`
初始值皆為 `0`。這就是大家抽到的號碼牌。
3. **抽牌規則 (核心精神)**
當行程 $P_i$ 想要進入臨界區間時，它會去偷看目前所有人手上的號碼牌，找出最大的那個數字，然後加 1 當作自己的號碼。`number[i] = max(number[0], number[1], ..., number[n-1]) + 1;`

### 二、 程式碼拆解 (Figure 補充)
以下是行程 $P_i$ 運作的完整邏輯：
```cpp
do {
    /* ------------------- 抽牌階段 ------------------- */
    choosing[i] = TRUE; // 宣告：我正在抽號碼牌，請大家先不要跟我比對
    
    // 找出目前全場最大號碼，加 1 後拿走
    number[i] = max(number[0], number[1], ..., number[n-1]) + 1; 
    
    choosing[i] = FALSE; // 宣告：我抽完了
    /* ------------------------------------------------ */

    /* ------------------- 等待階段 ------------------- */
    // 逐一檢查其他所有行程 (從 P_0 檢查到 P_n-1)
    for (j = 0; j < n; j++) {
        // 1. 如果 P_j 正在抽牌，我就等他抽完。
        // 因為他可能抽到跟我一樣大的牌，必須等他確定號碼才能比大小。
        while (choosing[j]) 
            ; 
            
        // 2. 雙重檢查：如果 P_j 也有號碼牌 (number[j] != 0)，
        //    並且 P_j 的順位比我高，我就陷入忙碌等待。
        //    【順位比較規則】：(number[j], j) < (number[i], i)
        while ((number[j] != 0) && 
               ((number[j], j) < (number[i], i))) 
            ; 
    }
    /* ------------------------------------------------ */

    /* 臨界區間 (Critical Section) */

    // 離開時，把號碼牌撕掉 (設為 0)
    number[i] = 0; 

    /* 剩餘區間 (Remainder Section) */
} while (1);
```
### 三、 決勝點 (Tie-breaker) 的物理意義

這套演算法最精妙的設計在於它的順位比較規則：`(number[j], j) < (number[i], i)`。
在多核心系統中，如果 $P_1$ 和 $P_2$「同時」去偷看大家手上的牌（假設目前最大是 5），那他們會「同時」算出 6，並且「同時」把自己的號碼牌設為 6！這時候兩人手上的牌是一樣的（平手）。為了打破僵局，演算法引入了 **Lexicographical order** (字典序比較)：
1. **先比號碼：** 號碼小的優先 `(number[j] < number[i])`。
2. **號碼一樣，比 PID (行程編號)：** 如果兩人號碼一樣大，則行程編號較小的優先 (`j < i`)。

假設 $P_1$ 抽到 6，$P_2$ 也抽到 6。
* $P_1$ 檢查到 $P_2$ 時，比對 `(6, 2) < (6, 1)`，結果為 False。$P_1$ 不用等，直接闖入臨界區間！
* $P_2$ 檢查到 $P_1$ 時，比對 `(6, 1) < (6, 2)`，結果為 True。$P_2$ 乖乖卡在 while 迴圈罰站，直到 $P_1$ 離開並把 `number[1]` 設回 `0`。

### 四、 架構師評估與現代限制
* **理論成就：** 這是一個極度強大的純軟體演算法，它成功在沒有任何硬體特殊指令支援的情況下，解決了 $n$ 個行程的互斥問題。
* **現代硬體的背叛：** 如同前一節對 Peterson's Solution 的宣判，Baker's Algorithm 同樣會受到現代處理器指令重排 (Instruction Reordering) 的毀滅性打擊。如果在抽牌階段，寫入 `number[i]` 與 `choosing[i]` 的順序被硬體偷換，整個排隊機制就會瞬間崩潰。因此在現代作業系統開發中，這通常只作為學術理論探討，實務上仍需依靠硬體鎖。

---





























# 6.4 Hardware Support for Synchronization
## 6.4.1 Memory Barrier
### 目的原因
前一節 (6.3) 帶來了一個殘酷的現實：因為現代處理器與編譯器會為了效能而偷偷調換指令順序，導致純軟體的同步演算法在多核心系統上徹底失效。6.4 節開始，作業系統工程師決定直接呼叫硬體層級的支援 (Hardware Support) 來鎮壓這個亂象。
### 一、 系統的底層物理定律：記憶體模型 (Memory Models)
在探討解法前，課本先定義了現代計算機架構的 **Memory Model (記憶體模型)**。這代表硬體對軟體開發者所做出的「記憶體可見性保證」。主要分為兩大陣營：
1. **Strongly ordered (強排序)：**
    在某一顆處理器上對記憶體進行的修改，會**立刻 (immediately)** 顯現並讓其他所有的處理器看見。
2. **Weakly ordered (弱排序)：**
    在某一顆處理器上對記憶體的修改，不一定會立刻讓其他處理器看見。
    **實務補充：** 為了追求極致的運算速度，現代多核心處理器會把資料暫存在自己專屬的 L1 Cache 裡，稍後才統一寫回主記憶體。這導致了嚴重的「時間差」，也就是弱排序的物理根源。

因為系統核心開發者 (Kernel developers) 無法假設使用者的電腦到底是哪一種記憶體模型，硬體廠商必須提供一種「強制力」，這就是 Memory Barriers。

### 二、 硬體級鎮暴警察：記憶體屏障 (Memory Fences)
為了解決跨處理器的可見性與重排問題，計算機架構提供了一種特殊指令，稱為 **Memory Barriers** 或 **Memory Fences (記憶體柵欄)**。
* **運作邏輯：** 當程式執行到這個指令時，系統會**強制確保**在此指令之前的「所有載入 (loads) 與儲存 (stores) 操作」都已經徹底完成，且修改的記憶體數值已經傳播給其他處理器看見，然後系統才會被允許執行柵欄之後的讀寫操作。
* **防禦效果：** 這完美抵禦了指令重排！因為硬體絕對不敢把柵欄後面的指令偷換到柵欄前面。

### 三、 程式碼實戰：修復 6.3 節的重排災難
把 6.3 節因為指令重排而崩潰的範例拿回來，加上硬體指令 `memory_barrier()` 進行修復：
**【修復前：可能印出錯誤的 `x = 0`】**
* Thread 2 若先執行 `flag = true` 才執行 `x = 100`，會導致 Thread 1 提早突破迴圈。

**【修復後：加上記憶體屏障】**
```cpp
// Thread 1
while (!flag)
    memory_barrier(); // 防護 1：確保確實讀取到最新的 flag 狀態後，才允許執行下一行的 print
print x;

// Thread 2
x = 100;
memory_barrier();     // 防護 2：設立柵欄！
flag = true;
```
**【修復原理解析】：**
在 Thread 2 中，`memory_barrier()` 卡在兩行程式碼中間。這道柵欄強迫 CPU：「你必須先把 `x = 100` 徹底寫入實體記憶體並讓大家都看見，才能去執行 `flag = true`」。這徹底粉碎了 CPU 擅自調換這兩行指令的可能性，確保了 Thread 1 絕對會印出正確的 `100`。

### 四、 修復 Peterson's Solution
同理，前一節提到 Peterson's Solution 失敗的原因，在於進入區間 (Entry section) 的前兩行變數賦值可能被硬體對調。只要加上屏障即可完美修復：
```cpp
flag[i] = true;
memory_barrier();  // 加入硬體屏障，強迫 flag 寫入後，才能執行下一行
turn = j;
```

### 五、 架構師的實務警語
課本在最後給出了一個重要的實務觀念：**Memory barriers 是極度低階 (very low-level) 的硬體操作**。
一般的軟體工程師（App 開發者）在寫 C++ 或 Java 時，幾乎不會直接呼叫這個指令。這通常是**作業系統核心開發者 (Kernel developers) 為了打造更安全的同步工具（如 Mutex Lock 或 Semaphore）時**，才會在底層原始碼中使用的特殊武器。
## 6.4.2 Hardware Instructions
### 背景原因
純軟體解法之所以脆弱，是因為高階語言的一行程式碼（如 `count++`）在底層會被拆成多個微指令，而排程器隨時可能在這些微指令之間插隊。為了守護臨界區間 (Critical Section)，硬體工程師直接把「讀取 $\rightarrow$ 判斷 $\rightarrow$ 寫入」這整套動作，銲死成單一個不可分割的機器語言指令。

課本原句精準定義了這個概念：
> "...allow us either to test and modify the content of a word or to swap the contents of two words atomically—that is, as one uninterruptible unit."
（允許我們以原子化的方式——也就是作為一個不可中斷的單元——來測試並修改一個字組的內容，或是交換兩個字組的內容。）

即使有兩顆實體核心「同時」發出這個硬體指令，記憶體控制器也會強迫它們「乖乖排隊」，依序執行 (executed sequentially in some arbitrary order)。

### 一、 第一項武器：`test_and_set()` 指令
這是一個極度霸道的指令。我們用 C 語言的邏輯來模擬這顆硬體晶片的內部運作 (Figure 6.5)：
```c
boolean test_and_set(boolean *target) {
    boolean rv = *target; // 1. 先偷偷記下目標原本的值
    *target = true;       // 2. 無論如何，霸道地把目標設為 true
    return rv;            // 3. 回傳目標「原本」的值
}
```
【實戰應用：Figure 6.6 互斥鎖實作】
* 宣告一個全域的布林變數 `lock`，初始化為 `false`。
```c
do {
    // 進入區間 (Entry Section)
    while (test_and_set(&lock))
        ; /* do nothing (忙碌等待) */
        
    /* critical section (臨界區間) */
    
    // 離開區間 (Exit Section)
    lock = false;
    
    /* remainder section */
} while (true);
```
**推演邏輯：**
* **第一個抵達的行程：** 看到 `lock` 是 `false`。它呼叫 `test_and_set`，這顆硬體晶片會回傳 `false` 給它，但同時瞬間把 `lock` 變成 `true`。因為回傳 `false`，它成功打破 `while` 迴圈，闖入臨界區間！
* **第二個抵達的行程：** 此時 `lock` 已經是 `true` 了。它呼叫 `test_and_set`，晶片回傳 `true`，並且再次把 `lock` 設為 `true`。因為拿到 `true`，它只能乖乖卡在 `while` 迴圈裡發呆。

### 二、 第二項武器：`compare_and_swap()` (CAS) 指令
相較於 `test_and_set` 的無腦強迫寫入，CAS 顯得更聰明且有條件。它操作三個運算元：`value` (記憶體目標)、`expected` (我期待看到的值)、`new_value` (我想寫入的新值)。
```cpp
int compare_and_swap(int *value, int expected, int new_value) {
    int temp = *value;      // 1. 記下記憶體現在真實的值
    
    if (*value == expected) // 2. 條件判斷：如果記憶體的值跟我期待的一樣！
        *value = new_value; // 3. 才允許寫入新值
        
    return temp;            // 4. 無論有無寫入，都回傳最初看到的真實值
}
```
**【實戰應用：Figure 6.8 互斥鎖實作】**
宣告一個全域變數 `int lock = 0;`。
```cpp
do {
    // 進入區間：我期待 lock 是 0，如果是，請幫我改成 1。
    while (compare_and_swap(&lock, 0, 1) != 0)
        ; /* do nothing */
        
    /* critical section */
    
    lock = 0; // 離開時把鎖解開
    
    /* remainder section */
} while (true);
```
* **硬體實務對接：** 課本在 6.4.2 結尾補充，在 Intel x86 架構中，這個強大的 CAS 指令對應的底層組合語言是 `cmpxchg`。為確保原子性，還會加上 `lock` 前綴字來鎖住記憶體匯流排：`lock cmpxchg <destination operand>, <source operand>`。

### 三、 終極型態：滿足「有限等待 (Bounded Waiting)」的 CAS 演算法
上面寫的 Figure 6.6 和 Figure 6.8 雖然完美達成了「互斥 (Mutual Exclusion)」，但課本無情地指出：
> "Although this algorithm satisfies the mutual-exclusion requirement, it does not satisfy the bounded-waiting requirement."
（雖然此演算法滿足了互斥需求，但它並未滿足有限等待需求。）

想像一下，如果有 10 個行程在搶鎖，當鎖解開的瞬間，這 10 個人是「各憑硬體運氣」去搶那顆 CPU 晶片的 CAS 指令。運氣最差的行程可能永遠搶不到，導致飢餓 (Starvation)。

**【完美解法：Figure 6.9 深度拆解】**
為了解決這個問題，工程師加上了一個 `boolean waiting[n]` 陣列，用來記錄大家排隊的狀態。
```cpp
// 全域變數：waiting 陣列初始化為 false，lock 初始化為 0
int lock = 0;
boolean waiting[] = false;

while (true) {
    /* --- 進入區間 (Entry Section) --- */
    waiting[i] = true; // 1. 舉手宣告：我 P_i 在排隊等鎖了！
    key = 1;           // 2. key 代表鎖的狀態 (1 代表鎖著)
    
    // 3. 雙重條件卡關：只要我還在等待名單內，且還沒拿到鎖，就卡住
    while (waiting[i] && key == 1)
        key = compare_and_swap(&lock, 0, 1); // 瘋狂嘗試搶鎖
        
    waiting[i] = false; // 4. 成功闖關！把等待標記取消
    
    /* --- critical section (臨界區間) --- */
    
    /* --- 離開區間 (Exit Section) - 演算法的魔術核心 --- */
    j = (i + 1) % n; // 1. 從我的下一個鄰居開始掃描 (Cyclic ordering)
    
    // 2. 繞著環狀陣列找，直到找到「第一個正在排隊」的人，或是繞回我自己
    while ((j != i) && !waiting[j])
        j = (j + 1) % n;
        
    // 3. 決定如何交接權力
    if (j == i) 
        lock = 0; // 情況 A：繞了一圈發現根本沒人在排隊，直接把全域鎖解開
    else
        waiting[j] = false; // 情況 B：找到了苦苦排隊的 P_j！
                            // 注意！這裡「沒有」把 lock 設為 0！
                            // 而是直接透過修改 P_j 的 waiting 變數，
                            // 強行從內部打破 P_j 的 while 迴圈，把權力「私下傳接」給他！
                            
    /* --- remainder section --- */
}
```

---
## 6.4.3 Atomic Variables
### 背景原因
前一節我們看到了底層硬體指令 CAS 的強大與複雜。這一節，課本將視角拉回軟體工程師的角度：我們該如何優雅地使用這些硬體支援？
### 一、 為什麼需要原子變數？ (The Purpose of Atomic Variables)
在實務開發上，我們很少直接拿 compare_and_swap() 來刻互斥鎖 (Mutual Exclusion)。系統通常會提供一套封裝好的原子資料型別 (Atomic data types)（在 C++ 中通常是 `std::atomic<int>`，或課本用的 `atomic_int`）以及對應的操作函式。

回顧 6.1 節，一般高階語言的 `count++` 在底層會被拆成「讀取 $\rightarrow$ 暫存器加 1 $\rightarrow$ 寫回」三個步驟，隨時會被中斷。而原子變數的作用，就是確保針對這個單一變數的更新動作，在物理上是「一氣呵成、絕對互斥」的。

### 二、 程式碼深度拆解：CAS 的高階封裝
課本提供了一段極度經典的程式碼，展示了底層函式庫是如何利用 CAS 指令來實作一個無鎖 (Lock-free) 的 `increment()` 加法運算。
```cpp=
void increment(atomic_int *v) 
{
    int temp;
    
    do {
        // 1. 先記錄下變數 v 目前的真實數值
        temp = *v;
        
        // 2. 核心邏輯：
        // 呼叫 CAS 檢查 v 目前的值是否依然是 temp。
        // 如果是，代表這段期間沒人動過它，請大膽地把它改成 temp + 1。
        // 如果 CAS 回傳的值不是 temp，代表這瞬間有其他執行緒偷改了 v！
        // 此時 while 條件成立，迴圈重跑，重新讀取新的 v 值再試一次。
    } while (temp != compare_and_swap(v, temp, temp+1));
}
```

【架構師觀點：樂觀鎖 (Optimistic Locking)】
這種 `do-while` 搭配 CAS 的寫法，在軟體工程被稱為「樂觀鎖」。它不主動把別人擋在門外，而是「樂觀地」假設沒有人會來跟我搶。我先算好答案 (`temp + 1`)，要寫入的最後一刻再檢查有沒有人偷改 (`compare_and_swap`)。如果被改了，摸摸鼻子認栽，重算一次就好。

### 三、 致命的侷限性：為何原子變數無法解決所有問題？
這是本節最重要的觀念反轉。雖然原子變數很棒，但課本嚴厲警告：**它並不能在所有情況下解決競態條件 (they do not entirely solve race conditions in all circumstances)**。

**【情境推演：被攻破的 Bounded-Buffer (有限緩衝區)】**
假設我們把 6.1 節的 `count` 宣告成原子變數 (`atomic_int`)，確保 `count++` 和 `count--` 絕對不會算錯。但災難依然會發生：
1. **初始狀態：** 緩衝區目前是空的 (`count == 0`)。有兩個消費者 (Consumers) 正在執行 `while (count == 0)` 的迴圈苦苦等待。
2. **生產者介入：** 生產者放進了 1 個物品，並執行原子加法，讓 `count` 變成 1。
3. **邏輯崩潰：** 因為 `count` 變成 1 了，這兩個消費者會「同時」打破她們的 `while (count == 0)` 迴圈！
4. **結局：** 這兩個消費者都興沖沖地往下執行程式碼，準備從緩衝區把物品拿出來。但緩衝區裡面明明只有 1 個物品，卻有兩個人要拿，系統直接引發不可預期的崩潰 (proceed to consume, even though the value of count was only set to 1)。

### 四、 結論與下一章伏筆
原子變數通常只被限制用在「單一共享資料的單純更新」上，例如計數器 (counters) 或是序號產生器 (sequence generators)。面對像生產者－消費者這種牽涉到「條件判斷 $\rightarrow$ 執行後續動作」的複雜邏輯，我們需要更強大、能夠鎖住一整段程式碼區塊的通用工具。這直接帶出了下一節將探討的重頭戲：**Mutex Locks (互斥鎖)** 與 **Semaphores (號誌)**。

---

# 6.5 Mutex Locks
### 背景說明
前一節 (6.4) 介紹了底層硬體提供的同步武器。但實務上，應用程式設計師 (Application programmers) 極少直接去呼叫硬體指令。作業系統設計師肩負起橋樑的責任，利用這些硬體指令，打造出高階的軟體同步 API。本節介紹的就是最經典、最基礎的 Mutex Lock。

*(註：Mutex 這個單字其實就是 Mutual exclusion (互斥) 的縮寫。)

### 一、 互斥鎖的運作邏輯與程式碼實作
Mutex Lock 的內部維護著一個布林變數 `available`，用來標示這把鎖目前是否可用。
**【標準使用結構 (Figure 6.10)】**
```cpp
while (true) {
    acquire();  // 申請鎖：如果鎖被別人拿走，就會卡在這裡阻塞 (Blocked)
    
    /* critical section (臨界區間) */
    
    release();  // 釋放鎖：離開時交出控制權，讓別人可以進去
    
    /* remainder section (剩餘區間) */
}
```
**【底層實作剖析】**
```cpp
// 取得鎖的函式
acquire() {
    while (!available)
        ; /* busy wait (忙碌等待) */
    available = false; // 成功突破迴圈，把鎖拿走
}

// 釋放鎖的函式
release() {
    available = true; // 把鎖還回去
}
```
* 這段 `acquire()` 與 `release()` 的程式碼，絕對不能被中斷！對這兩個函式的呼叫必須被當作「原子操作 (Atomically)」來執行。在現代作業系統中，`acquire()` 底層通常就是直接呼叫上一節學到的`compare_and_swap()` (CAS) 硬體指令來確保其原子性。

### 二、 致命缺點與意外優勢：自旋鎖 (Spinlock)
1. **致命缺點：Busy Waiting (忙碌等待)**
觀察 `acquire()` 的程式碼，如果一個行程拿不到鎖，它會陷入 `while (!available)` 的無窮迴圈中不斷打轉。
* **資源浪費：** 在單一 CPU 核心被多個行程共享的多重程式設計系統 (Multiprogramming system) 中，這是一個嚴重的問題。這個在迴圈裡空轉的行程，正在白白浪費寶貴的 CPU 運算週期 (CPU cycles)，而這些週期原本是可以拿去給其他程式做正事的。
* 這種因為拿不到鎖而「自旋 (Spins)」的互斥鎖，在作業系統領域被賦予了一個專有名詞：**Spinlock (自旋鎖)**。

2. **意外優勢：為何多核心系統超愛 Spinlock？**
既然自旋鎖會浪費 CPU，為什麼它沒有被淘汰？
* **免除切換成本：** 當一個行程拿不到鎖時，如果不想讓它空轉，唯一的作法就是把它「強制休眠 (Sleep)」，這牽涉到極度昂貴的 Context Switch (內容交換)。
* **多核心情境 (Multicore Systems)：** 假設行程 A 在 Core 0 拿到了鎖，行程 B 在 Core 1 苦等。如果行程 A 只需要極短的時間就能做完事情，那麼讓行程 B 在 Core 1 上「自旋空轉」個幾微秒，可能比大費周章把它休眠再喚醒還要快得多。

### 三、 講義補充解析 (藍色方塊重點)
課本透過兩個藍色資訊方塊，補充了系統工程師在實務上評估鎖的兩大進階觀念：
**【補充一：Lock Contention (鎖競爭)】**
* **Uncontended (無競爭狀態)：** 當行程想拿鎖時，鎖剛好是 `available` 的，順利拿走。
* **Contended (競爭狀態)：** 程想拿鎖，但鎖被別人拿走了，導致行程被阻塞 (Blocks)。
    * **High contention (高競爭)：** 一大群執行緒同時在搶同一把鎖。這會嚴重拖垮並行應用程式的整體效能 (Decrease overall performance)。
    * **Low contention (低競爭)：** 只有少數幾個執行緒在搶。
**【補充二：什麼才叫做「短暫時間 (Short Duration)」？】**
前面提到，只要臨界區間處於「短暫時間」，就應該使用 Spinlock。但「短暫」在物理時間上到底是多少？
* **成本計算：** 將一個行程休眠並不是免費的。它需要付出 兩次 Context Switches 的代價：
    1. 第一次切換：把拿不到鎖的行程從 CPU 拔下來，丟到等待佇列 (Waiting state) 休眠。
    2. 第二次切換：當鎖解開時，把該行程從等待佇列喚醒，重新放回 CPU 執行。
* **黃金守則 (General Rule)：** 如果該把鎖預計被持有的時間，小於兩次 Context Switch 的總耗時，那就毫無懸念地使用 Spinlock (自旋鎖)。反之，如果預計會鎖很久，就應該讓行程休眠，避免浪費 CPU。

---
# 同步機制 (Synchronization) 在最底層的根本目的
### 1. 破解高階語言的「原子性幻覺」
在撰寫 C++ 或其他高階語言時，像 `count++` 這樣的語法看起來只是一個單一的動作。但對底層 CPU 而言，它必須被拆分為三個獨立的物理步驟：
1. **Load (載入)：** 從主記憶體 (RAM) 將數值拉進 CPU 暫存器。
2. **Add (運算)：** 在 CPU 內部將數值加一。
3. **Store (寫回)：** 將暫存器的新數值寫回同一塊主記憶體。

**同步機制的首要目的**，就是承認這個「拆解過程」極度脆弱。當系統進行 Context Switch 或是多顆實體核心同時運作時，必須有底層機制來保證這三個步驟能「**一氣呵成**」地執行完畢，中間絕對不允許其他實體插隊去摸那塊記憶體。

### 2. 建立排他性的「物理防護罩」
同步機制 (Synchronization) 在最底層的根本目的，正是為了解決「多個執行實體（行程或執行緒）在同一個時間點，試圖讀寫同一塊記憶體區塊」所引發的資料毀損問題。誠如提問中所指出的「同一個記憶體 block」，當多個執行緒 (Threads) 共用同一個全域變數或資料結構時，該記憶體區塊就成了兵家必爭之地。
* 同步機制的物理實現（如 Memory Barriers、硬體 CAS 指令），其目的就是要在這塊記憶體周圍建立起名為 **臨界區間** (Critical Section) 的防護罩。
* 它強制規定，不管外面排了多少個嗷嗷待哺的執行緒，同一個物理時間點，防護罩內永遠**只能有一個實體**在對該記憶體進行操作。

### 3. 從「互斥 (Mutual Exclusion)」延伸至「協調 (Coordination)」
除了防範資料被同時踩踏之外，同步機制在更高階的應用上，還包含著順序的協調。
* 有時候，問題不在於「不能同時寫」，而在於「必須等 A 寫完，B 才能去讀」。
* 例如在生產者與消費者問題中，消費者必須「等待」生產者把資料放進記憶體 block 之後才能動作。同步工具（如 Condition Variables 或 Semaphores）的目的，就是讓執行緒之間能夠互相傳遞信號：「資料準備好了，換上場吧！」

**總結來說：** 如果每個程式都只在自己專屬的、被隔離的記憶體沙箱裡玩耍，這個世界上根本不需要同步機制。正是因為要**「共享同一塊記憶體資源以達成協作」**，同步機制才成為了作業系統與多核心程式設計中最核心、也最困難的守門員**。

---

# 6.6 Semaphores
前一節的互斥鎖 (Mutex) 就像是一把只有唯一一把鑰匙的洗手間門鎖。但如果我們今天面對的是一個「有 5 個停車位的停車場」，Mutex 就顯得不夠用了。這就是號誌 (Semaphore) 誕生的契機。
## 6.6.1
### 號誌的定義與歷史由來
號誌的概念是由荷蘭籍計算機科學大師 Edsger Dijkstra 所提出。
* 號誌 `S` 是一個整數變數。
* Dijkstra 原本將 `wait()` 操作命名為 P (源自荷蘭語 proberen，意思是 "to test" 測試)。
* 將 `signal()` 操作命名為 V (源自荷蘭語 verhogen，意思是 "to increment" 增加)。
**【程式碼定義與原子性要求】**
```cpp
wait(S) {
    while (S <= 0)
        ; // busy wait (忙碌等待：如果資源沒了，就在這裡卡住)
    S--;  // 拿走一個資源，數量減 1
}

signal(S) {
    S++;  // 釋放一個資源，數量加 1
}
```
**架構師鐵則 (Atomicity)：**
課本強烈警告，對號誌整數值 `S` 的所有修改，絕對必須以原子方式執行 (must be executed atomically)！
1. 當一個行程正在修改號誌時，絕不允許其他行程同時修改它。
2. 在 `wait(S)` 中，從「測試 `S <= 0`」到「執行 `S--`」，這整個連貫動作必須是不被中斷的單一單元 (executed without interruption)。

### Semaphore Usage
作業系統通常將號誌嚴格區分為兩種應用場景：
1. **Binary Semaphores (二元號誌)**
    * 其整數值被嚴格限制只能是 0 和 1。
    * 它的行為與 Mutex Lock 幾乎一模一樣。事實上，在某些沒有原生提供 Mutex 的系統中，工程師就是直接拿二元號誌來當作互斥鎖使用 (providing mutual exclusion)。
2. **Counting Semaphores (計數號誌)**
    * 它的值不受限制 (unrestricted domain)。
    * **核心用途：** 用來控管擁有「有限數量實體 (finite number of instances)」的共享資源（例如：伺服器最多只允許 100 個連線）。
    * **運作邏輯：**
        1. **初始化：** 將號誌初始化為該資源的可用總數。
        2. **申請資源：** 行程呼叫 `wait()`，號誌數量減一。
        3. **釋放資源：** 行程呼叫 `signal()`，號誌數量加一。
        4. **資源耗盡：** 當號誌值歸零 (`S == 0`) 時，代表所有資源都被佔用了。此時任何呼叫 `wait()` 的行程都會被阻塞 (block)，直到有人釋放資源讓 `S` 大於 0。

### 執行順序的「協調 (Coordination)」
課本在 6.6.1 節後半段，完美展示了號誌如何解決跨行程的「執行順序控制」。這正是你稍早提到的「必須等 A 做完，B 才能做」的情境。
**【情境設定】**
有兩個並發執行的行程：$P_1$ 擁有程式碼語句 $S_1$，$P_2$ 擁有程式碼語句 $S_2$。
任務目標：我們必須強制要求 $S_2$ 只能在 $S_1$ 徹底執行完畢之後，才能開始執行 (require that $S_2$ be executed only after $S_1$ has completed)。

**【解法實作】**
宣告一個名為 `synch`的共享號誌，並將其初始化為 `0`。
1. 在 $P_1$ 的程式碼中插入：
```cpp
S1;             // P1 先執行它該做的事
signal(synch);  // 執行完後，發送信號 (synch 變成 1)
```
2. 在 $P_2$ 的程式碼中插入：
```cpp
wait(synch);    // P2 執行前，先卡在這裡等。因為 synch 初始是 0，P2 會被阻塞！
S2;             // 只有收到信號突破 wait 後，才能執行 S2
```
**【底層邏輯推演】**
因為 `synch` 初始值是 0，如果 $P_2$ 跑得比較快，它一遇到 wait(synch) 就會立刻卡住進入等待狀態。它必須一路等到 $P_1$ 執行完 $S_1$、並發出 `signal(synch)` 把號誌變成 `1`之後，$P_2$ 才拿得到通行證繼續執行 $S_2$。這完美達成了跨行程的非同步協調！

---
## 6.6.2
前一節定義了號誌 (Semaphore) 的基礎行為，但在最原始的定義中，`wait()` 操作內部依然包含著 `while (S <= 0)`; 這種會浪費 CPU 的忙碌等待 (Busy Waiting) 程式碼。

本節課本展現了作業系統底層如何透過 C 語言結構體與 Context Switch 機制，來徹底改良這個效能缺陷。

### 一、號誌資料結構的全面升級
為了讓拿不到資源的行程 (Process) 可以去睡覺排隊，號誌必須具備維護「排隊名單」的能力。因此，課本將號誌重新定義為以下結構：
```cpp
typedef struct {
    int value;               // 資源數量
    struct process *list;    // 鏈結串列，指向所有正在等待此號誌的 PCB (行程控制區塊)
} semaphore;
```
### 二、 wait() 與 signal() 的程式碼重構
配合新的資料結構，核心操作被重新改寫。這裡調用了作業系統底層的兩個系統呼叫 (System Calls)：`sleep()` (讓當前行程休眠) 與 `wakeup(P)` (將特定行程喚醒)。

**【`wait()` 申請資源操作】**
```cpp
wait(semaphore *S) {
    S->value--;              // 1. 先無條件扣除資源
    if (S->value < 0) {      // 2. 如果扣完變負數，代表資源早就不夠了
        add this process to S->list; // 將自己加入等待佇列
        sleep();                     // 強制自己休眠 (交出 CPU)
    }
}
```

**【signal() 釋放資源操作】**
```cpp
signal(semaphore *S) {
    S->value++;              // 1. 無條件歸還資源
    if (S->value <= 0) {     // 2. 🚨 關鍵判斷：如果加完之後，數值依然小於等於 0
        remove a process P from S->list; // 從佇列中挑選一個幸運兒
        wakeup(P);                       // 將其喚醒，放入 Ready Queue 準備執行
    }
}
```
### 三、核心考點：負數的物理意義與 signal() 邏輯
這段程式碼最容易讓人困惑的地方在於 `signal()` 中的判斷式：為什麼還資源之後，數值小於等於 0 反而代表有人在等？
1. **先扣再測 (Decrement then test)：**
    仔細觀察這版的 `wait()`，它是先執行 `S->value--`，才去檢查是否小於零。這會導致一個現象：如果資源本來是 `0`，又來了 3 個行程呼叫 `wait()`，這個數值會一路被扣到 `-3`。
2. **絕對值 = 排隊人數：**
    正如課本所述，如果數值是負的，它的絕對值 (Magnitude) 就精準代表了「等待名單 (`S->list`) 中排隊的行程總數」。
3. **推演 `signal()` 的觸發條件：**
    假設目前 `S->value` 是 -3 (代表 3 人在排隊)。當有人呼叫 `signal()` 歸還資源時，數值會加 1 變成 -2。因為 `-2 <= 0` 條件成立，系統知道「哦！雖然有資源還回來了，但外面還欠著兩個人呢！」，於是立刻去佇列拔出一個行程並 `wakeup()` 它。直到數值回正為 1，才代表外面已經沒有任何行程在苦苦等候了。
    
### 四、系統架構師的防護網：維持原子性 (Atomicity)
雖然解決了應用程式層級的忙碌等待，但這又引發了一個遞迴的底層難題：`wait()` 和 `signal()` 這兩段程式碼本身，也是在操作共享變數 (`S->value` 和 `S->list`)！
如果兩個行程同時呼叫 `wait()`，導致一起修改佇列，系統依然會崩潰。因此，必須保證這兩個操作的「原子性 (Executed atomically)」。
* **單核心環境 (Single-processor environment)：**
    最暴力的解法，直接在執行 `wait()` 和 `signal()` 期間關閉中斷 (Inhibiting interrupts)。只要沒有中斷，排程器就不會介入，指令就不會被交錯執行。
* **多核心環境 (Multicore environment)：**
    在多核心架構下，關閉所有核心的中斷是一項艱鉅且會嚴重拖垮效能的任務 (Diminish performance)。因此，SMP 系統必須仰賴 6.4 節學過的硬體指令，例如 `compare_and_swap()` 或 `Spinlocks` (自旋鎖)，來為 `wait()` 和 `signal()` 的程式碼加上底層護盾。
    
### 五、殘酷的現實：轉移而非消滅
課本在最後一段給出了一個極具深度的工程反思。
使用這套實作方法，系統其實並沒有 100% 消滅忙碌等待 (Not completely eliminated busy waiting)。

真相是，工程師只是把忙碌等待從「應用程式的臨界區間」轉移到了「保護 `wait()` 與 `signal()` 運作的底層自旋鎖」上。
* **為什麼這樣做划算？**
    應用程式的臨界區間可能非常冗長 (Minutes or even hours)，如果在那邊自旋空轉，系統效能會崩潰。而 `wait()` 和 `signal()` 裡面的程式碼非常短，通常不超過 10 個硬體指令 (About ten instructions)。用自旋鎖保護這區區 10 個指令，發生衝突的機率極低，且空轉時間極短，是現代作業系統在效能與複雜度之間達成的完美平衡。
    









## 號誌等待佇列實作與排程策略深度統整
在作業系統中，號誌為了消除應用程式層級的忙碌等待 (Busy Waiting)，引入了「休眠與喚醒」機制。要達成此機制，系統必須具備管理排隊名單的能力，其底層實作與衍生出的排程難題可拆解為以下四大核心：
### 一、 等待佇列的實體記憶體實作 (資料結構)
為了極致節省記憶體並提升操作效率，作業系統並不會為號誌宣告龐大的獨立陣列來儲存等待名單。
1. **PCB 鏈結串列：** 系統工程師直接利用每個行程原有的**行程控制區塊** (Process Control Block, PCB) 作為資料節點。在 PCB 中新增一個指標欄位 (Link field)，當行程呼叫 `wait()` 被阻塞時，系統便透過這個指標將這些 PCB 像肉粽一樣串接成一個鏈結串列 (Linked List)。

2. **號誌結構體：** 號誌本身在記憶體中只是一個極度輕量的結構體，包含一個整數變數 (value)，以及一個**指向該 PCB 串列頭部的指標 (list pointer)**。

### 二、 策略解耦與 FIFO 的防護作用
號誌的原子性操作 (`wait` 扣除資源、`signal` 歸還資源) 與其背後的「排隊策略 (Queuing strategy)」在軟體工程上是**完全解耦 (Decoupled)** 的。這意味著系統可以使用任何排隊方式，都不會影響號誌「互斥」的基本功能。

然而，若要完美滿足臨界區間的**有限等待 (Bounded Waiting)** 鐵律：
* **最佳選擇是 FIFO (先進先出)：** 作業系統會在號誌中維護佇列的頭部 (Head) 與尾部 (Tail) 指標。採用 FIFO 可以確保最先被阻塞的行程，絕對會第一個被 `signal()` 喚醒。這從物理結構上保證了任何行程「被插隊的次數為 0」，完美達成有限等待的要求。

### 三、 優先權佇列 (Priority Queue) 引發的飢餓災難
若系統為了區分任務輕重，將等待名單的排隊策略從 FIFO 改為優先權佇列，將會面臨嚴重的系統性災難：
1. **災難推演：** 當一個低優先權行程 ($P_{low}$) 正在佇列中苦等資源時，若系統源源不絕地湧入高優先權行程 ($P_{high}$)，這些 $P_{high}$ 會不斷插隊到 $P_{low}$ 的前面。
2. **打破鐵律 (Starvation)：** 每當資源被釋放時，系統永遠只會喚醒最前面的 $P_{high}$。這將導致 $P_{low}$ 被無限期推遲，永遠拿不到資源，形成飢餓 (Starvation)，徹底打破了有限等待的底線。

### 四、 系統架構師的兩難取捨與最終解法
面對「公平性 (FIFO)」與「急迫性 (Priority)」的衝突，系統設計會根據應用場景走向兩條不同的道路：
1. **硬即時系統的刻意妥協 (Hard Real-Time Systems)：**
    在飛航控制或自動駕駛等攸關生死的系統中，「不公平才是安全的」。系統工程師寧可讓低優先權的任務（如背景日誌記錄）徹底餓死，也必須保證高優先權任務（如防撞煞車）能隨時插隊搶到號誌資源。在此情境下，放棄有限等待是刻意為之的架構選擇。
2. **通用作業系統的完美解法：老化機制 (Aging)：**
   在 Windows 或 Linux 等通用系統中，既需要優先權來加速關鍵互動任務，又不能讓背景程式死機。唯一的解法是引入老化機制 (Aging)。
   * **運作邏輯：** 當 $P_{low}$ 在號誌佇列中等待時，系統會隨時間動態調升其優先權。等得越久，優先權越高。
   * **破局點：** 最終 $P_{low}$ 的優先權會膨脹到足以擊敗新進來的 $P_{high}$，順利奪取資源。這在保有優先權調度彈性的同時，勉強守住了有限等待的底線，消滅了永久飢餓的可能性。

---
# 6.7 Monitors
## 背景
在先前的章節中，系統工程師已經開發出了互斥鎖與號誌來解決臨界區間問題。然而，本節開宗明義地指出了一項殘酷的軟體工程現實：**雖然號誌提供方便的機制，但不正確使用會導致難以偵測的時序錯誤 (timing errors)**。

因為這些錯誤通常只在「特定的執行交錯順序」下才會發生，且這些順序並非總是會出現，導致除錯極度困難。只要有一個行程 (Even a single process) 不守規矩，整個系統的互斥防線就會全面崩潰。

課本列舉了三種最經典的人為災難（假設所有的行程都共用一個初始化為 1 的二元號誌 `mutex`）：
1. **順序顛倒 (Interchanging Operations)：**
    開發者不小心把程式碼寫成了
    ```cpp
    signal(mutex); // 執行 signal(mutex) 離開（把門打開）。
    ...
    critical section // 執行臨界區間 (Critical Section) 操作共享資料。
    ...
    wait(mutex); // 執行 wait(mutex) 申請進入（把門上鎖）。
    ```
    * **後果：** 門一開始就被打開了，導致多個行程可以同時闖入臨界區間，嚴重違反互斥 (Mutual-exclusion) 需求。

2. **重複等待 (Replacing signal with wait)：**
    開發者手誤，寫成了 `wait(mutex); ... critical section ... wait(mutex);`。
    * **後果：** 當行程執行完臨界區間準備離開時，它沒有把鎖還回去，反而又試圖拿一次鎖。這會導致該行程永遠阻塞 (Permanently block) 在第二個 `wait()` 上，造成死結。

3. **完全遺漏 (Omitting Operations)：**
    忘記寫 `wait(mutex)`、忘記寫 `signal(mutex)`，或者兩者都忘了寫。
    * **後果：** 不是違反互斥，就是導致其他排隊的行程永久阻塞。

**【架構師的解法】**
為了解決這種因為「手殘」造成的系統崩潰，電腦科學家決定將同步機制直接內建到高階語言的語法構造 (High-level language constructs) 中。其中最基礎且強大的構造，就是 **Monitor type** (管程型別)。

## 6.7.1 Monitor Usage
### 一、什麼是管程 (Monitor Type)？
為了避免號誌 (Semaphore) 與互斥鎖 (Mutex) 因為人為失誤導致的時序錯誤 (Timing errors) 與死結，作業系統設計者與程式語言開發者共同提出了一種高階的同步構造 (High-level synchronization construct)：Monitor type (管程型別)。

課本首先給出定義：管程是一種 抽象資料型別 (Abstract Data Type, ADT)。它會把「共享資料」以及「操作這些資料的一組函數」封裝 (encapsulates) 在一起。

**【Figure 6.11 管程語法結構】**
```cpp
monitor monitor_name 
{
    /* shared variable declarations (宣告共享變數) */
    
    function P1 ( . . . ) {
        . . .
    }
    
    function P2 ( . . . ) {
        . . .
    }
    
    // ... 其他函數 Pn
    
    initialization_code ( . . . ) {
        . . .
    }
}
```

**【語法重點解釋】**
1. **封裝性限制：** 定義在管程內的函數（如 P1, P2），只能存取管程內部的區域變數。相對地，管程內部的局部變數，絕對不允許被外部的其他函數存取。
2. **自動互斥 (Mutual Exclusion)：** 這是管程最核心的價值！管程構造確保「同一時間只能有一個行程在管程內活躍 (active)」。 程式設計師「不需要」自己寫任何同步限制的程式碼（不用再痛苦地寫 `wait` 和 `signal` 來擋門），編譯器與系統會在底層自動幫你處理好。

### 二、 管程的物理結構透視 (對應 Figure 6.12)
![image](https://hackmd.io/_uploads/rJ00Idi0bx.png)
* **橢圓形邊界：** 這代表管程的互斥結界。
* **Shared data (上方區域)：** 被保護的共享變數。
* **Operations (中間柱狀體)：** 管程內部提供的函數（也就是剛才程式碼裡的 P1, P2）。
* **Entry queue (右上角的藍灰色方塊陣列)：** 這非常關鍵！因為管程嚴格規定「裡面只能有一個人」，所以當管程內已經有行程在活躍時，其他試圖呼叫這些函數的行程，全部都會被擋在結界外面，並進入這個 **進入佇列 (Entry queue)** 乖乖排隊。

### 三、 條件變數的引入 (Condition Variables)
雖然單純的管程解決了互斥，但在實務上不夠強大。
假設一個消費者行程進入了管程，準備讀取緩衝區，卻發現「緩衝區是空的」。此時它不能繼續執行，但它又佔著管程唯一的活躍名額，導致生產者也進不來，系統就死結了。

為了讓行程可以「暫時退位」，我們需要定義額外的同步機制：條件變數 (Condition variables)。
開發者可以宣告：`condition x, y;`

條件變數只能透過兩種操作來呼叫：

1. `x.wait()`：呼叫這個操作的行程，會立刻被強制休眠 (suspended)，並交出管程的活躍權。
2. `x.signal()`：精準喚醒「剛好一個 (exactly one)」因為條件 x 而休眠的行程。

**【絕對重點：管程 Signal vs. 號誌 Signal 的差異】**
課本特別強調，如果你呼叫 `x.signal()` 時，剛好沒有任何行程在休眠，那麼這個操作會 毫無反應 (has no effect)，管程的狀態就跟什麼事都沒發生過一樣。這跟「號誌 (Semaphore)」永遠會把數值加 1 存起來的行為截然不同！

### 四、 完全體管程的運作模型 (對應 Figure 6.13)
![image](https://hackmd.io/_uploads/B1aHhOoRZl.png)
加入了條件變數後，我們來看 Figure 6.13 (Monitor with condition variables)。這張圖比上一張多了一個核心區域：
* **Queues associated with x, y conditions (左上角的內部佇列)：**
    這就是管程的「內部等候區」。當一個行程在活躍區呼叫了 `x.wait()`，它就會被移出活躍區，並排入這個專屬於 x 的小佇列中休眠。此時，管程中央的活躍區段空了出來，外面 `Entry queue` 排隊的行程就可以進來了。
    
### 五、 喚醒的物理衝突 (The Signaling Dilemma)
現在面臨一個兩難：
假設行程 P 正在管程內活躍執行。P 呼叫了 `x.signal()`，把在內部等候區休眠的行程 Q 給叫醒了。
問題來了：管程規定同一時間只能有一個人活躍，**現在 P 和 Q 都醒著**，誰能留在管程裡？

課本提出兩種可能性：
1. **Signal and wait (發信者退位)：**
    行程 P 喚醒 Q 之後，P 選擇自己去休眠（或者等待另一個條件），把控制權讓給剛醒來的 Q。
2. **Signal and continue (發信者繼續)：**
    行程 P 喚醒 Q 之後，P 繼續把剩下的程式碼執行完。剛被喚醒的 Q 必須繼續等待，直到 P 徹底離開管程。
* **兩種流派的優缺點分析：**
    課本提到，既然 P 本來就在執行，讓它繼續執行 (Signal and continue) 似乎比較合理；但反過來說，如果讓 P 繼續執行，等到 Q 終於能上場時，Q 當初在等待的「邏輯條件」可能已經被 P 後續的程式碼改變了。目前許多程式語言（如 Java, C#）皆有引入這類管程與條件變數的概念來支援併發。
    
### 六、為何「封裝」與「互斥」缺一不可？
*「內部函數才能碰共享變數，外部不能伸手」這件事，跟「同一時間只允許一個行程在管程內活躍」，這兩者有什麼關係？*

1. **第一層防護：封裝 (Encapsulation) ——「把窗戶封死，只留正門」**
    * 共享變數是「金庫裡的錢」，內部函數是「銀行櫃檯」，外部行程是「客人」。
    * **封裝的規定是：** 客人絕對不能自己去金庫搬錢（直接修改變數），必須且只能透過呼叫內部函數（跟櫃台講話）來操作。
    * **目的：** 強迫所有人統一出入口，為第二層防護做準備。
2. **第二層防護：互斥 (Mutual Exclusion) ——「門口的單人管制」**
    * 因為第一層防護逼迫大家只能走正門（呼叫內部函數），作業系統只要在門口派一個警衛，規定「一次只能放一個客人進去跟櫃台講話」，就完美達成互斥了。
3. **如果少掉其中一個會怎樣？**
    * 只有單人管制，沒有封裝：雖然警衛在門口控管櫃台，但如果客人可以繞到銀行後方，打破窗戶直接修改金庫裡的錢（直接存取變數），那門口的單人管制就毫無意義，資料依然會崩潰。
    * 只有封裝，沒有單人管制：大家都乖乖去櫃檯了，但如果系統放任 5 個客人同時找 5 個櫃台修改同一個帳戶餘額，競態條件 (Race Condition) 依然會發生。

---
# 7.1 Classic Problems of Synchronization
### 一、為什麼需要探討「經典問題」？
前言中提到，本節將介紹一系列代表著廣大 並行控制問題 (Concurrency-control problems) 的經典範例。
這些問題（例如接下來會遇到的生產者－消費者問題、讀者－寫者問題、哲學家就餐問題）並非只是學術象牙塔裡的紙上談兵，它們是作業系統領域的「壓力測試基準 (Benchmark)」。
* **防護力測試：** 每當有學者或工程師提出一種「全新的同步演算法或機制」時，都必須將該機制套用到這些經典問題上進行測試，以證明新機制不會引發死結 (Deadlock) 或飢餓 (Starvation)。

### 二、號誌與互斥鎖的歷史與實務考量
* **學術傳統：** 為了解說方便與歷史傳承，課本在後續的解答中，將清一色使用 Semaphores (號誌) 來建構同步邏輯。
* **業界實務：** 但在現代真實的作業系統底層實作中，只要遇到只需要「互斥」功能的情境，工程師通常會直接使用 Mutex locks (互斥鎖) 來代替二元號誌，因為互斥鎖的底層實作往往更輕量且針對鎖的競爭有更好的效能最佳化。

### 三、Figure 7.1 生產者行程結構
課本在此處先釋出了一段「生產者 (Producer)」的標準程式碼結構，這段程式碼精準展示了如何將不同功能的同步工具組合在一起使用。
```cpp
while (true) {
    . . .
    /* 步驟一：產生一個新物品放在 next_produced 中 */
    . . .
    
    wait(empty);   // 步驟二：確認「有空位」，沒有空位就排隊休眠
    wait(mutex);   // 步驟三：確認「沒人正在使用緩衝區」，上鎖防護
    
    . . .
    /* 步驟四：將 next_produced 安全地放入緩衝區 (Buffer) */
    . . .
    
    signal(mutex); // 步驟五：解開防護鎖，離開緩衝區
    signal(full);  // 步驟六：通知全系統「多了一個已填滿的物品」
}
```
這段程式碼完美體現了兩種不同維度的同步控制：
* **資源控管 (Resource Management)：**
    * 利用 `empty` 與 `full` 這兩個計數號誌 (Counting Semaphores)。`wait(empty)` 的物理意義是「消耗一個空位資源」。如果緩衝區滿了（空位數量為 0），生產者會直接在這一行被系統強制休眠，連碰緩衝區的機會都沒有。這解決了資源溢出的問題。
* **互斥防護 (Mutual Exclusion)：**
    * 利用 `mutex` 這個**二元號誌** (或稱互斥鎖)。即便生產者確認了有空位，在真正要把物品塞進記憶體的瞬間，還是必須呼叫 `wait(mutex)` 建立絕對的單人結界。這防範了「多個生產者同時看到有空位，結果同時寫入同一個記憶體位址」的毀滅性資料覆蓋災難。

這種「先申請資源門票，再申請進入金庫」的雙重鎖設計，是撰寫安全的多執行緒應用程式 (Multithreaded applications) 最經典的標準起手式。
# 7.1.1 The Bounded-Buffer Problem
### 一、 解惑：為什麼「這段程式碼」就是解法？
這個問題早在第 6.1 節就已提出，當時展示了若沒有同步機制，`count++` 與 `count--` 會如何引發資料毀損。現在，課本要利用號誌 (Semaphores) 來給出最終的完美解答。

在併發程式設計 (Concurrent programming) 中，真正的業務邏輯（例如「把資料寫入陣列」或「從陣列讀取資料」）通常只有短短一行。真正的難題在於「如何決定誰什麼時候可以執行這一行」。

因此，課本提供的 Figure 7.1 與 Figure 7.2 程式碼結構，精準示範了在進入臨界區間 (Critical Section) 前後，必須放置哪些同步機制。這種「鎖的排列組合與順序」，就是同步問題的唯一解法。

### 二、 問題定義與共享資料結構
系統中存在一個可以容納 $n$ 個物品的緩衝區池 (Pool of $n$ buffers)。為了管控這個緩衝區，系統宣告了三個至關重要的共享號誌：
1. **`semaphore mutex = 1;` (二元號誌 / 互斥鎖)**
    * **物理意義：** 保衛緩衝區陣列的唯一一把大門鑰匙。確保同一時間絕對只有一個行程可以對緩衝區進行讀寫，達成互斥 (Mutual exclusion)。
2. **`semaphore empty = n;` (計數號誌)**
    * **物理意義：** 記錄目前緩衝區還有多少個「空位」。初始狀態整個緩衝區都是空的，所以值為 $n$。
3. **`semaphore full = 0;` (計數號誌)**
    * **物理意義：** 記錄目前緩衝區已經裝了多少個「完整的產品」。初始狀態沒有半個產品，所以值為 $0$。

### 三、程式碼結構對照與動態推演
**【生產者視角 (Figure 7.1)】**
```cpp
while (true) {
    /* 準備好新產品 next_produced */
    
    wait(empty);   // 1. 抽號碼牌：申請一個「空位」。如果 empty 是 0，代表客滿，在此強制休眠！
    wait(mutex);   // 2. 拿鑰匙：確認沒人在使用緩衝區，鎖上大門。
    
    /* 把 next_produced 放入緩衝區 */
    
    signal(mutex); // 3. 還鑰匙：操作完畢，打開大門。
    signal(full);  // 4. 廣播：通知全系統「多了一個完整的產品囉！」
}
```
**【消費者視角 (Figure 7.2)】**
```cpp
while (true) {
    wait(full);    // 1. 抽號碼牌：申請一個「產品」。如果 full 是 0，代表沒貨，在此強制休眠！
    wait(mutex);   // 2. 拿鑰匙：確認沒人在使用緩衝區，鎖上大門。
    
    /* 從緩衝區取出產品放到 next_consumed */
    
    signal(mutex); // 3. 還鑰匙：操作完畢，打開大門。
    signal(empty); // 4. 廣播：通知全系統「多騰出一個空位囉！」
    
    /* 消耗該產品 */
}
```
# wait() & signal()
### 第一層本質：它們是「作業系統的系統呼叫 (System Calls)」
在最底層的實作中（例如 6.6.2 節提到的改良版號誌），wait() 和 signal() 的內部必定會呼叫作業系統的核心功能：
* **`wait()` 的底層本質是 `sleep()` 或 `block()`：** 當一個行程呼叫 `wait()` 且拿不到資源時，這個函式會直接拔掉該行程的 CPU 執行權，把它的狀態從 `Running` 變成 `Waiting`，然後把它丟進等待佇列。
* **`signal()` 的底層本質是 `wakeup()`：** 這個函式會去檢查等待佇列，把裡面休眠的行程狀態改回 `Ready`，重新放入 CPU 的排隊隊列中準備執行。

一般應用程式的開發者無法、也不應該自己從頭用 `while` 迴圈刻出真正的 `wait()` 和 `signal()`，因為一般程式沒有權限去操作 CPU 的休眠與喚醒機制。

### 第二層本質：它們是「自帶硬體防護罩」的函式 (原子性)
為什麼不能自己寫一個普通的函式來做這件事？因為普通的函式會被中斷 (Interrupt)。

如第 6.4 節所述，作業系統在實作這兩個函式時，會在它們的程式碼外面包上一層「硬體級的防護」（例如關閉中斷、或是使用 `compare_and_swap` 晶片指令）。
這確保了當一個行程進入 `wait()` 或 `signal()` 的程式碼瞬間，絕對不會被中斷，這個特性稱為原子性 (Atomicity)。它們是堅不可摧的單一操作單位。

### 第三層本質：最容易搞混的「雙重身份」
在作業系統課本中，最讓人痛苦的是：號誌 (Semaphore) 和 管程的條件變數 (Condition Variable)，它們用來操作的函式名字一模一樣，都叫做 `wait()` 和 `signal()`，但物理行為完全不同！

**身份一：在「號誌 (Semaphore)」中的 wait 與 signal**
* 在這裡，它們是在**操作一個實體數字 (資源數量)**。
    * **`wait(S)` (又稱 P 操作、Down 操作)：** 物理意義是「消耗資源」。它一定會把數字減 1。如果減完發現數字小於 0，就乖乖去睡覺排隊。
    * **`signal(S)` (又稱 V 操作、Up 操作)：** 物理意義是「歸還資源」。它一定會把數字加 1，並且會永久記住這個狀態。即使現在沒有人在排隊，數字還是會忠實地加 1（例如從 0 變 1），下一個來呼叫 `wait()` 的人就可以直接拿走。

### 身份二：在「管程條件變數 (Condition Variable)」中的 wait 與 signal
在這裡，它們沒有任何數字可以扣，純粹是「信號傳遞」。
* **`x.wait()`：** 物理意義是「我條件不滿足，強制退位」。呼叫它的行程會無條件立刻進入休眠，並讓出管程的活躍權。
* **`x.signal()`：** 物理意義是「條件可能改變了，叫醒一個人」。
* **最關鍵的差異：** 如果當下沒有任何人在休眠排隊，呼叫它會毫無反應 (No effect)！信號會直接消失在空氣中，不會被記下來。下一個呼叫 wait() 的人還是會乖乖去睡覺。

### 深入剖析 x.wait()：主動退位與交出鑰匙
在管程中，`x.wait()` 絕對不是系統幫你呼叫的，它是程式設計師（你）在程式碼中寫下的一個「避險機制」。

**【呼叫時機】**
當行程好不容易突破了管程的大門（取得唯一活躍權），準備執行任務時，卻發現「邏輯條件不滿足」（例如：想領錢但帳戶沒錢、想讀資料但緩衝區是空的）。此時，程式設計師會讓行程執行 `x.wait()`。

**【慢動作拆解：執行 x.wait() 的瞬間發生了什麼？】**
這是一個「三合一」的原子操作（不可分割）：
1. **停止執行並休眠：** 該行程會立刻被拔除 CPU 執行權，狀態從 `Running` 變成 `Waiting`。
2. **移入專屬等候區：** 系統會把這個行程放到「條件變數 $x$ 專屬的等待佇列」中排隊。
3. **釋放管程互斥鎖 (最關鍵的一步)：**
    這是初學者最容易忽略的重點！因為該行程已經佔據了管程的「唯一名額」，如果它抱著這個名額睡著，外面的生產者永遠進不來，也就永遠沒人能把資源補齊。所以，`x.wait()` 在讓行程睡著的同時，會自動把管程的大門解鎖，讓外面 Entry Queue 苦苦等候的其他行程可以進來活躍。
    
* **實體比喻：** 你（行程）好不容易排到銀行櫃檯（進入管程），結果發現忘記帶印章（條件不滿足）。你只能大喊一聲 `wait()`，然後離開櫃台，坐到旁邊的「補印章等候區（條件變數佇列）」，並把櫃台讓給下一個客人。

### 深入剖析 x.signal()：精準喚醒與「失憶症」
當另一個行程進入管程，並且改變了某些狀態（例如：生產者把資料放進緩衝區了、有人存錢進帳戶了），這個行程有義務呼叫 `x.signal()` 來通知可能在睡覺的人。

**【慢動作拆解：執行 `x.signal()` 的瞬間發生了什麼？】**
系統會立刻去檢查「條件變數 $x$ 專屬的等待佇列」。接下來會發生兩種截然不同的情況：

**情況 A：佇列裡面有人 (有人在呼叫過 wait 而休眠)**
1. **精準挑選：** 系統會從 $x$ 的等待佇列中，挑出剛好一個 (Exactly one) 行程準備喚醒。
2. **喚醒與交接：** 被挑中的行程會被喚醒。但因為管程內只能有一個人活躍，這時就會觸發我們之前提過的流派選擇：
    * **若是 Signal and continue：** 發信號的人繼續做事，剛醒來的人被丟回門口的 `Entry Queue` 排在最前面，等發信號的人離開後再接手。
    * **若是 Signal and wait：** 發信號的人立刻退位休眠，把管程直接讓給剛醒來的人。

**情況 B：佇列裡面沒有人 (空無一物) —— 🌟 考試與面試必考題**
如果在呼叫 `x.signal()` 的這一瞬間，沒有任何行程在 $x$ 佇列裡睡覺，那麼：
* **毫無反應 (No Effect)：** 這個信號會直接「消失在空氣中」。
* **沒有記憶 (Memoryless)：** 管程**絕對不會**把這個信號記下來。這點與號誌 (Semaphore) 完全不同！號誌如果沒人等，呼叫 signal 會把數字 +1 存起來；但在管程中，如果下一秒才有個行程跑來呼叫 `x.wait()`，這個行程依然會立刻睡著，它不能拿上一秒發生過的 `signal` 來抵銷。
* **實體比喻：** 櫃台人員（發信行程）對著「補印章等候區」廣播：「現在可以辦理囉！」（呼叫 signal）。如果等候區剛好有人，那個人就會醒來準備辦事；如果等候區根本沒人，這句廣播就隨風飄散了，櫃台人員也不會記在黑板上。








# 7.1.2 The Readers–Writers Problem
這是一個極度經典的並發控制基準測試 (Benchmark)。在現實世界中，大多數資料庫或檔案系統的存取模式，都符合這個問題的物理設定。

### 一、問題定義與核心衝突
假設有一個資料庫被多個並發行程 (Concurrent processes) 共享：
* **Readers (讀者)：** 只想要讀取資料庫的內容，絕對不修改。
* **Writers (寫者)：** 想要更新（也就是讀取並寫入）資料庫的內容。

**【衝突點分析】**
如果兩個 Readers 同時存取資料庫，並不會發生任何不良影響 (No adverse effects)。然而，只要有一個 Writer 介入，並且與其他行程（不管是 Reader 還是其他 Writer）同時存取資料庫，系統就會陷入混亂 (Chaos)。
**防護鐵律：** Writer 在進行寫入時，必須擁有對共享資料庫的排他性獨佔權 (Exclusive access)。

### 二、變形探討與飢餓 (Starvation) 的誕生
為了解決這個衝突，學術界延伸出幾種變形，這些變形都牽涉到「優先權 (Priorities)」的取捨：
1. **第一類 (First readers-writers problem)：**
    **規則：** 只要目前有 Reader 正在讀取，且 Writer 尚未取得進入許可，那麼「新來的 Reader」都不需要為了等待 Writer 而停下來。
    **代價：** 這會導致 Writers 發生飢餓。如果 Readers 源源不絕地湧入，Writer 可能永遠等不到資料庫淨空的那一刻。
2. **第二類 (Second readers-writers problem)：**
    **規則：** 一旦有 Writer 準備好要寫入並開始排隊，系統就「不再允許任何新的 Reader」進入資料庫。
    **代價：** 這會導致 Readers 發生飢餓。

### 三、 共享資料結構與變數宣告
為了解決第一類問題，Readers 與 Writers 必須共享以下三個資料結構：
```cpp
semaphore rw_mutex = 1;
semaphore mutex = 1;
int read_count = 0;
```
* **`int read_count：`** 這是一個整數計數器，精準記錄「目前有多少個行程正在讀取物件」。
* **`semaphore mutex：`** 這是一個二元號誌（互斥鎖）。它的唯一功用，就是保護 `read_count` 這個變數。當多個 Reader 試圖同時加減 `read_count` 時，提供互斥防護。
* **`semaphore rw_mutex：`** 這是保護「共享資料庫本體」的二元號誌。所有的 Writers 都必須搶奪這把鎖；而 Readers 陣營，則由第一個進入與最後一個離開的代表來操作這把鎖。

### 四、 程式碼深度解析
**【Writer 視角 (Figure 7.3)】**
```cpp
while (true) {
    wait(rw_mutex); // 申請資料庫獨佔權
    
    /* 執行寫入動作 (writing is performed) */
    
    signal(rw_mutex); // 釋放資料庫獨佔權
}
```
Writer 的邏輯極度單純。它不在乎外面有多少 Reader 或 Writer，它只死死盯著 `rw_mutex` 這把大鎖。只要能突破這道防線，它就進去寫入，寫完就開鎖。

**【Reader 視角 (Figure 7.4)】**
Reader 的邏輯展現了極致的「群體協作與代表制」：
```cpp
while (true) {
    /* --- 進入區段 (Entry Section) --- */
    wait(mutex);           // 1. 鎖住計數器，準備更新人數
    read_count++;          // 2. 宣告：多了一個 Reader 進入
    if (read_count == 1)   // 3. 關鍵判斷：如果我是「第一個」進來的 Reader
        wait(rw_mutex);    //    由我負責去搶資料庫的 rw_mutex 大鎖，把 Writer 擋在門外！
    signal(mutex);         // 4. 計數器更新完畢，解開計數器的鎖

    /* 執行讀取動作 (reading is performed) */
    /* 注意：此時其他 Reader 只要通過 mutex 的計數器加法，就可以跟著一起進入這個區段，不需要再搶 rw_mutex！ */

    /* --- 離開區段 (Exit Section) --- */
    wait(mutex);           // 5. 鎖住計數器，準備更新人數
    read_count--;          // 6. 宣告：一個 Reader 準備離開
    if (read_count == 0)   // 7. 關鍵判斷：如果我是「最後一個」離開的 Reader (裡面空了)
        signal(rw_mutex);  //    由我負責把 rw_mutex 大鎖解開，讓外面苦等的 Writer 可以進來！
    signal(mutex);         // 8. 計數器更新完畢，解開計數器的鎖
}
```
**邏輯推演：為何會造成 Writer 飢餓？**
假設有一個 Writer 在外面苦苦等待 `rw_mutex`。只要資料庫裡面一直維持著 `read_count > 0` 的狀態（即使裡面的 Reader 不斷交替更換），`read_count == 0` 的條件就永遠不會觸發。因此 `signal(rw_mutex)` 永遠不會被呼叫，Writer 就只能在門外永久罰站 (Starve)。

### 五、 系統實務：讀寫鎖 (Reader-Writer Locks)
因為這個問題太過經典，許多現代作業系統已經將上述邏輯封裝成高階的 API，稱為 **Reader-Writer Locks (讀寫鎖)**。

開發者在申請鎖時，只需指定要使用「讀取模式 (read mode)」還是「寫入模式 (write mode)」。

**架構師的效能評估 (何時該用？)：**
* 這種鎖最適合用在「容易區分誰讀誰寫」，且 「讀者數量遠大於寫者 (more readers than writers)」 的應用程式中。
* 雖然建立 Reader-Writer Lock 的底層系統開銷 (Overhead) 比建立單純的互斥鎖 (Mutex) 還要高，但因為它允許多個讀者同時讀取，這種「並發性提升 (Increased concurrency)」所帶來的效能增益，足以完美彌補那些額外的開銷。

# 7.1.3 The Dining-Philosophers Problem
### 一、 問題的物理設定
![image](https://hackmd.io/_uploads/SkmTAqiRbl.png =50%x)
這是一個看似簡單，卻隱含著巨大系統設計風險的經典模型。它之所以重要，並非因為電腦科學家討厭哲學家，而是因為它精準代表了一大類「如何在不發生死結與飢餓的前提下，將多個資源分配給多個行程」的底層難題。

* **實體配置：** 5 位哲學家圍繞著一張圓桌。桌子中央有一碗飯，桌上有 5 支單獨的筷子 (single chopsticks)。
* **行為模式：** 哲學家的一生只在兩個狀態間交替：思考 (Thinking) 與 進食 (Eating)。
* **資源限制：**
    1. 哲學家肚子餓時，只能嘗試拿起「最靠近自己」的兩支筷子（左手邊與右手邊）。
    2. 每次只能拿起「一支」筷子。
    3. 絕對不能從鄰居手中把筷子搶過來。
    4. 必須「同時湊齊兩支筷子」才能開始吃飯。吃完後，同時放下兩支筷子並繼續思考。

### 二、 7.1.3.1 號誌解法 (Semaphore Solution) 與死結災難
最直覺的解法，是把每一支筷子都當作一個獨立的互斥鎖。

**【資料結構】**
宣告一個長度為 5 的號誌陣列，全部初始化為 1（代表筷子可用）：`semaphore chopstick[5];`
**【哲學家 $i$ 的程式碼 (Figure 7.6)】**
```cpp
while (true) {
    wait(chopstick[i]);           // 1. 嘗試拿起左手邊的筷子
    wait(chopstick[(i+1) % 5]);   // 2. 嘗試拿起右手邊的筷子
    
    /* 湊齊兩支，開始進食 (eat for a while) */
    
    signal(chopstick[i]);         // 3. 吃飽了，放下左手邊的筷子
    signal(chopstick[(i+1) % 5]); // 4. 放下右手邊的筷子
    
    /* 繼續思考 (think for awhile) */
}
```
* **陣列邏輯解析：** 因為桌子是圓的，若目前是第 4 號哲學家 (編號 0~4 中的最後一位)，他的右手邊並不是第 5 號筷子（不存在），而是第 0 號筷子。因此必須使用餘數運算 `(i+1) % 5` 來達成環狀陣列的索引對應。

**災難推演：死結 (Deadlock) 是如何發生的？**
這個解法確保了相鄰的兩人絕對不可能同時吃飯（滿足互斥），但它有一個毀滅性的漏洞：
1. 假設某個瞬間，**5 位哲學家同時肚子餓了**。
2. 5 個人同時執行了第一行程式碼 `wait(chopstick[i]);`。
3. 此時，桌上的 5 支筷子全部分別被 5 個人拿在左手中。所有的 `chopstick` 號誌值都變成了 0。
4. 接著，5 個人同時執行第二行 `wait(chopstick[(i+1) % 5]);`，試圖拿右手邊的筷子。
5. **結局：** 每個人都在等右手邊的鄰居放下筷子。這形成了一個完美的「**循環等待 (Circular Wait)**」，沒有人能吃到飯，也沒有人會放下左手的筷子。系統徹底當機。

**【課本提出的死結救援方案】**
為了解決上述死結，課本列出了三種系統設計思維：
1. **限制人數：** 最多只允許 4 位哲學家同時坐在桌邊。（5 支筷子分給 4 個人，根據鴿籠原理，絕對至少有一人能湊齊 2 支）。
2. **原子性拿取 (Atomic pickup)：** 強制規定必須「同時」拿起左右兩支筷子，且這個拿取動作必須在臨界區間 (Critical section) 內執行。
3. **非對稱解法 (Asymmetric solution)：** 打破對稱性。規定奇數編號的哲學家「先拿左、再拿右」，偶數編號的哲學家「先拿右、再拿左」。這會直接破壞循環等待的條件。
*(備註：課本強調，即使解決了死結，仍必須防範某個哲學家運氣極差、永遠搶不到筷子而餓死 (Starve to death) 的情況。)*

### 三、7.1.3.2 管程解法 (Monitor Solution) 的起手式
為了解決死結，課本接下來展示了如何利用上一節學到的 管程 (Monitor) 來建構一個保證不發生死結的演算法。這個解法採用了上述的「第二種救援方案」：只有當兩支筷子都可用時，才允許拿起。

**一、 資料結構與初始化**
```cpp
enum {THINKING, HUNGRY, EATING} state[5];
condition self[5];

initialization_code() {
    for (int i = 0; i < 5; i++)
        state[i] = THINKING;
}
```
* 狀態陣列 (`state[5]`)：精準追蹤五位哲學家的當前活動。系統啟動時，預設所有人都在思考 (`THINKING`)。
* 獨立休眠區 (`self[5]`)：宣告五個獨立的條件變數 (Condition Variables)。這意味著每位哲學家都有一個專屬的「等待佇列」。若拿不到筷子，可以直接在自己的專屬變數上休眠，互不干擾。

### 二、 核心判斷邏輯：test(int i) 函數
這是整個演算法的靈魂，負責判定第 `i` 位哲學家是否具備進食資格。
```cpp
void test(int i) {
    if ((state[(i + 4) % 5] != EATING) && 
        (state[i] == HUNGRY) && 
        (state[(i + 1) % 5] != EATING)) {
        state[i] = EATING;
        self[i].signal();
    }
}
```
* **嚴格的啟動條件：** 必須「同時」滿足三個條件：
    1. 左邊鄰居不在吃飯 (`state[(i + 4) % 5] != EATING`)。
    2. 自己處於飢餓狀態 (`state[i] == HUNGRY`)。
    3. 右邊鄰居不在吃飯 (`state[(i + 1) % 5] != EATING`)。
* **狀態轉移與喚醒機制：** 若條件成立，將自己標記為 `EATING`，並且執行 `self[i].signal()`。
    * **架構補充：** 若 `test()` 是由自己準備吃飯時所呼叫，此處的 `signal()` 會產生「毫無反應 (No effect)」的結果（因為自己尚未進入休眠）；但若 `test()` 是由鄰居吃飽放下筷子時幫忙呼叫的，這個 `signal()` 就能發揮作用，精準喚醒正在休眠的自己。

### 三、 拿取筷子：pickup(int i) 函數
```cpp
void pickup(int i) {
    state[i] = HUNGRY;
    test(i);
    if (state[i] != EATING)
        self[i].wait();
}
```
* **執行流程：**
    1. 首先宣告肚子餓 (`state[i] = HUNGRY`)。
    2. 呼叫 `test(i)` 嘗試評估是否能拿起左右筷子。
    3. **最後防線：** 若 `test(i)` 執行完畢後，狀態仍然不是 `EATING`（代表左右鄰居至少有一人正在吃飯），便強制執行 `self[i].wait()`，該哲學家陷入休眠並交出管程的活躍權。

### 四、 放下筷子：putdown(int i) 函數
```cpp
void putdown(int i) {
    state[i] = THINKING;
    test((i + 4) % 5);
    test((i + 1) % 5);
}
```
* **執行流程：**
    1. 吃飽後，將狀態恢復為 `THINKING`。
    2. **大愛精神（主動喚醒鄰居）：** 放下筷子後，主動呼叫 `test()` 檢查左鄰居 `(i + 4) % 5`，隨後檢查右鄰居 `(i + 1) % 5`。
    3. 若鄰居剛好處於 `HUNGRY` 狀態，且該鄰居的另一邊也沒人在吃飯，這個 `test()` 呼叫就會觸發內部的 `signal()`，將苦等的鄰居喚醒。

### 五、 系統架構反思：死結與飢餓的本質
這套管程解法完美消滅了「死結 (Deadlock)」。因為哲學家只會在「兩支筷子都絕對可用」的情況下才會改變狀態為 `EATING`，徹底打破了「持有並等待 (Hold and Wait)」的死結發生條件。

然而，課本在最後一段落下了沉重的工程警告：無死結的解法，並不必然消除飢餓的可能性 (A deadlock-free solution does not necessarily eliminate the possibility of starvation)。
試想一個極端情境：某位哲學家的左右鄰居非常有默契地「輪流」吃飯。這位夾在中間的哲學家即使每次呼叫 `pickup()` 宣告飢餓，也會因為 `test()` 條件永遠無法同時滿足，而不斷被判定進入 `wait()` 休眠，最終在無盡的等待中活活餓死。