# 02 · 開機流程

CPU 不認識 `entry.S` 或 `main.c` 這些檔名，只會執行記憶體中的機器指令。兩個來源檔最後會被連結到同一份核心映像。

## 磁碟開機：主機先準備什麼

`make run-disk` 分成主機建置與 QEMU 開機兩個階段。主機負責交叉編譯 RISC-V 程式，並把核心安裝進 WUFS 映像；這時 WUWAOS 尚未開始執行。

```text
HOST: make run-disk
============================================================

WUWAOS source
    |
    +-- compile kernel --> build/kernel-disk.elf
    |                              |
    |                              | install into WUFS
    |                              v
    |                     build/wuwaos-cli.img
    |                     +------------------------+
    |                     | WUFS0002 filesystem    |
    |                     |   /boot/kernel.elf     |
    |                     +------------------------+
    |
    +-- compile loader --> build/bootloader.elf
                                   |
                                   v
                          build/boot-flash.img
```

`build/kernel-disk.elf` 是移除符號後、供磁碟開機使用的核心 ELF。`build/boot-flash.img` 裡放的是載入器，QEMU 將它接成唯讀 flash；核心檔案則在另一顆 VirtIO 磁碟的 `/boot/kernel.elf`。此 CLI 路徑使用單顆 WUFS0002 磁碟，停用 swap，並要求 journal 無待重播紀錄。

## 從韌體到使用者 shell

QEMU 的 RISC-V CPU 啟動後，先執行 OpenSBI 韌體，再交接到 S-mode（監督模式）的 flash 載入器。以下才是客體電腦內部的開機過程。

```text
GUEST: QEMU virt / RISC-V 64
============================================================

                    CPU starts
                        |
                        v
              +--------------------+
              | OpenSBI firmware   |
              +--------------------+
                        |
                        | hand off to S-mode
                        v
              +--------------------+
              | Flash Bootloader   |
              | 0x20000000         |
              +--------------------+
                        |
                        | discover VirtIO disk
                        v
              +--------------------+
              | Read WUFS          |
              | /boot/kernel.elf   |
              +--------------------+
                        |
                        | validate ELF, load segments into RAM
                        v
              +--------------------+
              | Kernel _start      |
              | 0x80200000         |
              | stack + clear BSS  |
              +--------------------+
                        |
                        | call kernel_main(hart_id, fdt)
                        v
              +--------------------+
              | Initialize kernel  |
              | devices / VM / FS  |
              | IRQ / timer / proc |
              +--------------------+
                        |
                        | process_start -> scheduler_loop
                        v
              +--------------------+
              | User-mode shell    |
              | wuwaos/>           |
              +--------------------+
```

