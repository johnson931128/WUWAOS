---
aside: false
---

<script setup lang="ts">
import SystemOverview from './.vitepress/theme/components/SystemOverview.vue'
</script>

<div class="home-textbook">

<section class="editorial-hero">
<div class="eyebrow">WUWAOS · A SOURCE-BASED OS TEXTBOOK</div>

<h1>從第一條指令，到一個真正啟動的作業系統</h1>

<p>WUWAOS 以 C 與 RISC-V 組合語言實作，透過 QEMU 在 RV64 虛擬電腦上啟動。這份教材沿著硬體、核心與使用者程式的邊界，連接作業系統觀念、目前原始碼與可觀察操作。</p>

<div class="hero-meta"><span>QEMU virt</span><span>RV64 · single hart</span><span>S-mode kernel / U-mode apps</span></div>
</section>
</div>

<div class="spec-strip" aria-label="WUWAOS 平台規格">
  <div><strong>RV64</strong><span>RISC-V 64-bit · QEMU virt</span></div>
  <div><strong>1 hart</strong><span>單核心 · RR 排程</span></div>
  <div><strong>128 MiB</strong><span>QEMU guest RAM</span></div>
  <div><strong>1280 × 720</strong><span>可選 VirtIO GPU 桌面</span></div>
</div>

## 系統地圖

以下架構依目前原始碼繪製，不把教材概念、網站互動模型或其他 OS 的元件誤當成 WUWAOS 實作。選取模組可查看角色、來源與對應章節。

<SystemOverview />

## 一條循序閱讀的路線

從啟動環境一路走到系統本身的文件維護方式。每一章接上原始碼、限制與相關課程筆記。

<div class="roadmap-grid">
  <a href="./topics/getting-started.html"><small>01 · ORIENT</small><strong>開始使用</strong><span>主機、工具鏈與 QEMU</span></a>
  <a href="./topics/boot.html"><small>02 · BOOT</small><strong>開機流程</strong><span>從 OpenSBI、磁碟載入器到 CLI</span></a>
  <a href="./topics/traps"><small>03 · BOUNDARY</small><strong>例外與系統呼叫</strong><span>trap、ecall 與 ABI</span></a>
  <a href="./topics/processes"><small>04 · PROCESS</small><strong>行程管理</strong><span>建立、切換、結束與回收</span></a>
  <a href="./topics/scheduling"><small>05 · CPU</small><strong>計時器與 RR 排程</strong><span>ready queue、派發與時間片</span></a>
  <a href="./topics/memory"><small>06 · MEMORY</small><strong>實體頁與 Sv39</strong><span>虛擬位址、頁表與權限</span></a>
  <a href="./topics/paging"><small>07 · MEMORY</small><strong>按需分頁、置換與 Swap</strong><span>缺頁、FIFO 與回復路徑</span></a>
  <a href="./topics/io"><small>08 · I/O</small><strong>輸入輸出與 Pipe</strong><span>裝置、FD、TTY 與管線</span></a>
  <a href="./topics/filesystem"><small>09 · STORAGE</small><strong>WUFS 檔案系統</strong><span>inode、磁區與 journal</span></a>
  <a href="./topics/applications"><small>10 · USER SPACE</small><strong>Shell 與 fan 編輯器</strong><span>使用者映像與檔案操作</span></a>
  <a href="./topics/monitoring"><small>11 · OBSERVE</small><strong>workstate 即時監測</strong><span>CPU 派發時間、RSS 與 Swap</span></a>
  <a href="./topics/writing"><small>12 · DOCUMENT</small><strong>維護文件網站</strong><span>Markdown、Vue 與發布流程</span></a>
  <a href="./topics/threads"><small>13 · THREAD DESIGN</small><strong>Thread、OS 與 CPU</strong><span>執行緒角色、硬體與實作規劃；尚未實作</span></a>
</div>

## 從互動圖看狀態變化

互動教學把呼叫者、資源與資料流放在同一張圖上；可逐步前後切換、播放／暫停與重設情境。它們是依目前 source 整理的教學模型，不是 QEMU 即時狀態，也不等同完整 OS 模擬器。

