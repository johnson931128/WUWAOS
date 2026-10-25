# WUWAOS

可在 QEMU 上操作的小型 RISC-V 64-bit 作業系統。開機進入 U-mode（使用者模式）的 shell（命令列介面），透過 syscall（系統呼叫）控制行程。

記憶體使用 Sv39（三級分頁）：L2 → L1 → L0，每張表 4 KiB、512 個表項。頁表與使用者實體頁由空閒頁面串列動態配置；fork 複製私有頁，exec 切換後回收舊空間，wait 回收 Zombie 的頁面。`ps` 會顯示 `Memory pages: free=... total=... page_size=4096`，可在建立前後觀察頁數，全部子行程回收後應回到原值。現階段限 QEMU virt／128 MiB，使用者 code/data/stack 大小仍固定，尚無 heap 或缺頁配置；設計見 [Spec 08](docs/SPEC-08-memory.md)。

## 啟動

在專案根目錄的 PowerShell 執行：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make'
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

需要 WSL Ubuntu 的 Clang、LLD、llvm-objcopy、Make、qemu-system-riscv64 與 OpenSBI。
已使用 Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3 驗證。平台固定為 virt、單核心、128 MiB RAM。
更改編譯選項後先 `make clean`。build/ 的映像與本機日誌不納入 Git。

開機後看到：

先出現由中央向左右展開的 `WUWAOS` ASCII ART（文字字元圖案），約 1.4 秒後保留完整字樣並顯示核心資訊，再進入下方 shell。
動畫使用 ANSI 終端控制碼，建議至少 80 欄；會清除先前 OpenSBI 的可見輸出。速度由 kernel/splash.c 的每格 45 ms 與最後停留 200 ms 控制。

```text
WUWAOS user shell. Type help. State tracing is on.
wuwaos>
```

**在 wuwaos> 後輸入下面的命令，不是在 PowerShell 輸入。** 按 Enter 執行，可用 Backspace 修正。
按 Ctrl+A，再按 X 離開 QEMU；shell 的 exit 只結束 shell，不會關閉 QEMU。

## Fork → Exec → 排程 → 回收

從全新啟動開始，在 `wuwaos>` 一次輸入一條：

```text
fork
ps
wait 2
run counter
run counter
ps
pcb 3
wakeup 9
wait 3
wait 4
ps
```

`fork` 複製 shell 的使用者記憶體與暫存器。父返回子 PID，子返回 0；子修改後顯示 data=101、stack=101，父仍為 data=1、stack=100。子結束後保留 Zombie（已退出、待回收），`wait 2` 才釋放 PCB（行程控制區塊）欄位。

`run counter` 先 fork，再由子行程 exec（替換執行程式）。shell 與 app 是獨立映像；exec 保留子 PID、換掉使用者程式碼／資料／堆疊，從新入口執行。兩個 counter 先 sleep（等待事件）於通道 9，因此可用 ps／pcb 觀察。

`wakeup 9` 後兩者各自從 3 倒數至 1，每次 yield（主動讓出 CPU）使另一個 Ready（就緒）行程取得執行機會；`wait` 回收結果 0。最後 ps 應只剩 shell、Free PCB slots: 3。

目前使用 FIFO（先進先出）就緒佇列進行非搶占式排程：新建立或被喚醒的行程排到隊尾，從隊首取出下一個行程；Running 行程執行普通 syscall 時繼續執行，直到阻塞、結束或主動 yield。保留合作式 yield，讓出後重新排到隊尾，沒有計時器時間片。印出的提示字元與背景輸出可能交錯；`run` 不自動回收，請依輸出的 PID 執行 wait。若不是全新啟動，PID 不一定是上述數字。

`ps` 會附上 `Ready queue (FIFO), count=N: ...`，只列出等待 CPU 的 Ready 行程，不含正在執行的 shell。`pcb <pid>` 的 `ready_queued=1` 表示該 PCB 已在佇列中；0 表示不在佇列。背景行程可能在輸入下一條命令前已經完成，因此 ps 看到空佇列也是正常情況。

也可用 `run quick` 觀察結束碼 40，或 `run worker` 搭配 `wakeup 7`／`wait <pid>`。下方保留直接 spawn（建立指定程式）的操作。

## fan 文字編輯器

在 QEMU 的 `wuwaos>` 輸入：

```text
fan /home/user/note.txt
```

直接輸入文字、空白及 Enter 換行。方向鍵移動，Home／End 到行首／行尾，Backspace／Delete 刪除，PageUp／PageDown 捲動，Tab 插入空白。`Ctrl+O` 儲存，`Ctrl+X` 離開；有未儲存內容時選 `y` 儲存後離開、`n` 捨棄、`c` 或 Escape 取消。`Ctrl+L` 完整重畫。離開後用 `cat /home/user/note.txt` 讀回，QEMU 重啟後檔案仍保留。

