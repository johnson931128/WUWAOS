---
aside: false
---

<script setup lang="ts">
import { ref } from 'vue'
import SystemOverview from './.vitepress/theme/components/SystemOverview.vue'
import HomeTerminal from './.vitepress/theme/components/HomeTerminal.vue'
import { useScrollFx } from './.vitepress/theme/composables/useScrollFx'

const rail = ref<HTMLElement | null>(null)
// Scroll-driven reveal (#074) + timeline rail that grows and lights each chapter
// as it enters view (#145). Purely presentational; chapters remain real links.
useScrollFx({ revealSelector: '[data-reveal]', rail, railItemsSelector: '.wuwa-rm-item' })
</script>

<div class="wuwa-home">

<section class="wuwa-home-hero" aria-label="WUWAOS 作業系統介紹">
  <div class="wuwa-hero-copy" data-reveal>
    <p class="wuwa-hero-overline"><span class="wuwa-live-dot" aria-hidden="true"></span> <b>WUWAOS</b><span class="wuwa-sep">/</span> RISC-V 64 · 可啟動的實驗型作業系統</p>
    <h1>從一行 <span class="wuwa-accent">echo hello</span>，看見整套作業系統如何把指令送到螢幕。</h1>
    <p class="wuwa-hero-lead">沿著 WUWAOS 真實的 C / RISC-V 原始碼，逐步拆解開機、Trap、行程、排程、記憶體與檔案系統——一套能在 QEMU 上啟動、可逐層理解的小型 OS。</p>
    <div class="wuwa-hero-actions">
      <a class="wuwa-hero-primary" href="./topics/getting-started.html">開始探索 <span aria-hidden="true">↗</span></a>
      <a class="wuwa-hero-secondary" href="https://github.com/johnson931128/WUWAOS">原始碼 <span aria-hidden="true">↗</span></a>
    </div>
    <dl class="wuwa-hero-facts" aria-label="平台簡介">
      <div><dt>Arch</dt><dd>RV64</dd></div>
      <div><dt>Core</dt><dd>1 HART</dd></div>
      <div><dt>Runtime</dt><dd>QEMU virt</dd></div>
      <div><dt>Impl</dt><dd>C + Assembly</dd></div>
    </dl>
  </div>
  <HomeTerminal />
</section>
<p class="wuwa-page-note">以實際 WUWAOS 原始碼為依據 · 終端與圖示為<b>教學示意</b>，非 QEMU 即時追蹤。</p>

## 系統地圖

以下架構依目前原始碼繪製，不把教材概念、網站互動模型或其他 OS 的元件誤當成 WUWAOS 實作。選取模組可查看角色、來源與對應章節。

<SystemOverview />

<section class="wuwa-block" aria-labelledby="roadmap-title" data-reveal>
  <p class="wuwa-kicker"><span class="wuwa-idx">01</span> LEARNING ROADMAP</p>
  <h2 id="roadmap-title">一條循序閱讀的路線</h2>
  <p class="wuwa-lead">從開機一路走到維護文件本身。每一章接上原始碼、限制與相關課程筆記；沿著這條路線讀，概念會一層層落到實作上。</p>
  <nav class="wuwa-roadmap" aria-label="學習路線">
    <div class="wuwa-rail" :ref="rail" aria-hidden="true"><div class="wuwa-rail-fill"></div></div>
    <a class="wuwa-rm-item" href="./topics/getting-started.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">01</span><span class="wuwa-rm-kind">Orient</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">開始使用</span><span class="wuwa-rm-desc">主機、工具鏈與 QEMU</span></span></a>
    <a class="wuwa-rm-item" href="./topics/boot.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">02</span><span class="wuwa-rm-kind">Boot</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">開機流程</span><span class="wuwa-rm-desc">從 OpenSBI、磁碟載入器到 CLI</span></span></a>
    <a class="wuwa-rm-item" href="./topics/traps.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">03</span><span class="wuwa-rm-kind">Boundary</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">例外與系統呼叫</span><span class="wuwa-rm-desc">trap、ecall 與 ABI</span></span></a>
    <a class="wuwa-rm-item" href="./topics/processes.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">04</span><span class="wuwa-rm-kind">Process</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">行程管理</span><span class="wuwa-rm-desc">建立、切換、結束與回收</span></span></a>
    <a class="wuwa-rm-item" href="./topics/scheduling.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">05</span><span class="wuwa-rm-kind">CPU</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">計時器與 RR 排程</span><span class="wuwa-rm-desc">ready queue、派發與時間片</span></span></a>
    <a class="wuwa-rm-item" href="./topics/memory.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">06</span><span class="wuwa-rm-kind">Memory</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">實體頁與 Sv39</span><span class="wuwa-rm-desc">虛擬位址、頁表與權限</span></span></a>
    <a class="wuwa-rm-item" href="./topics/paging.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">07</span><span class="wuwa-rm-kind">Memory</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">按需分頁、置換與 Swap</span><span class="wuwa-rm-desc">缺頁、FIFO 與回復路徑</span></span></a>
    <a class="wuwa-rm-item" href="./topics/io.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">08</span><span class="wuwa-rm-kind">I/O</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">輸入輸出與 Pipe</span><span class="wuwa-rm-desc">裝置、FD、TTY 與管線</span></span></a>
    <a class="wuwa-rm-item" href="./topics/filesystem.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">09</span><span class="wuwa-rm-kind">Storage</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">WUWAOS 檔案系統</span><span class="wuwa-rm-desc">inode、磁區與 journal</span></span></a>
    <a class="wuwa-rm-item" href="./topics/applications.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">10</span><span class="wuwa-rm-kind">User space</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">Shell 與 fan 編輯器</span><span class="wuwa-rm-desc">使用者映像與檔案操作</span></span></a>
    <a class="wuwa-rm-item" href="./topics/monitoring.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">11</span><span class="wuwa-rm-kind">Observe</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">workstate 即時監測</span><span class="wuwa-rm-desc">CPU 派發時間、RSS 與 Swap</span></span></a>
    <a class="wuwa-rm-item" href="./topics/writing.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">12</span><span class="wuwa-rm-kind">Document</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">維護文件網站</span><span class="wuwa-rm-desc">Markdown、Vue 與發布流程</span></span></a>
    <a class="wuwa-rm-item" href="./topics/threads.html"><span class="wuwa-rm-side"><span class="wuwa-rm-num">13</span><span class="wuwa-rm-kind">Thread design</span></span><span class="wuwa-rm-main"><span class="wuwa-rm-title">Thread、OS 與 CPU</span><span class="wuwa-rm-desc">執行緒角色、硬體與實作規劃；尚未實作</span></span></a>
  </nav>
