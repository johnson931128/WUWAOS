# Spec 33：核心模組分層與圖形終端解耦

日期：2026-11-07。

狀態：已實作並完成列出的建置、靜態檢查與 QEMU 驗收；限制見下方紀錄。

## 目標與範圍

在不改變 WUWAOS 現有功能與執行邊界的前提下，依職責整理 source tree，特別拆開 GUI、kernel TTY/I/O、process 與裝置驅動的依賴。維持單一固定 Terminal 視窗、既有 UART shell、系統呼叫 ABI、WUFS 格式、使用者程式連結內容、kernel/user 容量界線及現有命令和啟動方式。

本工作不新增視窗或 UI 功能，不搬移 UI 至 U-mode，不提供 POSIX PTY/window manager，也不改變排程或輸入輸出的既有可觀察順序。規格會以模組化 source tree 敘述目前 WUWAOS；不是動態載入核心模組、微核心或教科書中概念架構的宣稱。

## 目標目錄

| 區域 | 職責 | 主要來源 |
| --- | --- | --- |
| `arch/riscv/` | 啟動、陷阱、上下文切換、SBI、計時與 linker | `entry.S`、`trap.S/C/H`、`swtch.S`、`context.h`、`sbi`、`timer`、`clock`、`linker.ld` |
| `drivers/uart/` | UART MMIO 操作 | `uart.c/h` |
| `drivers/virtio/` | VirtIO block、GPU、input 裝置 | `block`、`gpu`、`input` |
| `kernel/boot/` | 核心啟動與模組組裝 | `main.c` |
| `kernel/proc/` | 行程生命週期、排程、資源監測、窄 UI-facing service/event hook API | `process`、`scheduler`、`workstate`、`terminal_service.h`、`event_hooks.h` |
| `kernel/mm/` | 頁面、虛擬記憶體與 swap | `page`、`vm`、`swap` |
| `kernel/fs/` | WUFS 和路徑查找 | `fs`、`path` |
| `kernel/io/` | 檔案描述符、核心 console、TTY | `io`、`console`、`tty` |
| `kernel/ipc/` | Mutex、Semaphore | `sync` |
| `kernel/syscall/`、`kernel/exec/`、`kernel/lib/` | syscall 分派、ELF/user image、核心記憶體函式 | 各自同名來源 |
| `ui/` | 核心內的桌面/視窗、VT model/render、圖形 primitive/font、鍵盤映射、splash | `desktop`、`terminal/vt`、`terminal/renderer`、`graphics/primitives`、`input/keymap`、`assets`、`boot/splash` |
| `user/lib/`、`user/shell/`、`user/apps/` | 使用者 ABI/輸出函式庫、shell、互動程式 | `api`/`print`/entry、shell main/shell_exec/sync_demo、app demo、fan、workstate |
| `include/` | ABI、配置與各模組公開契約 | 保留目前公開 headers，按穩定用途整理 |

精確檔名允許在實作時依現有介面調整；最終文檔必須與真實路徑一致。`kernel/desktop.c` 的整理必須拆成真實的責任模組，而非單純搬檔：圖形 primitives/font、桌布/視窗框/滑鼠游標/dirty composition、ANSI VT terminal model、keyboard mapping/input translation 分開。Renderer 僅繪製 terminal cells/characters/terminal cursor，不承接整個 desktop window composition。

## 模組邊界與互動

### TTY 與 UI

核心 `kernel/io/tty` 擁有 GUI 端點的 input/output rings 與 session 狀態。既有 CR/LF 與 raw-owner 狀態留在 `kernel/io/io`，不因分類搬動而改變。GUI 關閉只隱藏視窗，不終止 session。建立新 GUI shell 時初始化新 session；session leader 結束時由核心 I/O/TTY 清除 session 並將 ANSI alternate-screen/游標狀態復原。Process cleanup 不呼叫 UI 繪圖或 renderer。

TTY 提供窄介面：GUI session start/end/query、輸入 bytes push/read、輸出 bytes enqueue，以及註冊輸出 consumer。Desktop input adapter 將裝置事件映射為鍵盤字元，再透過 TTY push 輸入；核心 IO 只呼叫 TTY。TTY 輸出 consumer 在 boot 組裝時註冊為 UI VT feed/dirty callback。輸出 callback 將 bytes 餵入 ANSI VT model；VT model 持有 parser、cursor、色彩及 normal/alternate screen 狀態，不 include GPU、driver、process 或 TTY headers。Renderer 僅讀取 VT model 繪製 cell/字元/terminal cursor；window frame、wallpaper、mouse cursor 與全畫面 dirty composition 留在 desktop/window 模組，不得由 renderer 維護另一份 terminal state。

GUI 視窗啟動經 kernel-owned 的 `kernel/proc/terminal_service.h` 請求 `process_start_gui_terminal`；UI include 此公開核心服務 API，不暴露 Process 欄位。Process 不 include UI headers，也不直接呼叫 `desktop_*`；boot/main 是唯一裝配 UI callbacks/服務的位置。GUI session 的端點識別與 raw ownership 留在 kernel I/O，IO/console/process 不 include UI desktop headers。

### Scheduler 與事件來源

由 boot 裝配點註冊單一 UI `poll` 與 `active` callback 給 process event loop（目前 scheduler loop 實際位於 `process.c`）。保持目前 `process_resume` 與 scheduler loop 中的 `io_poll`、UI poll 相對順序及 idle 判斷；當 `current_index == -1` 時不得執行一般 syscall 或以不存在的 current process 存取 UI。此 callback 只抽離 UI 耦合，不增加新的事件來源或排程政策。

