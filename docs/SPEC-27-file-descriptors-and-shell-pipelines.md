# Spec 27：檔案描述號、一般 I/O 與 Shell 管線

## 目標

將目前僅支援 stdin/stdout 與固定 `writer | reader` 的 I/O，擴充為每行程檔案描述號（FD）表與共用 open-file description，讓終端、一般 WUFS 檔案和管道使用同一組 `open/read/write/close/dup2/pipe` syscall。Shell 能將可串接命令以一般 argv 和多階段管線執行，並提供輸入及輸出重導向。

## 現況與限制

- `kernel/io.c` 以 PCB 的 `input_pipe`、`output_pipe` 指定固定端點，管線上限 2 個，只有 fd 0/1 可讀寫。
- `SYS_PIPELINE` 固定建立 `writer` 與 `reader`；shell parser 最多四個 token，不能表示一般命令階段。
- `kernel/fs.c` 有路徑讀取與整檔覆寫，沒有持續開啟的 inode handle 或 offset 寫入。
- `user/linker.ld` 固定 user code 16 KiB、data 4 KiB；shell ELF 已接近 code 上限。規格不放寬 ELF 固定位址或大小，必須把命令實作拆檔並使用既有 `-Oz`，建置時以 linker assertion 驗證。
- Spec 26 的外部 ELF ABI 入口仍為 `a0=0, a1=0`，無 `argc/argv`；本規格維持此行為，不宣稱一般 Linux 程式相容。

## 架構與演算法

1. 每個 PCB 有固定 8 格 FD table，0、1、2 初始分別為 console input、console output、console error。這三個保留為特殊 console 描述，不占全域 OFD 槽位；其他 FD table 項目指向核心全域 16 格 open-file description（OFD）表。open 與 pipe 使用最小可用 FD，所以 close 標準描述後可重用該槽。`dup`、`dup2` 與 `fork` 增加 OFD 引用，並共享同一檔案 offset。exec 保留 FD，exit、kill 與回收時逐一 close。描述號和 OFD 槽位不足回傳 `ERR_NO_SLOT`，無效或方向不符的 FD 回 `ERR_BAD_FD`。
2. OFD 類型為 console input/output、WUFS regular file、pipe read/write endpoint。`read/write` 依描述型別與權限處理，不因 FD 數字推定能力；console 行輸入與 raw 模式沿用既有行為。Pipe 使用固定 256-byte ring buffer，空讀且尚有 writer 時阻塞；無 writer 且緩衝資料已讀完回 EOF 0；滿寫時阻塞；無 reader 時回 `ERR_BROKEN_PIPE`。每次 open/dup/fork 均維持精確端點引用，最後一個端點關閉會喚醒等待者。
3. 檔案 open 支援 `O_RDONLY/O_WRONLY/O_RDWR` 及 `O_CREAT/O_TRUNC/O_APPEND`。OFD 持有 inode 編號及共享 offset；檔案 read/write 逐段以 offset 操作 WUFS，offset 溢位回錯誤；append 每次以當前檔案 EOF 為寫入起點，並更新共享 offset。開啟中的 inode 以核心引用計數保護；remove/remove_tree、覆寫已開啟目的檔的 fs_copy 與 truncate 均按 inode 編號檢查引用計數，遇到開啟 inode 回 `ERR_BUSY`。失敗的 open 不留 FD/OFD。首版採拒絕刪除，不做 unlink 後延遲釋放。
4. Shell 使用容量固定的 lexer/parser：最多 3 stages、每階段最多 8 個 argv、命令列 128 bytes。支援 `|`、`<`、`>`、`>>`。管線先建立每個相鄰階段的 pipe，再 fork 全部 stage、用 dup2 接上 stdin/stdout，並立即關閉 parent 與 child 的多餘端點；父 shell 之後等候全部 stage。建置／exec 診斷寫到 fd 2。任何中途錯誤均關閉已配置 FD、終止並回收已啟動子行程。
5. 可串接命令支援 `echo`、`cat`、`ls`、`pwd` 與無參數外部 ELF。`echo`/`cat` 使用通用 read/write FD API；`cat` 遇到終端輸出時可補顯示用換行，但 pipe／檔案輸出逐 byte 保留原資料。外部 ELF 仍不帶 argv，依 Spec 26 ABI 啟動。一般既有 shell builtins（例如 `cd`、`workstate`、`fan`、`sync`、檔案管理命令）保持原有互動方式；只允許明確列出的可串接命令出現在管線，其他命令回可讀錯誤，不在子行程中偷偷修改 shell 狀態。
6. 舊 `writer | reader` 語法仍可工作，但改由一般 parser 與管道執行路徑處理，移除對 `SYS_PIPELINE` 的 shell 依賴；其他公開 syscall 編號不變。新增 syscall 使用 35 以後的編號；舊 `SYS_PIPELINE` 編號保留但 syscall 回 `ERR_NO_SYSCALL`，明確標示 deprecated，不承諾其舊固定管線行為。`>` 開啟目的檔時使用 create/truncate，`>>` 使用 create/append；成功開啟後，即使後續 stage 建立失敗，目的檔建立或截斷副作用仍保留，shell 顯示該狀態。
7. 為符合固定 16 KiB shell code 區，既有 workstate UI 移至獨立內嵌 `PROGRAM_WORKSTATE` user image，由 `workstate` 命令 fork/exec/wait；它成為獨立行程並占一 PCB slot，CPU/RAM 統計仍按原快照 API 計算。啟動失敗顯示 syscall 錯誤，退出、kill 均經行程清理釋放 raw console ownership 並還原 ANSI 畫面。