</section>

<section class="wuwa-block" aria-labelledby="labs-title" data-reveal>
  <p class="wuwa-kicker"><span class="wuwa-idx">02</span> INTERACTIVE LABS</p>
  <h2 id="labs-title">從互動圖看狀態變化</h2>
  <p class="wuwa-lead">把呼叫者、資源與資料流放在同一張圖上：逐步前後切換、播放／暫停與重設情境。它們是依目前 source 整理的教學模型，不是 QEMU 即時狀態，也不等同完整 OS 模擬器。</p>
  <div class="wuwa-labs">
    <a class="wuwa-lab-entry" style="--role: var(--os-io)" href="./topics/block-async.html">
      <span class="wuwa-lab-top"><span class="wuwa-lab-idx">LAB 01 · Block Async I/O</span><span class="wuwa-lab-glyph" aria-hidden="true">⇄</span></span>
      <h3>磁碟工作期間，核心如何讓其他行程前進？</h3>
      <p>提交、等待、完成：caller 進入 event wait，device lane 完成，scheduler poll 收取。</p>
      <span class="wuwa-lab-cta">進入互動圖 <span aria-hidden="true">→</span></span>
    </a>
    <a class="wuwa-lab-entry" style="--role: var(--os-process)" href="./topics/file-descriptors.html">
      <span class="wuwa-lab-top"><span class="wuwa-lab-idx">LAB 02 · FD 與 OFD</span><span class="wuwa-lab-glyph" aria-hidden="true">↔</span></span>
      <h3>誰保存檔案的目前位置？</h3>
      <p>為什麼 dup／fork 共用讀取位置，而兩次 open 不共用？FD 到 OFD 的引用數變化。</p>
      <span class="wuwa-lab-cta">進入互動圖 <span aria-hidden="true">→</span></span>
    </a>
    <a class="wuwa-lab-entry" style="--role: var(--os-memory)" href="./topics/paging.html">
      <span class="wuwa-lab-top"><span class="wuwa-lab-idx">LAB 03 · 換頁、PTE 與 Swap</span><span class="wuwa-lab-glyph" aria-hidden="true">⇅</span></span>
      <h3>RAM 不足時如何選頁？I/O 失敗後如何恢復映射？</h3>
      <p>FIFO victim、PTE 撤銷、swap 寫入與 rollback 回復，逐步看 PTE、RAM 與 slot 狀態。</p>
      <span class="wuwa-lab-cta">進入互動圖 <span aria-hidden="true">→</span></span>
    </a>
    <a class="wuwa-lab-entry" style="--role: var(--os-storage)" href="./topics/journal.html">
      <span class="wuwa-lab-top"><span class="wuwa-lab-idx">LAB 04 · Journal 與安全替換</span><span class="wuwa-lab-glyph" aria-hidden="true">⇶</span></span>
      <h3>寫入回錯不表示資料一定沒落盤；重播如何判讀？</h3>
      <p>追蹤 staging、journal、commit header 與 home sectors，看 durable boundary 在哪。</p>
      <span class="wuwa-lab-cta">進入互動圖 <span aria-hidden="true">→</span></span>
    </a>
  </div>