### Input 與裝置

VirtIO input driver 僅回報標準化事件，不 include GPU/UI headers。Pointer normalization 的最大座標透過 `input_set_pointer_bounds(max_x, max_y)` 注入；GPU bounds 由 boot/UI 配置。保留原 normalization 演算法和畫面 pixel 結果。

UART MMIO 移入 `drivers/uart`；kernel console 保留 routing、格式化與 log 行為，透過明確 UART API 操作裝置。其他 drivers 可繼續使用明確的 allocator/logging 介面。

## 建置與相容

Makefile 使用明確的模組 source/object 路徑，最多保留 `-I.` 與 `-Iinclude`；不得把所有子目錄加入搜尋路徑，模組 include 使用可辨識的完整相對路徑。Nested dependency files 必須支援增量建置；clean 只刪除已知的 object/dependency/ELF/BIN build products（包含新 nested objects/deps 及舊 flat objects），不得遞迴刪除 `build/`，避免誤刪 disk images、swap、logs 或其他 scratch artifacts。維持 `build/kernel.elf`、`build/user.elf`、`build/app.elf`、`build/fan.elf`、`build/workstate.elf` 與 bins 名稱、連結順序、`-Oz`、run/run-gui 與 launcher targets。不得放寬 16 KiB user code 與 `0x80400000` kernel budget。

更新 `tools/make_desktop_font.ps1` 的預設輸出位置；更新 README、code guide 與當前架構 docs 的 GitHub source links。歷史規格保留原路徑與驗收文字；其中舊 `kernel/`、`user/` GitHub source links 固定到 refactor 前 commit `800a805`，不再連向不存在的舊 main 路徑。新增 modules 不更動 WUFS image format 或 package/archive entry behavior。

## 驗收條件

- 靜態檢查確認核心 IO/console/process 不 include `ui` headers，也不呼叫 `desktop_*`；VT model 無 GPU/driver/process/TTY include；input driver 無 GPU/UI include。
- 乾淨建置與再次增量建置均成功；clean 不留可誤用的 flat objects。既有輸出 ELF/BIN 名稱與旗標不變，使用者 binary 可逐位元比對；kernel 未超出原 linker budget。
- 使用獨立 scratch disk/swap 進行 UART 與 GUI QEMU 驗收：GUI shell 輸入輸出及 QMP 鍵盤/截圖、`fan` alternate-screen save/exit、`workstate` 的 `q`、GUI 隱藏/重開沿用原 session、`exit` 後新 session、無 GUI UART shell、pipe/redirection、WUFS 持久性。避免將新增測試程式碼或測試框架納入 repository。
- 文件網站建置通過，活躍程式碼連結對應到新路徑；原始 `build/wuwaos.img` 與 swap 在驗收前後 SHA-256 相同。

## 理論參考與實作區分

模組化核心邊界的概念可參考使用者筆記 `notes/ch2 Operating-System Structures.md`「作業系統服務與命令直譯器」；核心透過 I/O 服務隔離程式與裝置的概念可參考 `notes/ch12  I_O Systems.md`。這些筆記提供教科書觀念；WUWAOS 仍是單一靜態連結的 S-mode kernel，其 user shell 和應用程式以既有方式連結，UI 仍在 kernel 內執行。

## 驗收紀錄

- `make clean && make -j2 all build/user.bin build/app.bin build/fan.bin build/workstate.bin build/kernel-disk.elf` 成功；後續增量建置無需重建。既有 user/app/fan/workstate BIN 與重構前 SHA-256 完全相同；kernel linker 成功，未調高既有 budget。完整輸出在忽略的 `build/architecture-clean-build.log`。
- QMP 擷取重構前後 desktop 與初開 Terminal PPM，兩組 SHA-256 各自完全相同。QEMU GUI 驗收包含 shell help/pipe/redirection、Fan 儲存後離開、workstate `q`、視窗隱藏重開保留原 session，以及 shell `exit` 後開新 session。畫面在 `build/architecture-after-*.png`，UART trace 在 `build/architecture-after-uart.log`。
- `cat /home/user/ui.txt` 的穩定畫面顯示 `persist`；停止 GUI QEMU 後以 host 唯讀 parser 驗證 WUFS metadata，讀得 `b'persist\n'`。另以無 GPU 的 UART QEMU 開機，確認既存檔 readback、`echo ui | cat`、檔案 redirection/write/readback；停止後唯讀驗證 `ui.txt` 與 `uart.txt` 的檔案 bytes 及全盤 metadata。
- `input.c`、VT model、core IO/console/process 的 include/call 邊界經靜態檢查；活動文件的 GitHub source links 均指向現行路徑，舊規格 source links 固定至 `800a805`。文件網站建置成功。
- 原始 `build/wuwaos.img` SHA-256 `6ba61b2e9e6c5af4931dd39d338619bf66bf25c5639bb916adc59b6b62fd4989`，原始 swap SHA-256 `f6ddb73f54cf434916200d752a0596b14da7d6534fe0f8cf454e6b1c02078355`；驗收全程使用獨立 `build/architecture-*.img` scratch disks。
- 限制：QEMU 驗收涵蓋互動、session 重開與已完成寫入的持久性，沒有做真實斷電/宿主崩潰測試，也沒有宣稱涵蓋所有裝置故障排列。這次只重整模組界面，未改核心演算法或 syscall/WUFS 格式。