<div class="lab-grid">
  <a class="home-link-card" href="./topics/block-async"><b>01 · Block Async I/O</b><span>提交、等待、完成：磁碟工作期間，核心如何讓其他行程前進？</span></a>
  <a class="home-link-card" href="./topics/file-descriptors"><b>02 · FD 與 OFD</b><span>為什麼 dup／fork 共用讀取位置，而兩次 open 不共用？</span></a>
  <a class="home-link-card" href="./topics/paging"><b>03 · 換頁、PTE 與 Swap</b><span>RAM 不足時如何選頁？I/O 失敗後如何恢復映射？</span></a>
  <a class="home-link-card" href="./topics/journal"><b>04 · Journal 與安全替換</b><span>寫入回錯不表示資料一定沒落盤；重播如何判讀？</span></a>
</div>

## 自己啟動並觀察

在目前原始碼的專案根目錄，從 PowerShell 執行 `wsl -d Ubuntu -- make run-disk`。主機先交叉編譯，接著 OpenSBI 啟動 flash 裡的載入器，從一顆 WUFS 磁碟載入核心。看到 `wuwaos/>` 就能在同一個視窗輸入指令。 [開始使用](./topics/getting-started.md)

Windows 免安裝 v1 ZIP 提供該版本的直接開機流程，可用 `.\Start-WUWAOS.cmd` 啟動 CLI，或加上 `gui` 開啟桌面。 [免安裝版本說明](./portable-windows.md)

<div class="lab-grid">
  <a class="home-link-card" href="./topics/getting-started.html"><b>開始使用 →</b><span>取得環境、建置並啟動 QEMU</span></a>
  <a class="home-link-card" href="https://github.com/johnson931128/WUWAOS"><b>GitHub 原始碼 ↗</b><span>瀏覽核心、驅動、使用者程式與文件</span></a>
</div>

## 實作狀態與驗證邊界

