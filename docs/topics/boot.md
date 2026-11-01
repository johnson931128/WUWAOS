# 02 · 開機流程

CPU 不認識 `entry.S` 或 `main.c` 這些檔名，只會執行記憶體中的機器指令。兩個來源檔最後會被連結到同一份核心映像。

## 從韌體到 C

```text
QEMU virt → OpenSBI → 0x80200000 的 _start
  → 設定 stack、清除 BSS → call kernel_main
  → FDT 裝置探測、頁表／檔案系統／圖形及輸入初始化
  → process_start → scheduler_loop → shell（UART）
```

OpenSBI（RISC-V 韌體）完成早期準備，將控制權交給 S-mode（監督模式）核心。`kernel/linker.ld` 指定 `_start` 為入口，從 `0x80200000` 放置核心。

## entry.S：先準備 C 所需的環境

原始碼：[kernel/entry.S（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/entry.S)。

`la sp, __stack_top` 設定 sp（堆疊指標）。C 的函式呼叫與區域資料會用到堆疊，因此必須先準備它。

`1:` 到 `2:` 的迴圈逐 byte 清除 BSS（未明確指定初值的靜態資料區）。例如 PCB 陣列與頁面管理欄位會從零開始。清除 B 個 bytes 的成本是 O(B)，不另外配置記憶體。

`call kernel_main` 才是兩個檔案的接點：組合語言以函式呼叫進入 C。若 C 意外返回，後面的 WFI（等待中斷指令）迴圈阻止 CPU 繼續執行未知內容。

## main.c：把子系統串起來

原始碼：[kernel/main.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/main.c)。

先安裝 trap（陷阱／例外入口），再顯示 UART 動畫、探索 VirtIO 裝置、建立核心分頁映射與掛載檔案系統。若 FDT 找到 VirtIO GPU，核心建立 framebuffer 並畫出桌面；若另找到 VirtIO tablet，啟用游標更新。之後初始化 SBI 計時器、建立第一個行程並交給行程恢復流程；排程器輪詢輸入事件，同時保留 UART shell。

`fdt` 指向 FDT（硬體裝置樹），是韌體交給核心的硬體描述資料。核心用它尋找 VirtIO 裝置，實體頁配置器也會保留它占用的記憶體。

## 開機動畫與觀察

`kernel/splash.c` 透過 UART 輸出 ANSI 終端控制碼，以圖案中點為中心逐欄揭露 WUWAOS。字形以 5×7 位元資料表示，再用背景色與空格組成 70 欄實心筆畫，從螢光黃綠漸變到薄荷綠；不依賴特殊字型。這時尚未進入使用者模式；動畫中的忙等不是排程器，也不代表已啟用計時器中斷。色塊版本見 [Spec 13](../SPEC-13-block-splash.md)。

執行 `make run`，應依序看到字樣、核心與裝置資訊、最後的 `wuwaos>`。`/boot/kernel.elf` 是 WUFS 中的副本；目前真正啟動核心仍由 QEMU 的 `-kernel` 載入。

## VirtIO 圖形桌面

QEMU 裝上 modern VirtIO GPU 與 tablet 後，kernel 以 FDT 找到裝置 MMIO 位址，不假設它們在固定 VirtIO slot。GPU 建立 B8G8R8X8 2D resource，掛上一段連續的 1280×720 framebuffer RAM，再設定 scanout 並提交像素；桌面是淡藍底和白色 `WUWAOS` 字樣。VirtIO tablet 提供絕對 X/Y 座標，核心依裝置範圍縮放、夾限到畫面內，於同步事件後更新游標。游標移動只重送舊位置與新位置覆蓋的矩形區域。

這是 kernel renderer（核心繪圖器），不是桌面伺服器或 user process；沒有視窗、工作列、圖形 shell、鍵盤操作或檔案管理器。沒有 GPU/tablet 時，原本的文字 shell 照常啟動。架構與實際驗收見 [Spec 28](../SPEC-28-native-desktop.md)。

WSLg/Linux 圖形工作階段可用 `make run-gui GUI_DISPLAY=gtk`（或 `sdl`）。Windows/WSL1 可分別在兩個 PowerShell 視窗啟動 VNC guest 與 TigerVNC viewer：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run-gui-vnc'
& 'C:\path\to\vncviewer64.exe' '127.0.0.1::5901'
```

TigerVNC viewer 請從[官方 releases](https://github.com/TigerVNC/tigervnc/releases)安裝／下載。VNC server 和 QMP debug control 都綁定 localhost；`run-gui-vnc` 預設 VNC port 為 5901、QMP port 為 4451。圖形目標會像 `make run` 一樣先建立或更新磁碟映像；客體寫入會持續保留。QEMU 執行期間不要從主機端操作磁碟映像。VNC QEMU 使用 `-serial stdio`，在其 WSL 終端按 Ctrl+C 結束。

## 延伸資料

觀念參考：`notes/ch2 Operating-System Structures.md` 的開機與核心結構。實作細節與早期動畫驗收見[程式碼導覽](../CODE_GUIDE.md)及 [Spec 04](../SPEC-04-boot-splash.md)。