載入器找到 `/boot/kernel.elf` 後，檢查 ELF 標頭與可載入區段，將核心放到 RAM，再跳到核心入口。交接時保留 OpenSBI 傳來的 hart ID（硬體執行緒編號）與 FDT（裝置樹）位址。找不到磁碟、核心檔案或 ELF 不合法時，載入器會印出錯誤並停止開機。[載入器原始碼](https://github.com/johnson931128/WUWAOS/blob/main/bootloader/boot.c)

### Flash、磁碟與 RAM 的關係

```text
Read-only Flash       VirtIO disk                 RAM
+---------------+     +--------------------+      +----------------+
| Bootloader    |---->| /boot/kernel.elf   |----->| WUWAOS kernel  |
+---------------+     +--------------------+      | 0x80200000     |
        |                                        +----------------+
        |                                                ^
        +---------------- jump to _start ----------------+
```

Bootloader 負責找到並載入核心；核心開始執行後，會初始化自己的磁碟驅動並掛載 WUFS。**載入器讀磁碟與核心掛載檔案系統，是兩個不同階段。**

## 直接開機：文字與圖形入口

`make run`、`make run-gui` 與 `.\Start-WUWAOS.cmd gui` 採用直接開機。QEMU 以 `-kernel` 預先載入 `build/kernel.elf`，OpenSBI 直接交接到核心。

```text
HOST: QEMU -kernel build/kernel.elf --> kernel placed in RAM

GUEST: CPU -> OpenSBI -> kernel _start -> kernel_main
                                            |
                                            +--> UART shell
                                            |
                                            +--> desktop, if devices exist
```

兩條路徑都在 `0x80200000` 進入同一份 `arch/riscv/entry.S`。直接開機仍可掛載 WUFS，但啟動用的核心由 QEMU 載入。圖形模式目前使用這條路徑。[啟動指令](./getting-started.md)

## entry.S：先準備 C 所需的環境

原始碼：[arch/riscv/entry.S（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/entry.S)。

`la sp, __stack_top` 設定 sp（堆疊指標）。C 的函式呼叫與區域資料會用到堆疊，因此必須先準備它。

`1:` 到 `2:` 的迴圈逐 byte 清除 BSS（未明確指定初值的靜態資料區）。例如 PCB 陣列與頁面管理欄位會從零開始。清除 B 個 bytes 的成本是 O(B)，不另外配置記憶體。

`call kernel_main` 才是兩個檔案的接點：組合語言以函式呼叫進入 C。若 C 意外返回，後面的 WFI（等待中斷指令）迴圈阻止 CPU 繼續執行未知內容。

## main.c：把子系統串起來

原始碼：[kernel/boot/main.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/boot/main.c)。

先安裝 trap（陷阱／例外入口），再顯示 UART 動畫、依 FDT 探索 PLIC 與 VirtIO 裝置、建立 Sv39 分頁映射並掛載 WUFS。接著註冊磁碟 IRQ、啟用 PLIC 通知與計時器，建立行程並進入排程器。若 FDT 找到 VirtIO GPU、tablet 與 keyboard，核心也建立 framebuffer、啟用輸入 queue 並繪製 Terminal 圖示；點擊後由 scheduler 建立獨立的 GUI shell。原始 PID 1 UART shell 仍存在；沒有圖形輸入設備時直接使用 UART。

`fdt` 指向 FDT（硬體裝置樹），是韌體交給核心的硬體描述資料。核心用它尋找 VirtIO 裝置，實體頁配置器也會保留它占用的記憶體。

## 開機動畫與觀察

`ui/boot/splash.c` 透過 UART 輸出 ANSI 終端控制碼，以圖案中點為中心逐欄揭露 WUWAOS。字形以 5×7 位元資料表示，再用背景色與空格組成 70 欄實心筆畫，從螢光黃綠漸變到薄荷綠；不依賴特殊字型。這時尚未進入使用者模式；動畫中的忙等不是排程器，也不代表已啟用計時器中斷。色塊版本見 [Spec 13](../SPEC-13-block-splash.md)。

從專案根目錄在 PowerShell 執行 `wsl -d Ubuntu -- make run-disk`，會先看到 OpenSBI 和 flash 載入器的訊息，再看到 WUWAOS 字樣、核心與裝置資訊，最後進入 `wuwaos/>`。

## VirtIO 圖形桌面

QEMU 裝上 modern VirtIO GPU 與 tablet 後，kernel 以 FDT 找到裝置 MMIO 位址，不假設固定 VirtIO slot。GPU 建立 B8G8R8X8 的 1280×720 2D resource；CPU 繪製 shadow framebuffer，裝置使用另一段持久 DMA backing。Boot 資源建立仍同步等待，runtime 的 transfer／flush 改由 `gpu_poll` 分步完成，期間新 dirty 區域合併到下一次請求，不覆寫裝置仍在使用的 backing。VirtIO tablet 的絕對座標會縮放並夾限到畫面內。

### 桌面 Terminal 與 GUI shell

目前視窗只繪製邊框與內容，沒有舊版深色實心陰影；既有截圖及 portable v1 保留先前單視窗外觀，詳見 [Spec 30](../SPEC-30-graphical-terminal.md)。2026-11-08 的多視窗與 GPU 復原尚未執行互動／視覺驗證（NOT_RUN）。

每次點擊左上角 Terminal 圖示會建立一個新 shell 視窗，最多四個；各有獨立 TTY、ANSI cell buffer 與輸入狀態。點擊視窗會置頂並取得鍵盤焦點，拖曳標題列移動，拖曳右下角調整視窗大小。畫面依 wallpaper → 底至頂的視窗 → cursor 合成，再提交 dirty 區域。視窗滿額或 shell 建立失敗會在桌面顯示提示。

關閉視窗會解除輸出 consumer、喚醒阻塞讀取並釋放 UI 引用。Shell 收到終端關閉錯誤後退出，持有該 TTY 的其他 OFD 仍須正常釋放才能回收 endpoint。Shell `exit` 後保留最後畫面，視窗可手動關閉。UART shell 的輸入與 GUI session 各自處理。

這是小型 kernel TTY，沒有 POSIX PTY、clipboard、job control 或 Ctrl+C。視窗大小調整只是裁切 viewport；終端模型仍固定 100×30，不會重排文字、通知 shell 改變列數或提供捲動檢視。GPU 故障會嘗試有界 reset／重建資源；無法復原時停用 GUI、保留 DMA storage，UART 路徑保留。架構分層見 [Spec 33](../SPEC-33-module-architecture.md)，本輪演算法與未驗證限制見[計畫](https://github.com/johnson931128/WUWAOS/blob/main/plan.md)；舊 [Spec 28](../SPEC-28-native-desktop.md)／[Spec 30](../SPEC-30-graphical-terminal.md) 保留當時驗收。

Windows portable 環境可直接使用 `.\Start-WUWAOS.cmd gui`，QEMU 會開啟原生視窗，不需要另外的 VNC viewer。WSLg/Linux 圖形工作階段可用 `make run-gui GUI_DISPLAY=gtk`（或 `sdl`）。既有 Windows/WSL1 環境則可從 repo 根目錄以單一 PowerShell 指令啟動背景 QEMU 與原生 VNC viewer：

```powershell
./tools/run-desktop.ps1
```

Launcher 會先檢查 Windows viewer、WSL 工具鏈及現有 QEMU，再建置並選擇未使用的 localhost VNC/QMP ports。關閉 viewer 會透過本次 QMP session 結束其 QEMU。TigerVNC viewer 可從[官方 releases](https://github.com/TigerVNC/tigervnc/releases)取得；若不在 PATH、標準安裝位置或 `build/tigervnc`，可用 `-ViewerPath` 指定。預設使用專案磁碟並保留客體寫入；`-Snapshot` 會改用關閉時刪除的暫存副本。launcher 的建置與序列輸出記錄在 `build/desktop-<session-id>`。手動進階流程可使用 `make run-gui-vnc` 加 Windows viewer。

## 延伸資料

觀念參考：`notes/ch2 Operating-System Structures.md` 的開機與核心結構，`notes/ch12  I_O Systems.md` 的裝置 I/O 分層。WUWAOS 目前用自訂 GUI TTY 將 VirtIO input 連到既有 shell，不宣稱完整 PTY。實作細節與早期動畫驗收見[程式碼導覽](../CODE_GUIDE.md)及 [Spec 04](../SPEC-04-boot-splash.md)。
