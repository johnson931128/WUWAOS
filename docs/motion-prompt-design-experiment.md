---
title: Motion Prompt 動效設計紀錄
outline: [2, 3]
---

# Motion Prompt 動效設計紀錄

本頁記錄 WUWAOS 網站改版中**實際使用**的動效：每個效果的 Motion Prompt ID、所在位置、目的、時程／緩動、參考網址。所有效果都遵守同一條界線——**只呈現真實的轉場與狀態，不模擬不存在的 kernel 行為，也不把教學圖說成 QEMU 即時追蹤**。

動效語彙取自 [Motion Prompt](https://showheyohtaki.github.io/motion-prompt)（MIT License，© 2026 Shohei Ohtaki）。每個 ID 對應一個有規格（時程、緩動、距離、角度）的效果；本頁的實作保留這些數字，只替換屬於 WUWAOS 的顏色、文字與元素。

## 共用的運動語言

全站共用兩條緩動曲線（來自 Motion Prompt kit），讓所有效果讀起來像同一套系統：

| Token | 值 | 用途 |
| --- | --- | --- |
| `--wuwa-ease-out` | `cubic-bezier(0.22, 1, 0.36, 1)` | 元素進入、落定（kit `eOut`） |
| `--wuwa-ease-io` | `cubic-bezier(0.65, 0, 0.35, 1)` | 對稱的進出、狀態切換（kit `eIO`） |
| `--wuwa-dur-reveal` | `0.7s` | 卷動揭示的時程 |
| `--wuwa-dur-hover` | `0.3s` | 懸停回饋的時程 |

## 首頁

### #006 Typewriter（終端打字）

| 項目 | 內容 |
| --- | --- |
| 位置 | `HomeTerminal.vue` — Hero 右側教學終端 |
| 目的 | 讓「輸入 `echo hello`」這件事有**正在發生**的順序感：先打指令，再出現輸出，最後顯示 U→ecall→S→UART 的邊界。這是全站最長的「主角」動畫，直接點題「從一行 echo hello 看見整套 OS」。 |
| 時程／緩動 | 首字 `0.3s`、每字 `0.24s`（保留 spec 數字）；每個字由 `scale 1.35 → 1` 落定（`wuwa-type-pop .28s --wuwa-ease-out`）；輸出與 stages 分段淡入（`.5s` / `.6s`，eOut）。 |
| 保留／替換 | 保留 spec 的 `0.24s/char`、`1.35→1` 縮放、`0.94s` 游標明滅週期；把 spec 的「送信按鈕彈出」換成 WUWAOS 的 `hello` 輸出與邊界 caption。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#006> |

### #154 Spotlight（終端聚光）

| 項目 | 內容 |
| --- | --- |
| 位置 | `HomeTerminal.vue` — 終端框 `.wuwa-console::before` |
| 目的 | 深色終端上，只把**游標所在的那一小圈**照亮，讓視線跟著指標走；不做整框發光或粒子。這是「pointer 下的位置站出來」，而非裝飾性 glow。 |
| 時程／緩動 | `radial-gradient(120px circle at var(--spot-x) var(--spot-y))`，由 `pointermove` 直接驅動（無 keyframe，零延遲跟手）；`pointerleave` 回到預設 `50% / 32%`。 |
| 保留／替換 | 保留「只有游標周圍一圈被照亮」的核心；把 spec 的卡片邊框發光換成終端內的一層表面光（`rgba(255,255,255,0.14)`）。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#154> |

### #074 Fade-in on scroll（卷動揭示）

| 項目 | 內容 |
| --- | --- |
| 位置 | `index.md` 各 `[data-reveal]` 區塊；邏輯在 `composables/useScrollFx.ts`（IntersectionObserver，trigger `85%`） |
| 目的 | 首頁是一條「往下讀」的敘事（Hero → 系統地圖 → 路線 → 實驗室 → 啟動 → 狀態）。區塊依序浮現，把讀者的注意力分段送進下一層，而不是整頁一次出現。 |
| 時程／緩動 | `opacity 0→1` 同時 `translateY(28px) → 0`，`0.7s --wuwa-ease-out`（保留 spec 的 28px 位移與 eOut）。 |
| 保留／替換 | 保留 spec 的 `85%` 觸發線、`28px` 上移、`0.7s` eOut；用 `IntersectionObserver` 替換 spec 的演示用卷動軸。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#074> |

### #145 Timeline grow & light（學習路線軌道）

| 項目 | 內容 |
| --- | --- |
| 位置 | `index.md` 學習路線 `.wuwa-roadmap`；`.wuwa-rail-fill` 由 `useScrollFx` 依卷動深度寫 `--wuwa-progress`，`.wuwa-rm-item.is-lit` 點亮圓點 |
| 目的 | 13 章是一條**時間順序**的學習路徑（Boot → Trap → Process → … → Document）。左側軌道隨著閱讀向下生長、圓點依序點亮，把「讀到第幾層」直接畫出來。 |
| 時程／緩動 | 軌道填充 `height: var(--wuwa-progress)`（卷動驅動，linear 跟手）；圓點 `is-lit` 切換 `0.3s --wuwa-ease-out`（背景／邊框變 brand 色）。 |
| 保留／替換 | 保留「線往下長、到達的點被填滿」的核心；把 spec 的年份／事件換成章節標題，並讓**卷動向上可逆**（progress 隨卷動雙向更新）。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#145> |

### #086 Link underline（章節標題底線）

| 項目 | 內容 |
| --- | --- |
| 位置 | 學習路線 `.wuwa-rm-title::after`（`hover` / `focus-visible` 時） |
| 目的 | 安靜地指出「目前指到哪一章」，不用整卡變色就讓可點擊性被看見。 |
| 時程／緩動 | `scaleX(0) → scaleX(1)`，`0.3s --wuwa-ease-out`；懸停時 `transform-origin: left`（由左長出），離開時回到 `right`（往右縮回）。 |
| 保留／替換 | 保留 spec「由左長出、往右縮回」的底線；把橙色換成 brand 色。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#086> |

### #081 Card hover lift（卡片／按鈕懸停）

| 項目 | 內容 |
| --- | --- |
| 位置 | Hero 主／次按鈕（`translateY(-2px)`）、實驗室 entry 卡（`translateY(-3px)` + 背景提亮） |
| 目的 | 讓按鈕與實驗室入口「看起來可點」；只有指標下的那一個浮起，離開後落回，不做整組同動。 |
| 時程／緩動 | `0.3s --wuwa-ease-out`；lab entry 同時把 CTA 箭頭 `translateX(4px)` 提示方向。 |
| 保留／替換 | 保留「只有懸停的那個輕微浮起＋落回」的核心；用 `translateY` 替換 spec 的陰影擴張（全站刻意不用重陰影，維持工程圖面感）。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#081> |

## 全站

### #079 Scroll progress bar（閱讀進度）

| 項目 | 內容 |
| --- | --- |
| 位置 | `style.css` `.VPDoc::before`（`@supports (animation-timeline: scroll())` 內） |
| 目的 | 長文件（各章節、本頁）讀到幾成，頂端細線由左長到右。純進度指示，無其他含義。 |
| 時程／緩動 | `transform: scaleX(0→1)`，`animation-timeline: scroll(root block)`（linear，直接等於卷動比例）；`3px` brand 色。 |
| 保留／替換 | 保留 spec「fill = 卷動比例」的核心；用原生 CSS scroll-timeline 實作，**不支援的瀏覽器整段不出現**（無 JS fallback 的跳動）。 |
| 參考 | <https://showheyohtaki.github.io/motion-prompt/#079> |

### SVG flow-draw（系統地圖啟動交接）

| 項目 | 內容 |
| --- | --- |
| 位置 | `SystemOverview.vue` 兩條**實線** boot 路徑 `.flow-boot`（`韌體交接`、`_start → kernel_main`） |
| 目的 | 依 **M → S → U 的真實權限交接方向**畫出啟動流：線由上往下畫出，讓「交接是单向、有次序的」一眼可見。這是資料流的有意義動畫，不是裝飾。 |
| 時程／緩動 | `pathLength="1"` + `stroke-dasharray:1 / stroke-dashoffset:1→0`，`0.8s --wuwa-ease-out`；第二條 delay `0.5s` 形成次序。 |
| 保留／替換 | 用 `pathLength="1"` 正規化，讓不同長度的短線都以同一節奏畫出。**虛線的 ecall 邊界不參與線畫**（`stroke-dasharray:5 4` 會與 draw 衝突），改由下方 #svg-fade 處理。 |
| 參考 | 線畫（line-draw）技法；與 #006/#074 同屬 Motion Prompt 的進入類語彙 |

### SVG fade-in（ecall 權限邊界）

| 項目 | 內容 |
| --- | --- |
| 位置 | `SystemOverview.vue` 虛線 `.flow-call`（`process_start → U-mode`）與所有 `.flow-label` |
| 目的 | 交接畫完後，再淡入「使用者行程 ↔ 核心」的 ecall 邊界與標註——先把啟動方向講完，再講邊界，避免兩種資訊同時爭奪注意力。 |
| 時程／緩動 | `opacity 0→1`，`0.6s --wuwa-ease-out`，`delay 1.3s`（兩條 boot 線畫完後）。 |
| 保留／替換 | 以淡入替換線畫，因為該邊界是**虛線**，用 dash-offset 會與點線外觀打架；淡入讓虛線保持原樣。 |
| 參考 | 與 #074 同屬 fade 語彙 |

## 可近用性與還原（reduced motion）

每個效果都有**靜態終端狀態**，在 `prefers-reduced-motion: reduce` 下全部落定，不卡在中途：

- **終端（#006 / #154）**：`HomeTerminal.vue` 偵測 `prefers-reduced-motion`，直接把完整 `echo hello`、`hello`、stages 全部呈現；聚光停用。
- **卷動揭示（#074）**：`useScrollFx` 在 reduced 下**立即**對所有 `[data-reveal]` 加 `is-revealed`，`style.css` 也强制 `[data-reveal]` 可見。
- **路線軌道（#145 / #086）**：圓點底線與位移過渡在 reduced 下停用；軌道填充保留靜態終端值。
- **SVG（flow-draw / fade）**：`SystemOverview.vue` 在 reduced 下 `animation: none`，boot 線直接 `stroke-dashoffset:0`、ecall 邊界 `opacity:1`。
- **閱讀進度（#079）**：`.VPDoc::before` 在 reduced 下 `animation: none`。
- **實驗室**：四個 lab 元件在 reduced 下把過渡／動畫時程壓到 `0.01ms`，只保留**狀態的靜態呈現**。

鍵盤：所有可互動元素（系統圖模組、路線章節、實驗室控制）都有 `:focus-visible` 外框與 `focus-visible` 觸發的回饋；終端有 `aria-label` 說明「教學示意、非即時追蹤」與 sr-only 全文。

## 界線聲明（教學圖 ≠ QEMU trace）

- 終端與系統地圖都明確標註「**教學示意／依目前 source 繪製的靜態分層**」與「**非 QEMU 即時追蹤**」。
- 動效只負責**讓已存在的結構與狀態更清楚**（啟動交接方向、ecall 邊界、卷動進度、目前指到哪一章），不產生任何 OS 沒有資料來源的「即時」訊號。
- 實驗室的狀態機（Block Async、FD/OFD、Paging/Swap、Journal）依真實 WUWAOS source 整理；本頁不改變任何狀態模型，只做視覺與資訊階層調整。

## 授權

Motion Prompt 效果語彙（ID 與規格數字）取自 [Motion Prompt](https://showheyohtaki.github.io/motion-prompt)，**MIT License (c) 2026 Shohei Ohtaki**。各元件的 `<style>` 區塊保留對應的歸屬註解。