| 範圍 | 狀態 | 說明 |
| --- | --- | --- |
| CLI 磁碟開機 | 2026-11-09 已通過 QEMU 驗收 | PowerShell 啟動至 CLI、新磁碟開機、重啟後讀回檔案、缺少磁碟時停止開機。 [驗收紀錄](./history.md#cli-磁碟開機-2026-11-09) |
| 2026-11-08 核心與桌面更新 | 整合編譯通過，runtime **NOT_RUN** | Block 非同步請求、Swap PTE 回復、GPU 非阻塞 present／復原、stack guard、四個終端視窗、Fan 原子替換與故障診斷。 |
| 網站視覺／互動 | 設計規範與首頁已實作，整合驗收待執行 | 網站建置、瀏覽器操作與 QEMU 驗收分別記錄。 |
| Thread 機制 | 設計草案，**NOT_IMPLEMENTED** | [OS／硬體觀念](./topics/threads.md)與 [Spec 34](./SPEC-34-threads.md) 定義單 hart、共用 Process 資源與 Thread 生命週期；尚未新增 runtime API。 |

詳細步驟、演算法及已執行／未執行的驗收見 [實作計畫](https://github.com/johnson931128/WUWAOS/blob/main/plan.md)；只看進度可讀 [精簡清單](https://github.com/johnson931128/WUWAOS/blob/main/plan_current.md)。這些原始碼更新不包含於既有 portable v1 ZIP。

<details>
<summary>完整系統規格、限制與原閱讀路線</summary>

### 目前的系統邊界

| 項目 | 現況 |
| --- | --- |
| 平台 | QEMU virt、RV64、單核心、128 MiB RAM |
| 開機 | `make run-disk` 由 flash 載入器從 WUFS0002 啟動 CLI；`make run` 和 GUI 指令由 QEMU 直接載入核心 |
| 執行模式 | S-mode 核心、U-mode 使用者程式；可選桌面與 UI 服務在 S-mode 核心端 |
| 行程 | 最多 16 個 PCB；fork／exec／exit／wait 與事件等待 |
| 核心上下文 | RV64 swtch.S；每行程保留 C 呼叫堆疊、獨立排程器堆疊 |
| 同步 | 使用者 Mutex／Semaphore；核心 owner-aware gate 保護 OFD／pipe、FS 與 VM |
| 排程 | 單核心 RR、50 ms 時間片；使用者程式可搶占，核心內不被計時器搶占，但可事件等待／合作式切換 |
| 記憶體 | Sv39 三級頁表、4 KiB 頁、按需分頁、FIFO 置換；直接開機可使用獨立 Swap 磁碟 |
| 磁碟 | runtime Block event wait；WUFS0002 journal／開機重播與原子替換；reset 確認後隔離故障裝置，不能確認仍 fail-stop |
| 操作 | Shell FD/OFD、三階段管道、檔案重導向、fan 全螢幕文字編輯器 |
| 圖形輸出 | 可選 VirtIO GPU/tablet/keyboard；1280×720 桌面、最多四個獨立 Terminal、焦點／拖曳／viewport 裁切與非阻塞 present |
| 監測 | workstate 全螢幕；每秒更新行程、CPU 派發時間、RSS／Swap 與整體 RAM |

這是目前程式碼的實作紀錄，不承諾 Linux 相容。尚無通用 ELF／Linux 相容、網路或檔案管理器。視窗 resize 只裁切固定 100×30 模型，沒有 TTY resize ABI、文字重排或 clipboard。Stack guard 尚無 emergency trap stack；Block 隔離後不自動重掛，GPU 復原失敗會停用 GUI 並保留 DMA storage。目前桌面操作見[開機章節](./topics/boot.md#桌面-terminal-與-gui-shell)，[Spec 30](./SPEC-30-graphical-terminal.md) 與 [Spec 28](./SPEC-28-native-desktop.md) 保留早期驗收。

### 原閱讀路線

| 想理解什麼 | 從這裡開始 |
| --- | --- |
| 原始碼如何分層，UI 如何經 TTY 與 kernel 溝通？ | [程式碼架構](./CODE_GUIDE.md#ui-與-kernel-的資料通道) → [Spec 33](./SPEC-33-module-architecture.md) |
| entry.S 怎麼進入 C，最後顯示提示字元？ | [開機流程](./topics/boot.md) |
| 桌面圖示如何開啟 GUI shell？ | [開機流程](./topics/boot.md#桌面-terminal-與-gui-shell) |
| 使用者程式怎麼請核心幫忙？ | [例外與系統呼叫](./topics/traps.md) |
| 行程如何建立、排程、結束與回收？ | [行程管理](./topics/processes.md) → [計時器與 RR 排程](./topics/scheduling.md) |
| Thread 與 Process 有何不同，CPU 如何執行？ | [Thread、OS 與 CPU（規劃）](./topics/threads.md) → [Spec 34](./SPEC-34-threads.md) |
| CPU 如何找到資料，缺頁時發生什麼？ | [Sv39](./topics/memory.md) → [按需分頁與交換](./topics/paging.md) |
| 輸入的文字如何存到磁碟？ | [輸入輸出](./topics/io.md) → [WUFS](./topics/filesystem.md) |
| 如何寫自己的互動程式？ | [Shell 與 fan](./topics/applications.md) |
| 如何觀察行程的 CPU 與記憶體用量？ | [workstate 即時監測](./topics/monitoring.md) |

</details>

<details>
<summary>文件組織、特殊事蹟與參考</summary>

### 文件如何組織

主題章節描述目前設計；[原始規格與驗收紀錄](./history.md) 保留開發過程。舊規格中的架構可能已被後續版本取代，閱讀時應留意每份文件的階段說明。

本站以互動教學、架構說明、函式／參數與操作指令連接觀念和實作；完整 OS 原始碼以 GitHub 連結提供，不整段匯入來源檔。OS 與文件變更提交到 main，網站發布另由專用工具處理。驗收結果是各規格記錄的當時操作，網站動畫與建置不代表重新執行過 OS 驗證。

### 特殊事蹟

2026 年 11 月 5 日收到 XERJ Corpus Hub 來信，表示選中 WUWAOS 的非空白磁碟保護實作，擬以 MIT 授權並附來源的方式作為程式參考。

![XERJ Corpus Hub 於 2026 年 11 月 5 日寄出的 WUWAOS 程式參考通知郵件截圖](./images/wuwaos-xerj-corpus-hub-email-2026-11-05.png)

[查看原圖](https://raw.githubusercontent.com/johnson931128/WUWAOS/main/docs/images/wuwaos-xerj-corpus-hub-email-2026-11-05.png)

### 參考與致意

閱讀版面與循序章節安排參考 [《1000 行打造作業系統》](https://operating-system-in-1000-lines.vercel.app/tw/)。本文內容依 WUWAOS 自己的程式碼整理，沒有複製該書文章。作業系統概念參考 repository 的 `notes/` 課程筆記，各章另列對應檔案；筆記全文不包含在網站中。

研究矩陣與呈現規範見[全站改版研究](./redesign.md)及[視覺教材設計規範](./visual-system.md)。

</details>