fan 是在 WUWAOS 使用者模式執行的獨立 C 程式，畫面經 UART 與 ANSI 控制碼顯示在終端，需至少 80×24。第一版支援最多 2 KiB ASCII／LF 文字；超大檔案、UTF-8 或不支援的控制字元會拒絕開啟，避免截斷覆寫。既有 Tab 以 `>` 顯示並保留。新檔案直到儲存才建立，父目錄須先存在。沒有 Vim 命令模式、undo 或一般磁碟執行檔載入；fan 映像仍嵌入核心。詳見 [Spec 09](docs/SPEC-09-fan.md)。

## 直接建立行程並觀察

從全新啟動開始，一次輸入一條：

```text
help
ps
pcb 1
spawn worker
ps
pcb 2
wakeup 7
ps
wait 2
ps
```

實際流程：

1. shell 為 PID 1（行程識別碼）；spawn 建立 PID 2。
2. 追蹤訊息顯示 New（建立中）→ Ready（就緒）→ Running（執行中）。
3. worker（工作程式）先 yield（讓出 CPU），再 sleep(7) 等事件；ps 可見 Waiting channel（等待通道）。
4. pcb 2 顯示實際 PCB（行程控制區塊）的父 PID、狀態、頁表、保存指令位址、堆疊與通道 7。
5. wakeup 7 顯示 woken=1，子行程恢復執行，最後進入 Zombie（已結束、等待回收）。
6. wait 2 取得結束碼 0，將欄位回收到 Unused（可重用）；ps 不再列出 PID 2。

PID 會持續遞增；若不是剛啟動，請依 spawn 顯示的 PID 操作，不要固定使用 2。
可繼續輸入 `spawn quick`，它會結束並保留結果 40，供 wait 回收。
也可先 `spawn worker`，用 `kill <剛取得的 PID>` 終止等待中的子行程，再 wait 取得 -9。

**先 wakeup 再 wait 等事件的 worker。** wait 是阻塞式前景等待；如果唯一能發出事件的 shell 自己也在 wait，就沒有行程可以喚醒 worker。本版未實作 Ctrl+C 取消前景工作。

## 真實 Pipe（行程間資料管道）

```text
writer | reader
ps
trace off
writer|reader
ps
trace on
```

核心建立兩個子行程及一條 16-byte 環形緩衝區。writer 寫入 80 bytes，reader 從管道讀出、收到 EOF（資料結束）後輸出：

```text
[reader] Hello through a real pipe: this message is longer than the 16-byte ring buffer.
[reader] EOF, bytes=80
```

trace on 時可看到 Waiting(pipe write／pipe read) 與 I/O ready（輸入輸出已就緒）的狀態轉換。
父行程等待並回收兩個子行程，顯示 pipeline complete 後提示字元恢復。可重複執行。
需要至少 2 個空閒 PCB 欄位；先回收其他 Zombie，或結束／回收既有 worker。

這版是固定內建 `writer | reader`，不是通用多段管線，不提供檔案重導向、pipe()/dup2() 或任意程式組合。

## 磁碟檔案系統

`make run` 會連接 `build/wuwaos.img` 這個 16 MiB QEMU 虛擬磁碟。首次使用會建立 WUFS（WUWAOS 自有檔案系統），並將當前核心 ELF 放在 `/boot/kernel.elf`。後續建置會更新這個檔案，同時保留其他使用者檔案。

```text
ls /
ls /boot
mkdir /home/user/notes
touch /home/user/notes/today.txt
write /home/user/notes/today.txt hello
cat /home/user/notes/today.txt
```

目錄採 Linux 熟悉的名稱，檔案系統格式是 WUFS，並非 Linux 可掛載的格式。`write` 會覆寫檔案內容，只接受一個不含空白的文字 token。磁碟映像在 QEMU 關閉後保留；`make clean` 不會刪除它。

`/boot/kernel.elf` 是磁碟內的核心副本；目前仍由 QEMU `-kernel` 直接載入執行，尚未由 bootloader 從磁碟開機。`/bin` 與 `/dev` 目前先建立為空目錄。

## 指令表

