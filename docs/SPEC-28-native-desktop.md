# Spec 28：原生 QEMU 圖形桌面與游標

日期：2026-11-01

狀態：已實作；限 QEMU `virt` 與 modern VirtIO GPU/tablet。此階段提供 kernel framebuffer 桌布和游標，不是完整桌面環境。

## 目標

- QEMU 以同一份 WUWAOS 核心開機；偵測到 GPU 時顯示固定 1280×720 畫面。
- 背景為淡藍色，中央有白色 `WUWAOS` 字樣。
- 使用 VirtIO tablet 絕對座標事件移動畫面中的白色、黑邊箭頭游標。
- 保留 `make run` 的 `-nographic` UART shell 啟動路徑；沒有 GPU/tablet 時回到原文字流程。
- 不改使用者程式容量，不新增測試程式碼，不以此次 QEMU 驗收覆寫 WUFS 或 swap 原始映像。

## 工作範圍

包含 modern VirtIO-MMIO GPU 與 input 裝置探測、監督模式 MMIO 映射、2D scanout、固定像素 framebuffer、桌布字形繪製、tablet event queue、絕對座標縮放／邊界夾限、游標舊背景復原和 dirty rectangle 更新，以及 `run-gui`／`run-gui-vnc` 啟動方式。

不包含桌面 user process、視窗管理、工作列、圖形 shell、GUI keyboard、應用程式視窗、檔案管理器、動態解析度、3D/OpenGL、其他虛擬機型或實體硬體支援。既有 Shell、fan、workstate 仍透過 UART 輸入輸出。

## 架構與資料流

```text
OpenSBI FDT
  ├─ Device ID 2  → block.c 的 WUFS／swap 裝置
  ├─ Device ID 16 → gpu.c → control queue 0 → 2D resource / scanout
  └─ Device ID 18 → input.c → event queue 0 → 絕對座標
                    ↓
           vm_map_mmio → supervisor-only kernel mapping
                    ↓
       page allocator 連續 900 頁 framebuffer（1280×720×4 bytes）
                    ↓
    desktop.c 清背景、繪字、保留游標下方像素並呈現 dirty rectangle
                    ↑
 process_resume / scheduler_loop → desktop_poll → input_poll
```

GPU 與 tablet 的位址都從 FDT `virtio,mmio` 節點 `reg` 取得，並以 magic、modern version 2、Device ID 驗證；不依賴 QEMU 的 VirtIO MMIO slot 順序。核心頁表只映射被探測到的裝置頁，權限為 supervisor-only。GPU backing 由 `page_alloc_contiguous` 從核心頁池取得 900 個相鄰 4 KiB 頁，故可供裝置連續讀取，且不計入使用者常駐頁額度或 swap。

GPU control queue 使用 split virtqueue。初始化協商 `VERSION_1`、選 queue 0、設定 descriptor/available/used ring；每筆控制命令以一個唯讀 request descriptor 接一個可寫 response descriptor。單核心同步等待 used ring 完成，逾時上限 5 秒；逾時時停止核心，避免裝置仍可能 DMA 時回收 request 或 framebuffer。命令依序建立 B8G8R8X8_UNORM 2D resource、掛接 backing、設定 scanout，再用 TRANSFER_TO_HOST_2D 和 RESOURCE_FLUSH 更新畫面。

桌布使用 RGB `#B8DCF5`，以 5×7 W／U／A／O／S bitmaps 放大置中繪出白色字樣，另畫 16×16 黑框白填箭頭。VirtIO tablet 的 ABS_X／ABS_Y 最小值與最大值從 device config 讀取；輸入值先線性縮放至 framebuffer 像素範圍，之後限制左上角位置最多為 `(1264,704)`，讓 16×16 游標完整留在 1280×720 畫面內。核心累積軸事件，收到 SYN_REPORT 後才採用座標。

移動游標時先把舊位置所覆蓋的 framebuffer 像素從 16×16 save buffer 還原，再擷取新位置背景、繪製箭頭，最後一次傳送舊矩形與新矩形的 bounding rectangle。這可保留字樣下方原像素，也避免游標跨越畫面留下殘影。`desktop_poll` 在既有 process resume 與 scheduler loop 的 UART I/O poll 後執行；沒有可用桌面游標時會直接返回。

## 執行步驟與分層提交

| 層 | 內容 | Commit |
| --- | --- | --- |
| 1 | VirtIO GPU FDT/MMIO、連續 framebuffer、2D resource/scanout、`run-gui` 入口 | `44a59ce` Add VirtIO GPU 2D scanout |
| 2 | #B8DCF5 桌布、WUWAOS 置中字樣與實際 guest 截圖 | `de74346` Draw native WUWAOS desktop background |
| 3 | VirtIO tablet 絕對輸入、SYN_REPORT、clamp、箭頭游標與 dirty rectangle；排程器輪詢 | `bf4fe86` Add VirtIO tablet desktop cursor |
| 3 修正 | GPU/tablet 部分初始化時禁止對無游標 framebuffer 繪製；正式 GUI targets 接上磁碟建置並採正常持久化語意 | `eff544a` Harden desktop fallback and GUI launch targets |

README、開機／applications 主題、CODE_GUIDE、歷史索引與本規格共同描述現行功能；實際 guest 桌面截圖保留於 `docs/images/wuwaos-desktop.png`。

## 啟動方式