## 工作範圍

- 修改 `Process` FD table、`kernel/io.c/.h`、`kernel/fs.c/.h`、`kernel/syscall.c`、`include/syscall_numbers.h` 與 `user/api.c/.h`。
- 將 shell parser 與可串接命令拆分至獨立使用者程式檔，避免 16 KiB ELF code 區溢位；沿用原固定 VM 區域及 ELF 格式。
- 更新 `docs/CODE_GUIDE.md`、I/O 與 filesystem 主題、使用說明及本規格驗收紀錄；網站文章只連結 GitHub 原始碼，不內嵌 OS source；README 保持英文。
- 不加入測試程式碼，不更動無關的 fan、workstate、sync、VM、ELF 或使用者原磁碟映像。

## 執行步驟

1. 確認目前分支、工作樹、建置環境及 docs 發佈工具；按本規格加入 FD/OFD 與 WUFS offset I/O。
2. 依序接入 console/pipe/file syscall、FD 生命週期與 fork/exec/exit/kill 清理。
3. 拆分 shell 命令程式，加入 parser、stage 啟動、管線與重導向；處理語法錯誤、FD 耗盡、建立中途失敗與 wait 回收。
4. WSL 完整建置；使用獨立 scratch WUFS/swap 磁碟於 QEMU 互動驗收，不讀寫 `build/wuwaos.img`。
5. 驗收後更新文件並建置網站，確認頁面連結和限制說明；只提交本工作範圍的變更，commit/push main，最後執行 `tools/publish-docs.ps1` 發佈 GitHub Pages。

## 驗收條件

