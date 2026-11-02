# 02 · 開機流程

CPU 不認識 `entry.S` 或 `main.c` 這些檔名，只會執行記憶體中的機器指令。兩個來源檔最後會被連結到同一份核心映像。

## 從韌體到 C

```text
QEMU virt → OpenSBI → 0x80200000 的 _start
  → 設定 stack、清除 BSS → call kernel_main
  → FDT 裝置探測、頁表／檔案系統／GPU、tablet 與 keyboard 初始化
  → process_start → scheduler_loop → shell（UART；圖形桌面另可開 GUI shell）
```

OpenSBI（RISC-V 韌體）完成早期準備，將控制權交給 S-mode（監督模式）核心。`kernel/linker.ld` 指定 `_start` 為入口，從 `0x80200000` 放置核心。

## entry.S：先準備 C 所需的環境

原始碼：[kernel/entry.S（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/entry.S)。

`la sp, __stack_top` 設定 sp（堆疊指標）。C 的函式呼叫與區域資料會用到堆疊，因此必須先準備它。

`1:` 到 `2:` 的迴圈逐 byte 清除 BSS（未明確指定初值的靜態資料區）。例如 PCB 陣列與頁面管理欄位會從零開始。清除 B 個 bytes 的成本是 O(B)，不另外配置記憶體。

`call kernel_main` 才是兩個檔案的接點：組合語言以函式呼叫進入 C。若 C 意外返回，後面的 WFI（等待中斷指令）迴圈阻止 CPU 繼續執行未知內容。

## main.c：把子系統串起來

原始碼：[kernel/main.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/main.c)。

先安裝 trap（陷阱／例外入口），再顯示 UART 動畫、探索 VirtIO 裝置、建立核心分頁映射與掛載檔案系統。若 FDT 找到 VirtIO GPU、tablet 與 keyboard，核心建立 framebuffer、啟用輸入 queue 後繪製 Terminal 圖示；點擊後由 scheduler 建立獨立的 GUI shell。原始 PID 1 UART shell 仍存在；沒有圖形輸入設備時直接使用 UART。

`fdt` 指向 FDT（硬體裝置樹），是韌體交給核心的硬體描述資料。核心用它尋找 VirtIO 裝置，實體頁配置器也會保留它占用的記憶體。

## 開機動畫與觀察

`kernel/splash.c` 透過 UART 輸出 ANSI 終端控制碼，以圖案中點為中心逐欄揭露 WUWAOS。字形以 5×7 位元資料表示，再用背景色與空格組成 70 欄實心筆畫，從螢光黃綠漸變到薄荷綠；不依賴特殊字型。這時尚未進入使用者模式；動畫中的忙等不是排程器，也不代表已啟用計時器中斷。色塊版本見 [Spec 13](../SPEC-13-block-splash.md)。

執行 `make run`，應依序看到字樣、核心與裝置資訊、最後的 `wuwaos>`。`/boot/kernel.elf` 是 WUFS 中的副本；目前真正啟動核心仍由 QEMU 的 `-kernel` 載入。

## VirtIO 圖形桌面

QEMU 裝上 modern VirtIO GPU 與 tablet 後，kernel 以 FDT 找到裝置 MMIO 位址，不假設它們在固定 VirtIO slot。GPU 建立 B8G8R8X8 2D resource，掛上一段連續的 1280×720 framebuffer RAM，再設定 scanout 並提交像素；桌面是淡藍底和白色 `WUWAOS` 字樣。VirtIO tablet 提供絕對 X/Y 座標，核心依裝置範圍縮放、夾限到畫面內，於同步事件後更新游標。游標移動只重送舊位置與新位置覆蓋的矩形區域。

### 桌面 Terminal 與 GUI shell

點擊左上角 Terminal 圖示會建立固定大小視窗並啟動原本的 shell user image。VirtIO keyboard 事件轉成 US ASCII，送入獨立的 GUI canonical input；shell 輸出與 ANSI TUI 由 guest kernel 更新終端 cell buffer，再繪製到 framebuffer。關閉視窗只隱藏畫面，點圖示可接續同一個 shell；shell `exit` 後會回收，下次再點才建立新的 session。GUI 輸入／raw owner 與 UART 分開，`make run` 不帶圖形裝置時保留舊 UART shell。

這是小型 kernel TTY，不是 POSIX PTY 或一般視窗系統。視窗固定大小，不能拖曳或縮放；沒有 clipboard、job control 或 Ctrl+C。`fan`、`workstate` 使用 raw keyboard input。設計、驗收影像與限制見 [Spec 30](../SPEC-30-graphical-terminal.md)；GPU 與游標架構仍見 [Spec 28](../SPEC-28-native-desktop.md)。

WSLg/Linux 圖形工作階段可用 `make run-gui GUI_DISPLAY=gtk`（或 `sdl`）。Windows/WSL1 建議從 repo 根目錄以單一 PowerShell 指令啟動背景 QEMU 與原生 VNC viewer：

```powershell
./tools/run-desktop.ps1
```

Launcher 會先檢查 Windows viewer、WSL 工具鏈及現有 QEMU，再建置並選擇未使用的 localhost VNC/QMP ports。關閉 viewer 會透過本次 QMP session 結束其 QEMU。TigerVNC viewer 可從[官方 releases](https://github.com/TigerVNC/tigervnc/releases)取得；若不在 PATH、標準安裝位置或 `build/tigervnc`，可用 `-ViewerPath` 指定。預設使用專案磁碟並保留客體寫入；`-Snapshot` 會改用關閉時刪除的暫存副本。launcher 的建置與序列輸出記錄在 `build/desktop-<session-id>`。手動進階流程仍可使用 `make run-gui-vnc` 加 Windows viewer；`make run` 的 UART 文字模式維持不變。

## 延伸資料

觀念參考：`notes/ch2 Operating-System Structures.md` 的開機與核心結構，`notes/ch12  I_O Systems.md` 的裝置 I/O 分層。WUWAOS 目前用自訂 GUI TTY 將 VirtIO input 連到既有 shell，不宣稱完整 PTY。實作細節與早期動畫驗收見[程式碼導覽](../CODE_GUIDE.md)及 [Spec 04](../SPEC-04-boot-splash.md)。