| 指令 | 功能 |
| --- | --- |
| help | 指令說明 |
| ps | 有效行程、父 PID、名稱、狀態、等待原因、空閒欄位與 FIFO 就緒佇列 |
| pcb <pid> | 特定 PCB 的實際保存資料 |
| spawn worker / spawn quick | 建立背景子行程 |
| fork | 複製 shell，顯示父子返回值與私有資料，子行程結束後等待回收 |
| run worker / run quick / run counter | fork 子行程後 exec 獨立工作映像；以 wait 回收 |
| yield | shell 主動讓出 CPU |
| wakeup <channel> | 喚醒事件通道等待者 |
| wait <pid> | 等待直接子行程並回收 |
| kill <pid> | 終止 shell 的直接子行程；不可終止 PID 1 或自己 |
| trace on / trace off | 開關子行程狀態轉換紀錄；shell 自身用 pcb 1 查閱 |
| writer \| reader | 前景執行固定雙行程管線 |
| ls <path> | 列出目錄內容 |
| mkdir <path> | 建立目錄 |
| touch <path> | 建立空檔案；檔案已存在時不清空 |
| write <path> <text> | 建立或覆寫檔案，文字不含空白 |
| cat <path> | 顯示檔案前 2048 bytes；若內容未以換行結尾，顯示後補換行 |
| fan <path> | 前景文字編輯器，Ctrl+O 儲存、Ctrl+X 離開並自動回收行程 |
| exit | 結束 shell |

輸入上限為 126 個內容字元加換行；超長行顯示錯誤，不執行截斷命令。只有固定命令與正整數參數，不支援引號、一般參數展開、背景 & 語法或主機命令。

## 架構與限制

- 最多 4 個行程，每個獨立 Sv39（三級分頁）頁表、16 KiB 使用者 code（程式碼）、4 KiB data（資料）、4 KiB stack（堆疊）及 16 KiB 核心堆疊。
- fork 採完整複製，不使用 COW（寫入時複製）。exec 動態準備新位址空間，切換頁表後釋放舊空間；只接受內建程式 ID，fan 可額外接收一個檔案路徑參數。
- PCB 是原本 Process 的延伸，沒有第二份重複的狀態。包含父子關係、等待資訊、管道端點與待完成 I/O。
- `processes[4]` 保存完整 PCB；`scheduler.c` 的單向鏈結 FIFO 只保存 Ready PCB 的指標，入列與取出隊首 O(1)，移除任意節點 O(P)。
- read/write、行程管理與查詢都透過使用者模式系統呼叫。核心與 UART（序列埠）頁面仍不可由使用者模式直接存取。
- 檔案系統經由 VirtIO-MMIO block driver（虛擬區塊裝置驅動）讀寫 QEMU 磁碟。UART、pipe 與磁碟檔案走不同 I/O 路徑；目前檔案呼叫以路徑為參數，尚未有通用檔案描述號或 POSIX `open/close`。
- 仍是合作式排程；shell 提示前讓出兩輪，讓本版內建 worker 到達穩定的等待點。不是時間片或 timer（計時器）搶占。
- 等待終端輸入與磁碟操作目前以輪詢進行；沒有多核心、計時器中斷、外部 ELF loader（執行檔載入器）、Linux 相容檔案系統或浮點／向量執行環境。
- 未加入測試框架或負面測試程式。實際互動驗證及未執行的邊界路徑詳見 spec。

## 文件與閱讀順序

- [SPEC.md](docs/SPEC.md)：fork／exec／回收里程碑規格與驗收紀錄；目前排程以 SPEC-07 為準。
- [CODE_GUIDE.md](docs/CODE_GUIDE.md)：所有程式碼功能、演算法與硬體關係。
- [SPEC-01](docs/SPEC-01-user-syscalls.md)、[SPEC-02](docs/SPEC-02-process-lifecycle.md)、[SPEC-03](docs/SPEC-03-shell-pipe.md)：使用者呼叫、行程生命週期與互動 shell／pipe 規格。
- [SPEC-04](docs/SPEC-04-boot-splash.md)：開機動畫規格與驗收紀錄。
- [SPEC-06](docs/SPEC-06-filesystem.md)：VirtIO 區塊 I/O、WUFS 與持久磁碟驗收紀錄。
- [SPEC-07](docs/SPEC-07-fifo-scheduler.md)：FIFO 就緒佇列、非搶占式排程與驗收紀錄。
- [SPEC-08](docs/SPEC-08-memory.md)：動態實體頁配置、三級分頁與回收。
- [SPEC-09](docs/SPEC-09-fan.md)：fan 編輯器、raw console 與路徑參數傳遞。

先讀 user/shell.c 的 shell_main／command，再追蹤 user/api.c → kernel/syscall.c → process.c 或 io.c，最後看 trap.S 如何交還使用者程式。
手動練習：對照 pcb 2 顯示的等待通道與 sepc（保存的指令位址），說明 wakeup 後為什麼從 sleep 呼叫後面繼續。
下一個小步驟可另訂 timer 中斷與前景工作取消的規格。