- 建置通過且 shell/app/fan ELF 固定區段未超限；主機原有 `build/wuwaos.img` 雜湊不變。
- QEMU 中驗收 `echo`、檔案 `cat`、`echo ... | cat`、至少三階段管線、`<`、`>`、`>>`，資料內容與共享 offset 正確。
- 驗收 pipe 緩衝區超過容量時會阻塞並續傳、正常 EOF、讀端先關閉時寫入得 `ERR_BROKEN_PIPE`、子行程結束及 kill 會釋放端點並喚醒另一端。
- `cat` 無參數從 console stdin 讀取；輸入 Ctrl+D 以 EOF 結束（若行內已有字元，先回傳該段文字），raw mode 不將 Ctrl+D 特殊解讀。console 輸出的 cat 在末尾補顯示換行，pipe／檔案輸出不附加換行。
- 驗收 fork 後的 dup/shared offset、exec 保留 FD、一般 close/重複 close、無效 FD、open/FD/OFD/pipe 槽位不足與開啟中文件刪除 busy 錯誤；shell 在錯誤後仍可執行命令。
- 舊 builtins、`run worker/quick/counter`、`run <path>`、`writer | reader`、fan、workstate、demo sync 與既有 FS 命令可用。
- workstate 在 QEMU 中以獨立 PID 顯示，驗收 n/p 換頁、q 離開、kill 清理 raw mode；PCB 滿額時顯示可讀 fork 錯誤並保留 shell。
- 記錄實際執行的互動驗收項目及未驗收邊界；不得以靜態檢查替代 QEMU 結果。

## 驗收紀錄與限制

WSL Clang／LLD 完整建置通過。`user.elf` text 16310 bytes、低於 16384-byte linker 上限；`app.elf` 1793、`fan.elf` 8080、`workstate.elf` 4576 bytes，均通過相同 linker assertion。沒有新增測試程式碼。QEMU 使用獨立 `build/luna-fd-check.img` 與 `build/luna-fd-swap.img`，未掛載 `build/wuwaos.img`。

互動驗收：單行 `echo`、有引號參數、兩階段及三階段 echo/cat；檔案 `>` 建立／覆寫、`>>` 附加、`<` 讀入、檔案至 pipe 再寫檔；`ls | cat`、`pwd | cat`、相對 CWD 重導向；舊 `writer | reader` 經 pipe 正常輸出 80 bytes 並得到 EOF。將 1024-byte host text 匯入 scratch WUFS，執行三階段 `cat large.txt | cat | cat > large-copy.txt`，停機後讀取 WUFS 原始資料比較，source/copy 均 1024 bytes、完全一致且與 host source 相同。`cat large.txt | echo done` 觀察到讀端退出後 writer 收到 -32；其後 `ps` 僅 PID 1。利用三階段多個 input/output 重導向用盡 shell FD 槽，第二次 output open 回 -11；下一條一般管線仍成功，`ps` 僅 PID 1、free slots 15。shell 的 `cat` stdin 鍵入文字後 Ctrl+D 回傳該段內容，再次 Ctrl+D 得 EOF 並返回提示字元。`workstate` 以 PID 23 執行，按 q 返回並回收；`fan` 空檔 Ctrl+X、`demo sync`、相對工作目錄及既有 shell 命令亦成功。scratch QEMU 結束後 `build/wuwaos.img` SHA-256 維持為初始值 `F4F3E727EF15E62448D36968FD62DE9E77191B83145AE12FD039434BD7D70EE0`。

未直接操作驗收：一般 `dup`/`dup2` 的共享 offset 數值、fork 後共享檔案 offset、雙重 close／非法 FD 錯誤、開啟中 inode 的 rm/truncate/fs_copy busy、全域 OFD/pipe 表耗盡、process table 滿額啟動 workstate、kill 一個阻塞中的管線端點與 workstate n/p 換頁鍵。可由 syscall／清理路徑與靜態檢查佐證部分契約，但不宣稱上述互動已實測。外部 ELF 維持 Spec 26 無 argv；`SYS_PIPELINE` 編號 13 保留但回 `ERR_NO_SYSCALL`。無完整 shell quoting/escaping、seek、job control 或 POSIX 相容。

## 參考

作業系統觀念參考 `notes/ch12  I_O Systems.md` 的裝置分層、輪詢與阻塞；行程繼承參考 `notes/ch3 Process.md`。以上是教科書概念在 WUWAOS 自訂 ABI、固定容量表格與單核心核心中的具體設計，不是 POSIX 相容承諾。
