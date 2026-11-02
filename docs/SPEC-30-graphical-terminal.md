# Spec 30：桌面 Terminal 與圖形 shell

日期：2026-11-02

狀態：已實作並完成 QEMU 互動驗收；有限終端子集

## 目標

- 保留 `make run` 的 UART shell；在既有 1280×720 圖形桌面左上放置 Terminal 圖示。
- 使用 VirtIO tablet 點擊圖示開啟固定大小終端視窗，並以真正的 VirtIO keyboard 操作既有 `user.elf` shell。
- GUI 輸入、shell 輸出、行編輯與 ANSI 畫面更新均在 guest kernel/guest process 中完成；QEMU VNC 只呈現 guest framebuffer。

## 範圍與限制

- 僅新增一個固定位置、固定大小、可關閉/重新顯示的 Terminal 視窗；不做一般視窗管理、拖曳、縮放、工作列或 file manager。
- Terminal 使用單一自訂 kernel TTY/PTY 子集，不宣稱 POSIX `openpty` 或完整 `termios`。
- 虛擬鍵盤支援 US ASCII、左右 Shift/Ctrl、Enter、Backspace、Tab、Ctrl-D、方向鍵；滑鼠只需絕對定位與左鍵。
- 字元終端為 100×30；支援 CR/LF/Backspace/Tab、自動換行/捲動、CSI `H/f A/B/C/D J K m`、SGR 基本色/亮色/反白、私有 `?25`、`?1049h/l`。
- 不新增測試程式碼；不修改 `AGENTS.md`、既有 `tools/__pycache__` 或原始 WUFS/swap 映像。
- 不支援視窗拖曳/縮放、剪貼簿、job control、Ctrl+C、完整 POSIX PTY/termios 或多視窗。

## 架構

### 概念與目前實作

- 教科書概念：I/O 子系統可把裝置事件與上層使用者介面分層；行程生命週期由 parent、exit 與 reaping 串接。參考 `notes/ch12  I_O Systems.md` 與 `notes/ch3 Process.md`。
- WUWAOS 實作：本階段只有一個由 VirtIO keyboard/tablet 驅動的自訂 GUI TTY，並重用既有 shell、process 與 FD 路徑；不實作教科書以外的 POSIX PTY/termios ABI。

```text
QEMU VirtIO keyboard/tablet
  → FDT + device config 能力辨識
  → kernel event queue（按 SYN_REPORT 提交整批事件）
  → desktop hit-test / US key mapping
  → GUI TTY input ring → 已聚焦 Terminal shell 的 fd 0

GUI shell fd 1/2 → kernel TTY output ring → ANSI VT parser
  → terminal cell buffer → desktop framebuffer compositor → VirtIO GPU
```

- Tablet 與 keyboard 由 FDT 掃描辨識，不依賴 VirtIO MMIO slot 順序；兩者仍使用 supervisor-only MMIO mapping。GUI Make targets 共用 QEMU 裝置 recipe，`make run` 不增加 GUI 裝置。
- GUI shell 由 scheduler 可安全呼叫的專用 process helper 建立，父 PID 為 0、stdin/out/err 綁 GUI TTY endpoints。不可在 `current_index == -1` 時呼叫依賴 `process_current()` 的一般 syscall helper。視窗關閉僅隱藏並保留單一 session；圖示重新顯示同一 session。shell 執行 `exit` 後沿既有 parent=0 Zombie reaper 路徑回收 process/raw owner/FD，圖示下次點擊才建立新 shell。
- GUI TTY 與 UART console 分開輸入佇列、raw owner 與 CR/LF 狀態；canonical 模式在 Enter 後交付一行，Backspace 編輯目前輸入，Ctrl-D 在空行回傳 EOF；raw 模式交付單一 byte，交由 fan/workstate 自行處理 ANSI/按鍵。輸入只送到目前可見且聚焦的 GUI shell。輸出 ring 有固定容量，滿載時先排空至 VT parser 再繼續；GUI shell 阻塞時 scheduler 持續 poll，不在 desktop event callback 同步等待。
- kernel boot/driver/scheduler diagnostics 維持 UART。一般 user write 由 GUI shell FD 直接寫 GUI TTY；`ps`/`pcb`/`vmstat` 內建 syscall 只在呼叫其核心列印函式的短區間暫時路由，readline echo 依輸入端點寫回同一 TTY。其他 syscall、exec/exit 與可能掛起的呼叫不持有此路由狀態。
- Window 採 8×16 ASCII bitmap 字元格，至少完整繪製 `0x20–0x7e`；標題列有關閉按鈕、terminal 有可見游標。Terminal buffer 與桌布分層合成，重繪以 dirty region 批次提交，需保存/還原鼠標下方最終合成像素，確保游標移動不留下殘影。
- VT parser 有普通螢幕與 alternate screen 兩套 cell/cursor 狀態；`?1049h/l` 切換時保存/還原普通內容。SGR 支援 reset、bold、反白、基本前景/背景色與亮色；不支援的 CSI 安全忽略，不會送入 TTY/檔案資料流。