Linux/WSLg 圖形工作階段：`make run-gui GUI_DISPLAY=gtk`，或 `GUI_DISPLAY=sdl`。Windows WSL1 沒有 WSLg 時，用 `make run-gui-vnc` 啟動只綁 `127.0.0.1:5901` 的 VNC server；預設 QMP debug control 端點綁 `127.0.0.1:4451`，供 QEMU 控制與畫面擷取，不是日常桌面操作介面。可用 `GUI_VNC_DISPLAY`、`GUI_QMP_PORT` 更換 display number 或 QMP port。Windows 下載 [TigerVNC official releases](https://github.com/TigerVNC/tigervnc/releases)，再執行 `vncviewer 127.0.0.1::5901`；本機執行檔不納入 repository。

圖形 targets 與 `make run` 一樣依賴 `disk-image`，自動建立或更新 `build/wuwaos.img`、`build/wuwaos-swap.img`；WUFS 中的既有檔案由原 disk builder 保留，客體檔案操作會依正常磁碟語意持續保存。QEMU 執行期間不得由主機另行操作磁碟映像。文字模式維持 `make run`，它仍以 `-nographic` 啟動 UART shell。驗收使用的 QEMU instance 另外由直接 QEMU 命令加 `-snapshot` 啟動，以確保驗收不改寫原始映像；沒有執行 `make disk-image` 來驗收。

## 驗收紀錄

環境為 Windows PowerShell 呼叫 WSL1 Ubuntu、QEMU 8.2.2、Clang/LLD；Kernel 建置使用 `make build/kernel.elf -j2`，`-Wall -Wextra -Werror` 成功。沒有新增測試程式碼。

- GPU 單色 bring-up 的 QMP `screendump` 輸出 1280×720 guest framebuffer；guest log 為 `[gpu] 1280x720 VirtIO 2D scanout ready`。分層截圖保留在忽略的 build 目錄。
- 桌布截圖量得 1280×720，背景 RGB `(184,220,245)`，中央白字可見。最終文件使用 [`images/wuwaos-desktop.png`](./images/wuwaos-desktop.png)，由 QMP `screendump` 匯出 QEMU guest framebuffer，畫面中包含 guest 自繪箭頭游標。另以 QMP `query-vnc` 確認原生 TigerVNC viewer 連至 localhost，但截圖來源是 guest framebuffer。
- VirtIO tablet log 回報 X/Y max=`32767`。以 QMP `input-send-event` 依序送 `(4000,28000)`、`(30000,5000)`、`(32767,32767)`、`(0,0)`、`(16000,16000)`；每張 guest screenshot 都是 1280×720。相鄰畫面的差異逐像素檢查均落在舊游標與新游標 16×16 矩形範圍內，沒有游標殘影。
- Windows TigerVNC viewer 連到 QEMU `127.0.0.1:5901`，QMP `query-vnc` 顯示 loopback client；QMP guest 截圖與絕對座標事件確認箭頭可見且移動。Windows viewer 視窗擷取工具逾時，故 host 視窗擷取及人工實體滑鼠操作標為 **NOT_RUN**。
- 不加 GPU/tablet 的 QEMU `-nographic -snapshot` 啟動後，`help`、`echo hello`、`writer | reader`、`echo hello | cat`、`ps` 均在原 UART shell 執行成功；80-byte pipe 到 EOF，子行程正常 wait/reap。此為直接 QEMU 回歸；沒有呼叫會重建映像的 `make run`。
- 圖形 session 使用 snapshot；前後 `build/wuwaos.img` SHA-256 為 `f4d50e7f7ea3360fe6fa4f5eaadc40db4e56568fdd683c2ce291ee36990191a5`，swap 為 `f6ddb73f54cf434916200d752a0596b14da7d6534fe0f8cf454e6b1c02078355`，一致。

## 限制與未驗證

- 解析度固定 1280×720；未驗證調整視窗大小、DPI／縮放變化、額外 scanout、非 QEMU 裝置或非 RV64/QEMU virt 環境。
- 只處理絕對 tablet X/Y；沒有相對滑鼠、按鍵、鍵盤、觸控多點或 GUI 應用輸入。
- 桌面繪製及輪詢由 kernel 執行，無 user desktop server、window manager、terminal window、工作列、圖形 shell 或 file manager。
- GPU 5 秒逾時會停機而非自行復原；裝置不存在或初始化失敗會保留 UART shell。此輪未逐一模擬不存在／失效 GPU 及 input queue 錯誤回應分支。
- 原生 Windows viewer 視窗的自動擷取未成功；記錄為 NOT_RUN。QMP 輸入事件驗證了客體 VirtIO tablet→framebuffer 游標路徑，但不等同人工滑鼠操作驗收。
- `make run-gui-vnc`/`make run-gui` Make target 的 fresh-image prerequisite 由靜態 Makefile 確認，沒有呼叫 target 以免驗收更新既有磁碟；VNC 執行以等價 QEMU 參數在 snapshot 下實際驗收。SDL／GTK 只完成參數設定，沒有在 WSL1 實際啟用 GUI。

## 參考

- 教科書式的作業系統核心、開機及裝置 I/O 觀念，參考 `notes/ch2 Operating-System Structures.md` 及 `notes/ch12  I_O Systems.md`；這些概念不代表 WUWAOS 已實作一般桌面環境。
- VirtIO transport、GPU 2D commands、pixel formats 與 input event/config 結構依 [OASIS VirtIO 1.2 specification](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html) 核對；本實作限上文所述的 QEMU 裝置和命令子集。
- QEMU 裝置啟動與 virtio-gpu 使用說明：[QEMU VirtIO GPU documentation](https://www.qemu.org/docs/master/system/devices/virtio/virtio-gpu.html)。