</section>

<section class="wuwa-block" aria-labelledby="run-title" data-reveal>
  <p class="wuwa-kicker"><span class="wuwa-idx">03</span> RUN IT</p>
  <h2 id="run-title">自己啟動並觀察</h2>
  <p class="wuwa-lead">在目前原始碼的專案根目錄，從 PowerShell 執行 <code>wsl -d Ubuntu -- make run-disk</code>。主機先交叉編譯，接著 OpenSBI 啟動 flash 裡的載入器，從一顆 WUFS 磁碟載入核心。看到 <code>wuwaos/&gt;</code> 就能在同一個視窗輸入指令。 <a href="./topics/getting-started.html">開始使用</a></p>
  <p class="wuwa-lead">Windows 免安裝 v1 ZIP 提供該版本的直接開機流程，可用 <code>.\Start-WUWAOS.cmd</code> 啟動 CLI，或加上 <code>gui</code> 開啟桌面。 <a href="./portable-windows.html">免安裝版本說明</a></p>
  <div class="wuwa-labs">
    <a class="wuwa-lab-entry" style="--role: var(--os-kernel)" href="./topics/getting-started.html">
      <span class="wuwa-lab-top"><span class="wuwa-lab-idx">GETTING STARTED</span><span class="wuwa-lab-glyph" aria-hidden="true">↗</span></span>
      <h3>取得環境、建置並啟動 QEMU</h3>
      <p>工具鏈、磁碟開機流程與第一次看到 <code>wuwaos/&gt;</code>。</p>
      <span class="wuwa-lab-cta">開始使用 <span aria-hidden="true">→</span></span>
    </a>
    <a class="wuwa-lab-entry" style="--role: var(--os-cpu)" href="https://github.com/johnson931128/WUWAOS">
      <span class="wuwa-lab-top"><span class="wuwa-lab-idx">SOURCE ↗</span><span class="wuwa-lab-glyph" aria-hidden="true">⌘</span></span>
      <h3>GitHub 原始碼</h3>
      <p>瀏覽核心、驅動、使用者程式與文件。</p>
      <span class="wuwa-lab-cta">到 GitHub <span aria-hidden="true">→</span></span>
    </a>
  </div>
</section>

<section class="wuwa-block" aria-labelledby="status-title" data-reveal>
  <p class="wuwa-kicker"><span class="wuwa-idx">04</span> STATUS &amp; BOUNDARIES</p>
  <h2 id="status-title">實作狀態與驗證邊界</h2>
  <p class="wuwa-lead">網站動畫與建置不代表重新執行過 OS 驗證；下表保留「已驗收」與「NOT_RUN / NOT_IMPLEMENTED」的界線。</p>
  <table>
    <thead><tr><th>範圍</th><th>狀態</th><th>說明</th></tr></thead>
    <tbody>
      <tr><td>CLI 磁碟開機</td><td>2026-11-09 已通過 QEMU 驗收</td><td>PowerShell 啟動至 CLI、新磁碟開機、重啟後讀回檔案、缺少磁碟時停止開機。 <a href="./history.html#cli-磁碟開機-2026-11-09">驗收紀錄</a></td></tr>
      <tr><td>2026-11-08 核心與桌面更新</td><td>整合編譯通過，runtime NOT_RUN</td><td>Block 非同步請求、Swap PTE 回復、GPU 非阻塞 present／復原、stack guard、四個終端視窗、Fan 原子替換與故障診斷。</td></tr>
      <tr><td>網站視覺／互動</td><td>設計規範與首頁已實作，整合驗收待執行</td><td>網站建置、瀏覽器操作與 QEMU 驗收分別記錄。</td></tr>
      <tr><td>Thread 機制</td><td>階段 A 已實作，多 Thread API NOT_IMPLEMENTED</td><td><a href="./thread-stage-a.html">PCB／TCB 與排程拆分驗證</a>；目前每 Process 一條 Thread。<a href="./topics/threads.html">教學</a>與 <a href="./SPEC-34-threads.html">Spec 34</a> 說明後續建立／join 設計。</td></tr>
    </tbody>
  </table>
  <p class="wuwa-lead">詳細步驟、演算法及已執行／未執行的驗收見 <a href="https://github.com/johnson931128/WUWAOS/blob/main/plan.md">實作計畫</a>；只看進度可讀 <a href="https://github.com/johnson931128/WUWAOS/blob/main/plan_current.md">精簡清單</a>。這些原始碼更新不包含於既有 portable v1 ZIP。</p>
</section>

<section class="wuwa-block" aria-labelledby="appendix-title" data-reveal>
  <h2 id="appendix-title" class="wuwa-appendix-h2">附錄</h2>
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
</section>

</div>