## 步驟與驗收條件

1. 檢查並記錄原 UART、GUI、磁碟路徑；完成鍵盤/tablet probe、事件 queue、按鍵/點擊事件。
2. 加入 desktop icon/window、ASCII 字型、ANSI cell parser、cursor 合成及批次 dirty rendering。
3. 接上獨立 GUI TTY endpoint、process 建立/回收、FD fork/dup/exec/close 傳遞、raw/cooked/EOF 與 shell/kernel command output routing。
4. 更新 Makefile GUI 裝置參數、README、程式碼導覽、網站首頁及開機/應用主題；不將 OS source 複製到網站。
5. WSL Ubuntu 執行 `make -j2` 通過。直接啟動 QEMU `-snapshot`（VirtIO GPU/tablet/keyboard、WUFS 與 4095-slot swap），以 QMP `input-send-event` 實際點擊圖示與送鍵、`screendump` 留存 guest framebuffer。驗收 `help`、`echo`、`pwd`、`ls`、Backspace、`echo hello | cat`、`write`/`cat`、`fan` 編輯/存檔/退出、`workstate` 的 `q` 返回、`ps`/`vmstat` GUI 輸出、關閉/重開、Ctrl-D 結束 shell 與重建。圖片為 [GUI shell](./images/wuwaos-terminal.png) 及 [workstate](./images/wuwaos-terminal-workstate.png)。
6. 不帶 GPU/tablet/keyboard 的 `-nographic -snapshot` 實際執行 `help` 與 `echo uart-regression`，UART shell 正常。原始 `build/wuwaos.img` 和 `build/wuwaos-swap.img` 在 snapshot 前後 SHA-256 相同，guest 寫入只在 snapshot overlay。核心連結結尾 `0x80320000`，低於 `0x80400000` 上界；使用者 shell text 為 16310 bytes，低於 16 KiB 限制。沒有新增測試程式碼。

## 實作限制與未驗收項目

- 驗收使用 QEMU 8.2.2 WSL Ubuntu、1280×720 VNC framebuffer、QMP 輸入事件；尚未在實體 RISC-V 硬體或其他 QEMU 顯示器驗證。
- 已分別驗證 snapshot GUI 與無圖形裝置的 UART shell；未在同一個 VM 同時向 GUI raw app 與 UART shell 送輸入，因此兩端同時工作的隔離未動態驗證。
- 不支援功能如上列範圍；未逐一動態驗證不支援/錯誤 ANSI CSI、queue 滿載、行輸入容量滿載、process table/memory 耗盡與 VirtIO 裝置故障。
- 輸入字元限 US ASCII；沒有 composition、Unicode、剪貼簿、視窗拖曳/縮放、背景工作控制或 Ctrl+C。Terminal 標題列的 X 只隱藏視窗；使用 launcher 時關閉 VNC viewer 會停止整個 QEMU session。結束 shell 請使用 `exit` 或空行 Ctrl-D。
