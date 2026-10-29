# WUWAOS

WUWAOS 是以 C 與 RISC-V 組合語言開發的小型作業系統，能在 QEMU 模擬的 RISC-V 64-bit 電腦上啟動。它用來學習作業系統如何管理行程、排程、記憶體、同步工具及檔案系統。

**[閱讀實作文件網站](https://johnson931128.github.io/WUWAOS/)** · [程式碼與架構導覽](docs/CODE_GUIDE.md) · [規格與驗收紀錄](docs/history.md)

## 目前實作了什麼？

| 主題 | 目前功能 |
| --- | --- |
| 開機與執行 | QEMU `virt`、RISC-V 64-bit、單核心、128 MiB RAM；OpenSBI 啟動 S-mode（監督模式）核心 |
| 使用者程式 | U-mode（使用者模式）、syscall（系統呼叫）、互動式 shell |
| 行程與排程 | PCB（行程控制區塊）、fork／內建程式 exec／wait／exit、計時器中斷及 50 ms RR（輪詢排程） |
| 同步與通訊 | 自製 Mutex（互斥鎖）、Semaphore（號誌）、等待／喚醒、固定的 `writer \| reader` 資料管道 |
| 記憶體 | Sv39 三級頁表、4 KiB 分頁、按需分頁、FIFO 頁面置換與獨立 swap（交換區） |
| 檔案系統 | VirtIO 區塊裝置、自訂 WUFS、目錄與檔案讀寫，關閉 QEMU 後資料保留 |
| 互動工具 | `fan` 文字編輯器、`workstate` 即時監測、`demo sync` 同步示範 |

這是教學用途的小型系統，**不是 Linux 相容系統**。目前限制與未完成項目見[系統邊界](#系統邊界與後續方向)。

## 啟動 WUWAOS

以下以 Windows PowerShell、WSL Ubuntu 和此專案路徑為例。WSL 內需要 Clang、LLD、llvm-objcopy、Make、Python 3、QEMU RISC-V 和 OpenSBI。已記錄的驗證環境為 Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3；其他版本需視環境確認。

在 PowerShell 執行：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

`make run` 會先由主機的 Clang **交叉編譯** RISC-V 機器碼，再啟動 QEMU；沒有變動的檔案通常不會重新編譯。開機後會看到 WUWAOS 字樣及 shell 提示字元：

```text
WUWAOS user shell. Type help. State tracing is on.
wuwaos/>
```

接下來的 `help`、`ls`、`demo sync` 等命令是在 **WUWAOS 的 `wuwaos/>` 提示字元**輸入，不是在 PowerShell 輸入。提示字元會顯示目前目錄，例如 `wuwaos/home/user>`。

退出 QEMU：先按 **Ctrl+A**，放開後按 **X**。`exit` 只結束 shell，不會關閉 QEMU。

## 快速體驗

以下範例逐行輸入；行程 PID（行程編號）每次可能不同，請以畫面顯示的數字為準。

### 看可用指令與系統狀態

```text
help
ps
workstate
```

`help` 列出命令；`ps` 列出行程狀態；`workstate` 顯示每秒更新的行程 CPU／記憶體資訊，按 **q** 回到 shell。CPU% 是 WUWAOS 行程的派發時間比例，不是 Windows 主機的 CPU 使用率。詳見 [workstate](docs/topics/monitoring.md)。

### 建立檔案並編輯

```text
cd
mkdir notes
cd notes
touch today.txt
write today.txt hello_WUWAOS
cat today.txt
fan today.txt
```

`cd` 進入 `/home/user`；`pwd` 可顯示目前目錄，`cd ..` 回上一層，`cd -` 返回前一個目錄。`write` 覆寫檔案，只接受一個不含空白的文字參數；要編輯空白或多行文字，使用 `fan`。

`fan` 中按 **Ctrl+O** 儲存、**Ctrl+X** 離開，再以 `cat today.txt` 讀回。fan 目前支援最多 2048 bytes 的 ASCII／LF 文字，建議終端至少 80×24。`mkdir` 若回 `-17`，代表名稱可能已存在；可用 `ls` 查看。

刪除一般檔案使用 `rm today.txt`。刪除資料夾例如 `rm notes` 時，會警告並要求輸入 `Y`／`N`；選 Y 後會刪除該資料夾及所有內容。根目錄與活躍行程正在使用的工作目錄受到保護。WUFS 沒有交易日誌，磁碟出錯時遞迴刪除可能只完成一部分。

### 觀察同步工具

```text
demo sync
```

父行程先持有 Mutex；子行程嘗試取得時會等待。父行程釋放後，子行程取得並釋放鎖，再等待初始值為 0 的 Semaphore；父行程 `post` 後，子行程繼續並被回收。過程會顯示兩次真實 `ps` 快照；繁忙時不保證每次快照剛好看到等待狀態。範例以終端輸出展示臨界區域，沒有使用共享記憶體。[示範說明](docs/SPEC-23-sync-demo.md) · [同步工具設計](docs/SPEC-22-synchronization.md)

### 觀察行程生命週期

```text
spawn worker
ps
pcb <PID>
wakeup 7
wait <PID>
```

將 `<PID>` 換成 `spawn worker` 印出的數字。worker 會先等待通道 7，`wakeup 7` 後退出，`wait` 回收。**先 wakeup，再 wait**；若 shell 先阻塞等待，而沒有其他行程能喚醒 worker，操作就會停住。`run counter` 則示範 fork → exec，使用 `wakeup 9` 和 `wait <PID>` 回收。

### 管道與虛擬記憶體

```text
writer | reader
vmstat
```

前者執行固定的雙行程位元組管道；後者顯示缺頁與 swap 統計。可透過 `vmstat limit <frames>` 和 `mem alloc`／`write`／`read`／`free` 觀察按需分頁及 FIFO 置換，完整步驟與限制見 [記憶體章節](docs/topics/paging.md)。

## 命令速查

| 命令 | 用途 |
| --- | --- |
| `help`、`ps`、`pcb <pid>`、`workstate` | 指令說明、行程與資源觀察 |
| `spawn worker\|quick`、`fork`、`run worker\|quick\|counter` | 建立行程或執行內建程式 |
| `yield`、`wakeup <channel>`、`wait <pid>`、`kill <pid>` | 讓出 CPU、喚醒、回收或終止直接子行程 |
| `trace on\|off`、`writer \| reader` | 控制狀態追蹤、執行固定管道 |
| `demo sync` | 觀察父子行程使用 Mutex 和 Semaphore |
| `sync mutex create`／`lock <id>`／`unlock <id>` | 建立及操作互斥鎖；完整命令須在 lock／unlock 前加上 `sync mutex` |
| `sync sem create <n>`／`wait <id>`／`post <id>` | 建立及操作號誌；完整命令須在 wait／post 前加上 `sync sem` |
| `sync destroy <id>`、`syncbg ...` | 銷毀閒置物件，或在子行程操作同步物件；詳見 `help` |
| `pwd`、`cd [path]`、`ls [path]` | 查看及切換目錄、列出目錄內容 |
| `mkdir <path>`、`touch <path>`、`rm <path>`、`cp <source> <destination>` | 建立目錄／檔案、刪除或複製 |
| `write <path> <text>`、`cat <path>`、`fan <path>` | 覆寫、讀取及編輯檔案 |
| `cls` | 清除畫面並要求終端清除捲動歷史；效果視終端支援而定 |
| `vmstat`、`mem ...` | 查看與操作虛擬記憶體 |
| `exit` | 結束 shell；不會關閉 QEMU |

檔案指令支援絕對和相對路徑。輸入不支援引號、一般背景 `&`、任意程式管線或主機命令。同步操作可能阻塞：如果沒有其他行程能解鎖或 post，shell 會等待；目前沒有前景 Ctrl+C 取消功能。

## 系統架構

```text
PowerShell → WSL 的 Clang／Make → 已編譯的 RISC-V 核心
  → QEMU 虛擬電腦 → OpenSBI → WUWAOS 核心 → 使用者模式 shell
      → syscall（系統呼叫）→ 行程／排程／同步／記憶體／WUFS
      → UART 終端及 VirtIO 虛擬磁碟
```

核心有獨立排程器堆疊，使用 RISC-V 組合語言保存與恢復核心上下文。使用者行程可被計時器搶占；核心內不搶占，長時間 syscall 或同步磁碟 I/O 可能延後切換。[行程管理](docs/topics/processes.md) · [RR 排程](docs/topics/scheduling.md) · [虛擬記憶體](docs/topics/paging.md) · [WUFS](docs/topics/filesystem.md)

`make run` 使用 `build/wuwaos.img`（WUFS）及 `build/wuwaos-swap.img`（獨立交換區）；首次建立，後續保留使用者檔案。`make clean` 不會刪除磁碟映像。`/boot/kernel.elf` 是磁碟內的核心副本，**目前 QEMU 仍使用 `-kernel` 直接載入核心**，尚未從該副本開機。

## 系統邊界與後續方向

- 最多 4 個行程；每個行程的使用者程式碼區 16 KiB、資料區與堆疊各 4 KiB，heap 最多 1 MiB。fork 複製私有資料，尚無 COW（寫入時複製）或共享記憶體。
- 自製 Mutex／Semaphore 目前依賴單核心、核心內不搶占；同步物件最多 8 個。持鎖者離開時 Mutex 會標記資料可能不一致；號誌名額不會因行程結束自動歸還。
- WUFS 是 WUWAOS 自訂格式，不是可直接掛載的 Linux 檔案系統；尚無完整 POSIX 檔案描述號介面、權限、journal（交易日誌）或一般檔案重導向。
- shell 的 exec 目前載入內建程式映像，沒有一般磁碟 ELF 載入器、PATH 搜尋或 WUWAOS 內的 C 編譯器。未來在系統內編譯與執行 C 的規劃見 [Spec 21](docs/SPEC-21-native-compilation.md)。
- 尚無多核心、優先權排程、網路、圖形桌面或完整終端工作控制。各規格的驗收紀錄標明當時實際驗證過的範圍。

## 文件與網站

- [文件網站](https://johnson931128.github.io/WUWAOS/)：依開機、行程、排程、記憶體、I/O、檔案系統及應用程式分類。
- [CODE_GUIDE](docs/CODE_GUIDE.md)：程式檔案職責與架構。
- [規格與驗收索引](docs/history.md)：各階段設計、驗證與限制；舊規格保留當時背景。
- [notes/](notes/)：使用者提供的作業系統課程筆記。

在 Windows 主機預覽文件網站：

```powershell
npm.cmd ci
npm.cmd run docs:dev
```

以終端顯示的 Local 網址開啟；`npm.cmd run docs:build` 可建置網站。網站僅閱讀與搜尋，OS 原始碼以 GitHub 連結提供；維護與發布方式見 [網站維護文件](docs/topics/writing.md)。
