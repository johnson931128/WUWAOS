# Spec 03：互動 Shell、PCB 與固定雙行程 Pipe

## 目標與確認範圍

使用者希望在 QEMU 輸入操作，建立 process（行程）並觀察狀態變化，加入 shell（命令列介面）、pipe（行程間管道）與 PCB（行程控制區塊）。
已確認 pipe 先採內建 writer | reader，實作真實 byte stream（位元組資料流）與阻塞／喚醒，不加入通用多段管線、檔案重導向、外部程式載入或計時器搶占。
Spec 01／02 以獨立文件保留；本文件為目前規格。保留前階段尚未提交的改動，不自行 commit / push。
未經同意不加入測試程式碼。使用正常互動指令、內建工作程式與 QEMU 檢查功能驗收，不新增測試框架。

## 架構與介面

- 開機 PID 1 在 U-mode（使用者模式）執行 shell，以 read/write 等 syscall（系統呼叫）操作核心，不在核心直接解析指令。
- 原 Process 資料就是 PCB，以 ProcessControlBlock 結構名稱明確表達，不另外複製一份狀態。
- PCB 加入內建程式角色、輸入／輸出管道、待完成的 I/O（輸入輸出）操作、使用者緩衝區及長度。
- 維持最多 4 個行程；spawn 先接受 worker、quick 兩種內建程式，pipe 另外建立 writer、reader。
- 使用者程式碼與唯讀資料擴為 16 KiB，起點仍為 0x400000；可寫資料移至 0x404000，使用者 stack（堆疊）仍為 0x800000 的一頁。
- 每個行程仍有獨立 code/data/stack（程式碼／資料／堆疊）及頁表；核心與 UART（序列埠）不允許使用者模式直接存取。

## Shell 指令

| 指令 | 行為 |
| --- | --- |
| help | 顯示指令與限制 |
| ps | 列出有效 PCB 的 PID、父 PID、名稱、狀態、等待原因與可用欄位數 |
| pcb <pid> | 顯示特定 PCB 的頁表、堆疊、保存指令位址、等待資料、結束碼、管道端點 |
| spawn worker | 背景建立工作程式：yield → sleep(7) → 喚醒後結束 |
| spawn quick | 背景建立立即完成的子行程，便於觀察 Zombie（待回收） |
| yield | shell 主動讓出 CPU |
| wakeup <channel> | 通知事件通道，顯示喚醒人數 |
| wait <pid> | 等待直接子行程並回收，顯示 PID 與結束碼 |
| kill <pid> | 結束 shell 的直接子行程（不允許殺 PID 1／自己），關閉管道並留下可 wait 的結果 -9 |
| trace on/off | 開關子行程狀態轉換紀錄；預設開啟。shell 自身不列追蹤，避免打斷提示字元，可用 pcb 1 查閱 |
| writer \| reader | 前景建立兩個行程與一條真實 pipe，等待兩者結束並回收 |
| exit | 結束 shell；Ctrl+A，再按 X 仍可離開 QEMU |

不提供任意檔案執行、背景 & 語法、引號、一般參數展開、多段管線、檔案系統或主機命令。
Shell 只將固定命令轉成系統呼叫；未知語法顯示錯誤。數字解析檢查溢位，命令長度有界。

## 輸入與合作式排程

- 新增 read(0, buffer, capacity)，console（終端）採行輸入，容量 2–128 bytes，包含換行，可處理回車／換行與退格；超長行丟棄超出內容直到換行，不當成多條命令。
- 等待輸入時 PCB 進入 Waiting(console)，由核心輪詢 UART，完成一行後填回字元與返回值，再變成 Ready。
- scheduler（排程器）每次系統呼叫後輪詢待完成 I/O。沒有 Ready、但有終端等待者時持續輪詢，不使用關閉中斷後無法喚醒的 WFI。
- 工作程式仍採合作式；不主動讓出或阻塞的程式會占用 CPU。無 console 等待者也無 Ready 的情況仍顯示阻塞診斷。
- 字元回顯由核心行輸入處理；命令解析與提示字元由使用者 shell 處理。
- shell 每次提示前讓出兩輪，讓本版內建背景程式先到達等待／結束點，避免背景追蹤輸出蓋過新提示；不是計時器搶占或通用背景工作控制。

## Pipe 演算法與資源

- 最多 2 條管道，每條 16-byte ring buffer（環形緩衝區），保存讀／寫索引、資料量、讀／寫端點數。
- pipeline(pids) 專用系統呼叫，確認兩個行程欄位及管道資源後建立內建 writer、reader，把兩個 PID 寫回 shell。
- writer 的 fd=1（輸出描述號）接 pipe，reader 的 fd=0（輸入描述號）接同一 pipe；reader 的輸出仍為 console。其他程式 fd0/1 為 console。
- 不實作通用 pipe()/dup2()／任意描述號表；這版只提供明確固定的雙行程管線。
- read：有資料時最多讀取請求量；空且仍有 writer 時 Waiting(pipe read)；空且寫端已關閉時回傳 0，表示 EOF（資料結束）。
- write：有空間時可回傳部分 byte 數；滿且仍有 reader 時 Waiting(pipe write)；讀端全關閉時回傳 -32（broken pipe，沒有讀者）。writer 以迴圈處理短寫。
- PCB 保存尚未完成的 I/O 請求；I/O 輪詢完成一次傳輸或 EOF／錯誤後填 a0（返回值暫存器），改成 Ready，不重複執行 ecall。
- exit／kill 自動關閉該行程的端點、取消尚未完成的 I/O；兩端都關閉時釋放管道欄位。關閉事件會讓另一端的 pending I/O（待完成請求）被重新判斷。
- 所有讀寫先驗證完整的使用者範圍，之後透過指定頁表的核心映射複製；不直接信任使用者指標。

