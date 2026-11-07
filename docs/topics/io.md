# 08 · 輸入輸出與 Pipe

檔案系統處理名稱、目錄和資料放置；I/O system（輸入輸出系統）處理如何跟裝置交換資料。WUWAOS 提供 UART 終端、可選 GUI TTY、行程間管道與 VirtIO 區塊磁碟這幾條路徑。

## UART 終端

`drivers/uart/uart.c` 以 volatile 指標讀寫 `0x10000000` 的 UART（序列埠）裝置暫存器。輸出前輪詢是否可寫；讀取時檢查是否收到資料。`kernel/io/console.c` 負責輸出格式、路由與核心診斷，不直接操作 UART MMIO。volatile 保留硬體存取，不等於鎖或完整記憶體同步。

一般 shell 使用行輸入：收集字元、處理 Backspace，等 Enter 才完成 read。fan 使用 raw console（逐 byte、不回顯輸入），因此按方向鍵不必先按 Enter。

canonical line input 接受 Ctrl+D（ASCII EOT）作為 EOF：空行時 read 回 0；已有字元時先回傳目前字元，不附加換行，下次 read 再按 Ctrl+D 才回 EOF。這讓 `cat` 可由終端輸入文字並以 Ctrl+D 結束；raw mode 將 Ctrl+D 當一般輸入 byte。

raw console 同時只能有一個持有者；行程退出、被終止或成功 exec 時釋放。它是自訂介面，不是完整 POSIX termios。

## 圖形桌面 Terminal

有 VirtIO GPU、tablet 與 keyboard 時，kernel 桌面目前提供一個固定大小 Terminal 視窗。圖示開啟後建立另一個 shell process，其 FD 0/1/2 綁到 generation-checked GUI TTY endpoint；UART shell 綁到另一個固定 endpoint。每個 endpoint 各自持有輸入／輸出佇列、raw owner 與 CR/LF 狀態，避免 `fan` 或 `workstate` 讀取 GUI 按鍵時搶走 UART 輸入。Shell `ps`、`pcb`、`vmstat` 的核心列印依 fd 1 的 endpoint 暫時路由；開機與裝置診斷仍寫 UART。UI 目前仍只有一個 Terminal view，尚未提供多視窗。

TTY endpoint 是本專案自訂核心物件，不是 PTY master/slave ABI。`OFD_TTY` 持有 generation handle；dup/fork 共用 OFD 引用，session 結束後舊 handle 失效，所有 FD 引用釋放前不重用 endpoint slot，重用時 generation 遞增。圖示關閉只隱藏並保留 shell；`exit` 或空行 Ctrl+D 結束 shell。設計與 QEMU 驗收見 [Spec 30](../SPEC-30-graphical-terminal.md)。

