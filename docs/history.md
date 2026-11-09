# 原始規格與驗收紀錄

此處按開發順序保存規格。它們記錄「當時做了什麼」，不一定代表現在的架構；第一次閱讀建議先看左側主題章節。
Spec 33 以前的歷史 source link 若指向舊 `kernel/` 或 `user/` 路徑，固定指向 2026-11-07 重整前 commit `800a805`，以保留當時程式碼參照；目前目錄與 UI/kernel 邊界見 [Spec 33](./SPEC-33-module-architecture.md)。

## CLI 磁碟開機（2026-11-09）

`make run-disk` 交叉編譯核心並安裝到 WUFS0002。QEMU 透過 OpenSBI 進入唯讀 flash 載入器，再從磁碟的 `/boot/kernel.elf` 啟動 UART shell。使用 QEMU 8.2.2／OpenSBI 1.3 驗證了 PowerShell 啟動至 `wuwaos/>`、CLI 指令、新磁碟開機、重啟讀回與無磁碟時停止開機。 [開機流程](./topics/boot.md) · [實作與驗收範圍](https://github.com/johnson931128/WUWAOS/blob/main/plan.md#cli-磁碟開機2026-11-09) · [實作提交](https://github.com/johnson931128/WUWAOS/commit/7b0b3e2)

## 互動教學整理（2026-11-08）

網站以 HTML/CSS/Vue 加入 [Block Async I/O](./topics/block-async.md)、[FD/OFD](./topics/file-descriptors.md)、[換頁與 Swap](./topics/paging.md)、[Journal 與安全替換](./topics/journal.md) 的教學模型，按章節分別提交，搭配筆記引用、參數／欄位與函式對照。模型展示的是程式邏輯順序，不是即時 OS、效能量測或新 QEMU 驗收。

大型概念與狀態演示使用 DOM/CSS，自動排版的 Mermaid 僅保留在程式碼關聯與小流程；原始規格的圖片、流程與驗收仍保留歷史位置。第一輪不使用新靜態生成圖或第三方教科書圖片；瀏覽器互動與視覺驗收依使用者要求為 NOT_RUN。

## Spec 33 之後的實作進度

後續工作直接記錄於 [詳細計畫](https://github.com/johnson931128/WUWAOS/blob/main/plan.md)，不改寫舊規格的驗收紀錄。對照目前程式碼請讀 [程式碼導覽](./CODE_GUIDE.md) 與主題章節。

| 工作 | 目前狀態與紀錄 |
| --- | --- |
| 第 1 階段：多 TTY/session | 已完成 endpoint 世代 handle、獨立佇列與 OFD 路由；當時 UI 為單一 Terminal view（[d89a8a7](https://github.com/johnson931128/WUWAOS/commit/d89a8a7)） |
| 2A：事件等待 | 已完成 TTY／pipe 的 caller-stack 等待與 idle 喚醒，scheduler poll 不執行 usercopy（[f9e092b](https://github.com/johnson931128/WUWAOS/commit/f9e092b)） |
| 2B1：核心 gate | 已完成 owner-aware gate、OFD／pipe、FS／VM 保護與延後取消；增量建置及基本 GUI smoke 已執行，額外邊界矩陣未執行（[aa480e3](https://github.com/johnson931128/WUWAOS/commit/aa480e3)） |
| 2B2：非同步 Block | 已實作 per-device gate／event／bounce、runtime 等待與 VM PTE rollback（[eb84ff7](https://github.com/johnson931128/WUWAOS/commit/eb84ff7)、[7e0f826](https://github.com/johnson931128/WUWAOS/commit/7e0f826)） |
| 2C：GPU 非阻塞 present | 已實作 CPU shadow／DMA backing 分離、dirty merge、transfer／flush poll（[ea6671e](https://github.com/johnson931128/WUWAOS/commit/ea6671e)） |
| 第 3 階段：裝置復原 | 已實作 Block reset 後隔離、GPU 有界 reset／重建／停用（[19be53a](https://github.com/johnson931128/WUWAOS/commit/19be53a)、[e48845b](https://github.com/johnson931128/WUWAOS/commit/e48845b)） |
| 第 4 階段：堆疊與生命週期 | 已實作 process／scheduler 未映射 guard page；生命週期靜態 review，沒有 emergency trap stack（[b555d29](https://github.com/johnson931128/WUWAOS/commit/b555d29)） |
| 第 5 階段：四視窗 | 已實作獨立 TTY、z-order／focus、drag、裁切 viewport 與 close；固定 100×30 model（[05b8124](https://github.com/johnson931128/WUWAOS/commit/05b8124)） |
| 第 6 階段：安全替換與診斷 | 已實作 fs_replace／Fan、新 ERR_UNCERTAIN 與 UART 診斷（[496b757](https://github.com/johnson931128/WUWAOS/commit/496b757)）；後續故障輸出整理見 [6ef3029](https://github.com/johnson931128/WUWAOS/commit/6ef3029)、[3bc436d](https://github.com/johnson931128/WUWAOS/commit/3bc436d) |

2026-11-08 的 2B2、2C 與第 3–6 階段已完成一次必要整合編譯。依使用者要求未執行任何測試、未新增測試程式碼；所有新增 runtime／GUI／裝置故障與復原／重播均為 NOT_RUN。本次文件建置亦不代表 OS 驗收，詳細算法與限制以計畫為準。

## 原始規格

| 階段 | 規格與定位 |
| --- | --- |
| 01 | [使用者模式與系統呼叫](./SPEC-01-user-syscalls.md) |
| 02 | [合作式行程生命週期](./SPEC-02-process-lifecycle.md) |
| 03 | [Shell、PCB 與 Pipe](./SPEC-03-shell-pipe.md) |
| 04 | [開機 ASCII 動畫](./SPEC-04-boot-splash.md) |
| 05 | [Fork → Exec → 排程 → 回收](./SPEC.md)，靜態位址空間設計已更新 |
| 06 | [VirtIO 與 WUFS](./SPEC-06-filesystem.md) |
| 07 | [FIFO 排程器](./SPEC-07-fifo-scheduler.md) |
| 08 | [動態實體頁與三級頁表](./SPEC-08-memory.md)，預先載入使用者頁的部分由 10 取代 |
| 09 | [fan 全螢幕編輯器](./SPEC-09-fan.md) |
| 10 | [Demand paging、頁面置換與 Swap](./SPEC-10-demand-paging.md)，現行 VM 設計 |
| 11 | [文件網站](./SPEC-11-documentation.md) |
| 12 | [磁碟請求逾時與停止核心](./SPEC-12-block-timeout.md) |
| 13 | [螢光色塊開機字樣](./SPEC-13-block-splash.md) |
| 14 | [逐行命令說明](./SPEC-14-shell-help.md) |
| 15 | [計時器中斷與 RR 排程](./SPEC-15-timer-rr.md)，現行 CPU 排程政策 |
| 16 | [RV64 核心上下文切換](./SPEC-16-kernel-context.md)，獨立排程器堆疊、阻塞後恢復 C 呼叫 |
| 17 | [workstate 即時監測](./SPEC-17-workstate.md)，CPU／RAM 快照、毫秒休眠與全螢幕操作 |
| 18 | [檔案命令與 cls](./SPEC-18-file-commands.md)，刪除、複製、畫面與捲動歷史 |
| 19 | [工作目錄與 shell 命令處理](./SPEC-19-cwd-shell.md)，每行程目錄、相對路徑 |
| 20 | [資料夾刪除與確認](./SPEC-20-directory-removal.md)，子目錄與檔案回收 |
| 21 | [系統內編譯 C 程式的長期目標](./SPEC-21-native-compilation.md)，目前為規劃、尚未實作 |
| 22 | [自製 Mutex 與 Semaphore](./SPEC-22-synchronization.md)，單核心同步與驗證範圍 |
| 23 | [同步工具互動示範](./SPEC-23-sync-demo.md)，實際行程與等待狀態 |
| 24 | [功能示範影片](./SPEC-24-demo-video.md)，QEMU 實際操作與影片驗證 |
| 25 | [行程容量擴充](./SPEC-25-process-capacity.md)，16 個行程、資源回收與 workstate 分頁 |
| 26 | [WUFS 路徑式 ELF 載入](./SPEC-26-elf-exec.md)，受限靜態 RV64 使用者程式 |
| 27 | [檔案描述號、一般 I/O 與 Shell 管線](./SPEC-27-file-descriptors-and-shell-pipelines.md)，FD/OFD、WUFS offset I/O、多階段管線與重導向 |
| 28 | [原生 QEMU 圖形桌面與游標](./SPEC-28-native-desktop.md)，VirtIO GPU/tablet、1280×720 framebuffer 與 guest 游標驗收 |
| 29 | [Windows 單指令桌面啟動器](./SPEC-29-desktop-launcher.md)，背景 QEMU、原生 VNC viewer、QMP 關閉與 Snapshot 磁碟隔離 |
| 30 | [桌面 Terminal 與圖形 shell](./SPEC-30-graphical-terminal.md)，VirtIO keyboard/tablet、獨立 GUI TTY、ANSI 終端與 QEMU 驗收 |
| 31 | [WUFS 非空白磁碟保護與固定驗證入口](./SPEC-31-filesystem-initialization-guard.md)，kernel／主機映像工具的格式化條件、獨立磁碟與 SHA-256 檢查；使用者已確認本項驗證 |
| 32 | [WUFS0002 整磁區 redo journal 與安全儲存](./SPEC-32-wufs-redo-journal.md)，提交順序、重播、WUFS0001 唯讀相容、host image 安全替換與 QEMU fault 驗收 |
| 33 | [核心模組分層與圖形終端解耦](./SPEC-33-module-architecture.md)，目前 source tree 與 UI、TTY、kernel、driver 的資料通道 |

## 如何判斷是否驗證過

請讀各規格的「驗收紀錄」及「限制與未驗證」。設計方案、原始碼靜態檢查、實際 QEMU 操作是不同種類的證據；沒有記錄的邊界不能當成已通過。

完整檔案職責與演算法見 [程式碼與架構紀錄](./CODE_GUIDE.md)。
