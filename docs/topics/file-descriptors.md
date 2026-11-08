# 08 · 檔案描述號與共用位置

`fd` 是行程手上的小整數；它不是檔案本身，也不直接保存讀到哪裡。WUWAOS 透過核心的 open-file description（OFD）把 FD 連到檔案、存取權限與目前 offset。分清這兩層，就能說明為何 `dup` 和 `fork` 後的讀取會互相推進位置，而對同一路徑再次 `open` 則不會。

<script setup>
import FdOfdLab from '../.vitepress/theme/components/FdOfdLab.vue'
</script>

<FdOfdLab />

## 教科書概念與本機實作

課程筆記以 PCB／行程描述作業系統追蹤已開啟檔案的方式，並介紹 `fork` 後子行程繼承開啟檔案；檔案系統介面章則說明 open、read、write、close 這類操作如何讓使用者程式透過系統呼叫操作檔案。可參考使用者提供的 [`notes/ch3 Process.md`](https://github.com/johnson931128/WUWAOS/blob/main/notes/ch3%20Process.md)（PCB 與 fork 段落）以及 [`notes/Chapter 13 file system Interface.md`](https://github.com/johnson931128/WUWAOS/blob/main/notes/Chapter%2013%20file%20system%20Interface.md)（檔案操作介面）。

那些筆記是作業系統的一般觀念；它們不代表 WUWAOS 使用 Linux 的資料結構或完整 POSIX ABI。以下容量、特殊值、錯誤碼與實際路徑均以本專案目前原始碼為準。

## 兩層資料結構

| 層次 | 保存的資料 | 本專案的容量／生命週期 |
| --- | --- | --- |
| FD table | 每個 `Process` 各有 8 格整數表。合法 FD 是 `0` 到 `7`；空格記成 `-99`。 | `PROCESS_FD_LIMIT` 為 8。`open`、`dup`、`pipe` 會尋找可用格；close 後可再用。 |
| OFD | 核心全域 `open_files` 表中的物件，保存類型、讀寫權限、inode、append 設定、引用數及檔案 offset。 | `OFD_LIMIT` 為 16。最後一個 FD 引用釋放後，該 OFD 槽位清空，之後可重用。 |

一般檔案 FD table 項目存放 OFD 表索引。不同 process 的 FD 數字可以各自指到相同 OFD；相同的 FD 數字也不保證指向相同 OFD。`OFD_FILE` 的 `offset` 屬於 OFD，而非 FD 或 inode，因此多個 FD 共享同一 OFD 時也共享讀寫位置。

UART 初始的 fd 0、1、2 分別存 `-1`、`-2`、`-3` sentinel，核心據此路由輸入、輸出與錯誤輸出；它們不是 `open_files` 中的 OFD，不消耗 16 個全域槽位。GUI Terminal 的配置不同：核心建立兩個 TTY OFD，fd 0 指向可讀 input OFD，fd 1 和 fd 2 指向同一個可寫 output OFD，因此 output refs 是 2。`refs` 是 FD 引用總數，按每個 process table 中的 FD 項目計數，不按不同 process 數計數。

## open、dup 與 fork 的位置差異

互動模型預設展示一段完整序列：開啟 `story.txt` 得到 OFD A；讀 `AB`；`dup` 得 fd 4；再從 fd 3、fd 4 分別讀到 `C`、`D`；`fork` 後子行程的兩個 FD 繼續指向 A；接著從子行程讀取 `E`。父行程再對同一個路徑 `open`，得到新的 OFD B 和獨立 offset。最後逐一 close，直到 A 的 refs 歸零。

序列中 A 在 fork 前 refs=2（fd 3、4），fork 後 refs=4（父、子各兩個 FD）。fork 增加兩個 OFD 引用，沒有再建立 OFD。第二次 open 建立 B，refs=1、offset=0；從 B 讀取不移動 A 的 offset。close 只減少該 FD 的一個引用；A 在最後一個引用關閉時才回收，B 仍可獨立存活。

`io_read` 與 `io_write` 在操作一般檔案時，先取得對應 OFD 的 `KernelGate`，再讀寫 inode、更新同一個 OFD 的 offset，最後離開 gate。共享 FD 的競爭操作因此依序使用和更新 offset，避免兩個 caller 以相同舊位置重複讀寫。gate 只保護核心實際執行的共享 OFD 路徑；本互動模型不模擬排程時間或磁碟延遲。

## 欄位與 syscall 參數

| 名稱 | 作用 |
| --- | --- |
| `Process.fd_table[8]` | 每行程的 FD → OFD 索引；負 sentinel 用於 UART，`-99` 表示空格。 |
| `OpenFile.type` | 區分保留配置、一般檔案、pipe 端點及 TTY。 |
| `OpenFile.refs` | 指向這個 OFD 的 FD 項目數；`dup`、fork 繼承會增加，close 會減少。 |
| `OpenFile.readable` / `writable` | 由 open flags 或 TTY/pipe 端點建立的讀寫能力。方向不允許時回 `ERR_BAD_FD`。 |
| `OpenFile.inode` | WUFS inode 編號；同一路徑的不同 open 可以各自建立 OFD，但指向同一 inode。 |
| `OpenFile.offset` | 一般檔案的目前位置；同一 OFD 的 FD 共享此值，新的 open 會初始化自己的值。 |
| `OpenFile.append` | append 模式下，每次寫入以當前檔案 EOF 作為寫入起點。 |
| `open(path, flags)` | `path` 為使用者路徑；flags 支援 `FD_O_RDONLY`、`FD_O_WRONLY`、`FD_O_RDWR`、`FD_O_CREAT`、`FD_O_TRUNC`、`FD_O_APPEND`。 |
| `read(fd, buffer, length)` | 從 FD 指向的物件讀入最多 `length` bytes 至使用者 buffer。 |
| `write(fd, buffer, length)` | 從使用者 buffer 寫出最多 `length` bytes。 |
| `dup(oldfd)` | 複製到最小可用 FD，指向與 `oldfd` 相同的物件。 |
| `dup2(oldfd, newfd)` | 先替換 `newfd` 原有內容，再令它指向 `oldfd` 的物件；若兩者相同，直接回傳該 FD。 |
| `close(fd)` | 移除這個 process 的 FD 引用；只有最後一個 OFD 引用關閉才回收物件。 |

目前核心 I/O 介面的 C 函式簽名如下。`buffer` 是使用者虛擬位址，syscall 進入核心後會檢查相應的讀寫權限，再經 VM copy helper 存取資料。

```c
long io_open(const char *path, long flags);
long io_read(long fd, uintptr_t buffer, size_t length);
long io_write(long fd, uintptr_t buffer, size_t length);
long io_dup(long oldfd);
long io_dup2(long oldfd, long newfd);
long io_close(long fd);
```

存取模式常數為 `FD_O_RDONLY=0x01`、`FD_O_WRONLY=0x02`、`FD_O_RDWR=0x03`；附加旗標為 `FD_O_CREAT=0x10`、`FD_O_TRUNC=0x20`、`FD_O_APPEND=0x40`。它們是 WUWAOS 自訂值，不直接宣稱與宿主 OS 的旗標 ABI 相同。

`open`、`read`、`write`、`dup`、`dup2`、`close` 成功時各自回傳新 FD、實際讀寫 byte 數、目的 FD 或 `0`；失敗時回傳負錯誤碼。無空 FD/OFD 槽位回 `ERR_NO_SLOT`（-11）；無效或不允許方向的 FD 回 `ERR_BAD_FD`（-9）。部分檔案 I/O 依目前 staging 上限逐次處理，單次一般檔案讀寫最多 512 bytes。

## WUWAOS 的實作入口

| 工作 | 原始碼入口 |
| --- | --- |
| FD table 與 Process 欄位 | [`kernel/proc/process.h`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.h#L18-L46) |
| OFD 結構、全域容量、sentinel 與引用回收 | [`kernel/io/io.c`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/io/io.c#L10-L119) |
| fork 時繼承 FD 並按每個有效 FD 增加 refs | [`io_inherit`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/io/io.c#L125-L131)、[`process_fork`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c#L355-L381) |
| GUI 的 input/output OFD 配置 | [`io_attach_tty`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/io/io.c#L137-L149) |
| `open`、`close`、`dup`、`dup2` | [`kernel/io/io.c`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/io/io.c#L212-L282) |
| 檔案讀寫與 OFD gate／offset 更新 | [`io_read` / `io_write`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/io/io.c#L422-L511) |
| syscall 編號與 open flags | [`include/syscall_numbers.h`](https://github.com/johnson931128/WUWAOS/blob/main/include/syscall_numbers.h#L35-L53) |
| syscall 參數從 trap frame 傳給 I/O 實作 | [`kernel/syscall/syscall.c`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/syscall/syscall.c#L64-L116) |

這些連結對照本專案目前 `main` 原始碼。互動圖則是依 source 建立的固定示意，沒有即時讀取核心狀態，也沒有模擬完整 pipe 或使用者程式執行。