新增 syscall 編號：9=ps、10=pcb、11=kill、12=read、13=pipeline、14=trace。既有 1–8 保留。
只提供上述固定描述號與管線，不宣稱完成一般 Unix shell 或訊號／工作控制。

## 變更範圍與步驟

1. 擴充 console 非阻塞輸入、vm 使用者布局與 byte 回寫。
2. 擴充 PCB、行程查詢／終止／追蹤控制，讓排程可輪詢等待中的 I/O。
3. 新增 kernel/io.c/h 管理行輸入、固定管道與 pending I/O。
4. 擴充 syscall、user API、user/shell.c 與內建工作程式；修改 Makefile／user linker。
5. 更新 README、CODE_GUIDE 及本 spec，在實際 QEMU 輸入指令確認正常行為。

## 驗收條件

- 開機出現 wuwaos>，可輸入 help、ps、pcb 1。
- spawn worker 後 ps 可見等待通道 7；wakeup 7 後變 Zombie，wait 後不再占用有效欄位。
- spawn quick、kill 等基本控制可操作；無需重新編譯才能新增行程。
- writer | reader 的訊息由 pipe 真實轉送，資料量超過 16 bytes，出現滿／空造成的阻塞與喚醒，EOF 後兩行程回收，提示字元恢復。
- 管線可以重複執行，沒有遺留管道或行程欄位。
- PCB 顯示真實保存狀態；使用者仍無權直接存取核心與裝置。
- 驗收紀錄區分互動正常流程與未執行的負面／邊界路徑。

## 實際驗收紀錄

2026-10-16，Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3，WSL Ubuntu。

- 完整建置成功，啟用 -Wall -Wextra -Werror；核心與使用者 ELF 無未解析符號。
- 在真實 QEMU UART 互動輸入 help、ps、pcb 1，開機提示可操作。
- spawn worker 得到 PID 2，ps 顯示 Waiting channel；pcb 2 顯示真實頁表、sepc（保存指令位置）、堆疊及 channel=7。
- wakeup 7 回傳 1，worker 恢復並 exit(0)；ps 顯示 Zombie，wait 2 取得 0 並回收。
- spawn quick 得 PID 3，ps 可見 Zombie；wait 3 得到結束碼 40。
- 再建立等待事件的 PID 4，kill 4 成功，wait 4 得到 -9。
- writer | reader 建立 PID 5／6，真實 16-byte 緩衝區產生 pipe write／pipe read 等待與 I/O ready 喚醒；reader 收到完整 80 bytes 與 EOF，兩者 status=0 並回收。
- trace off 後使用無空白的 writer|reader 再執行一次（PID 7／8），結果相同。ps 只剩 PID 1，Free PCB slots=3。
- 輸入編輯用 Backspace 將 pcx 修正為 pcb，pcb 1 正常完成；LF Enter 與 CR Enter 皆可完成命令。未加入測試原始碼。
- QEMU monitor 讀取正在等待輸入的 shell PCB，state=4（Waiting）；讀取兩個實際 pipe 欄位，used／readers／writers 都為 0，確認重複管線後沒有占用資源。
- 實際頁表顯示使用者 0x400000 的 16 KiB 為 RXU、0x404000 與 0x800000 各一頁為 RWU；UART 及核心不帶 U 權限。
- shell exit 後顯示 All processes reaped; idle，再由 QEMU 的 Ctrl+A、X 離開。不是主機 shell 的假提示。
- 本機 build/shell-console.log、build/shell-traps.log 保存互動輸出及使用者 ecall 紀錄，不納入 Git。
- 最後補充 PCB 的使用者 stack 頂端與 I/O 緩衝區欄位後重新建置，啟動 QEMU 執行 pcb 1、exit；新增欄位可讀且正常退出。補充紀錄位於 build/final-shell-console.log。

未動態驗證的邊界：滿行程表／滿管道資源的失敗回復、非法或跨頁使用者指標、超長行／數字溢位、讀端提早關閉的 broken pipe、阻塞 I/O 期間 kill 管道端點、孤兒接管、多個 console 讀取者、所有行程無事件可喚醒等分支。已做程式碼檢查，不宣稱負面測試通過。
本版支援正常互動觀察，未提供 Ctrl+C 取消前景 wait；使用者應先 wakeup 再 wait 等事件的 worker。後續可另訂計時器與工作取消規格。