核心 `kernel/io/io.c` 擁有 FD/OFD、pipe 與 I/O 路由；`kernel/io/tty.c` 擁有各 endpoint 的 queues、session refs、raw owner、CR/LF 狀態與輸出 consumer。註冊的 consumer 將 GUI console bytes 送到 `ui/terminal/vt.c` 更新 ANSI VT model，再由 `ui/terminal/renderer.c` 繪出字元與終端游標；pipe bytes 不經 GUI TTY。桌面視窗/合成在 `ui/desktop/desktop.c`，鍵盤映射在 `ui/input/keymap.c`；process loop 經 boot 註冊的 callback 輪詢 UI。這些仍在 S-mode 核心內執行。完整模組與資料通道見 [Spec 33](../SPEC-33-module-architecture.md) 及[程式碼架構圖](../CODE_GUIDE.md#ui-與-kernel-的資料通道)。

## 等待時資料放在哪裡

TTY 與 pipe 的阻塞讀寫保留在原 syscall/kernel stack：呼叫端先讀 event sequence，再嘗試操作；沒有資料或空間時登記該 generation/sequence 的 wait，切換出去。TTY/pipe 狀態改變時只增加 sequence 並喚醒 waiters；行程回到原 stack 後重試，並由 caller 執行 usercopy。Pipe 一次最多以 256-byte stack staging buffer 搬移；usercopy 成功後才更新 ring 索引並通知另一端，失敗則不提交該次 ring 變更。

`scheduler` 的 `io_poll()` 只輪詢 UART 並通知 TTY input event，不複製使用者記憶體，也不完成高層 pipe 操作。事件 wait 結果存於 PCB，因此 source 關閉或 slot 後續重用時，已喚醒的行程不會再解參考舊 source。kill 對仍持有 syscall stack 的行程延後到安全返回邊界清理；deadline/取消也透過同一 wait result 返回。這是目前單 hart、核心不可搶佔下的 caller-stack 設計，並非非同步裝置請求。

每個 OFD 的可重入 Gate 保護共享 file offset／資料操作；每個 pipe 的 Gate 保護 ring copy/commit。FS scope 會涵蓋 metadata／交易 staging。目前已接入的 Gate 順序為 OFD/Pipe → FS → VM；per-device Block Gate 尚未接入。scheduler/UI poll 是 no-wait context，競爭中的 Gate 不會讓 poller 睡眠；Block request 目前尚未接上 scheduler event poll。

沒有 Ready 行程而存在 event/UI poll work 時，scheduler 以有限的絕對 SBI timer deadline、SIE=0/STIE=1 與 WFI 等待；醒來後停止/確認 timer、輪詢並檢查 timeout。UART 仍由輪詢觀察，不是中斷驅動。事件框架與目前實作進度見[後續計畫](https://github.com/johnson931128/WUWAOS/blob/main/plan.md#2-統一事件等待與非同步-io)。

## 真實的位元組管道

每個行程有 8 格 FD table，初始 0／1／2 對應 console input／output／error；全域 16 個 OFD（開啟檔案描述）保存檔案 offset、pipe endpoint 或 GUI TTY handle。`dup`、`dup2` 與 fork 共用 OFD，因此共用檔案 offset／endpoint 引用；exec 保留描述，exit／kill 逐格關閉。固定 UART console sentinel 不占 OFD；GUI TTY 使用 OFD，fd 1／2 共用可寫描述。open 支援唯讀、唯寫、讀寫、建立、截斷與附加旗標；`O_APPEND` 每次寫入時取目前 EOF。WUFS 開啟中的 inode 若被 rm、rm tree、cp 覆寫或 truncate，會回 busy，避免開啟描述指向已回收或換代 inode。

每個 pipe 使用 256-byte ring buffer（環形緩衝區）。`read_at`、`write_at` 是讀寫索引，`count` 記錄目前資料量；索引前進至尾端後繞回。

管道滿時寫入者等待；reader 成功讀走資料後以 event 喚醒寫入者。空管道且仍有寫入者時，reader 等待；所有寫端關閉且資料讀完才得到 EOF（資料結束）。沒有讀取者時，寫入回報 `ERR_BROKEN_PIPE`。每次操作可短寫，呼叫端以回傳位元組數確認進度。

每 byte 入／出緩衝區 O(1)，搬移 N bytes 為 O(N)，另外可能包含多次阻塞與恢復。端點引用透過 OFD 引用計數在 dup／fork 繼承、close／exit／kill 關閉時更新；多餘端點關閉後才會出現 EOF 或 broken pipe。

## 區塊裝置的另一條路

```text
檔案操作 → fs.c → block.c → VirtIO queue → QEMU 磁碟
換頁操作 → swap.c → block.c → VirtIO queue → 獨立 Swap 磁碟
```

`drivers/virtio/block.c` 從 FDT 探索現代 VirtIO-MMIO 裝置，每顆磁碟有自己的請求與佇列狀態；初始化、transfer 與 flush 明確傳遞 `BlockDevice *`，不靠全域 active-device selector。以磁碟標頭辨識 WUFS 與 Swap，不依賴裝置順序。目前磁碟請求仍由呼叫者同步 busy-poll 完成；非同步請求與 scheduler completion poll 尚未實作，也沒有快取。

每個已提交的 512-byte 請求以硬體時間計數器計時，預設超過 5 秒且仍未完成，就印出裝置、角色、操作、磁區與等待時間，然後停止核心。逾時不是取消成功，因此不返回上層、不回收或重用請求緩衝區。門檻在 `drivers/virtio/block.h` 調整；一次頁面交換含八筆磁區請求，不共用一個 5 秒總預算。這是目前 WUWAOS 的停止策略，尚無裝置重設與恢復。見 [Spec 12](../SPEC-12-block-timeout.md)。

## 在 QEMU 觀察

```text
writer | reader
```

shell 支援 `echo hello | cat`、`ls | cat`、最多三階段、`<`、`>` 與 `>>`。舊 `writer | reader` 也由一般 parser 執行，讀到既有示範訊息與 EOF。`cat` 可讀檔案或 stdin；只在 console 輸出且末尾不是 LF 時補顯示用換行，pipe 和檔案輸出不改資料。直接執行的外部 ELF 不帶 argv，沿用 [Spec 26](../SPEC-26-elf-exec.md) ABI。每階段最多 8 個參數 token，整行最多 128 bytes；此為 WUWAOS 固定容量設計，不是完整 POSIX shell。

觀念參考：`notes/ch12  I_O Systems.md` 的裝置分層、輪詢與阻塞，`notes/ch3 Process.md` 的 fork 與繼承。現行 FD 與管線驗收見 [Spec 27](../SPEC-27-file-descriptors-and-shell-pipelines.md)，早期固定管線規格 [Spec 03](../SPEC-03-shell-pipe.md) 保留歷史。
