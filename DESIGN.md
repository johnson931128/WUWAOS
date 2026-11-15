# WUWAOS — Website Design System

> **Design proposal / source-aware baseline — 2026-11-15.** 本文件描述網站已存在的樣式 token 及建議的設計意圖。不是對所有瀏覽器畫面的通過宣告。視覺變更仍須依 `AGENTS.md` 先呈現可比較草圖並確認，才能修改既有 UI。

## Overview

### Creative North Star — The Traceable OS Atlas（可追溯的作業系統圖鑑）

視覺隱喻：**精密機械的剖面圖與可翻閱的工程圖鑑**。一個 OS 不是裝飾性的發光黑盒子，而是由清楚標註、可追蹤的部件與路徑組成的系統。這個比喻用於組織訊息，不要求擬真 3D 齒輪或浮誇藍圖背景。

**三個視覺承諾**：

- **Clear before clever**：讀者先看到關鍵機制，再看到細節與裝飾。
- **One diagram, one question**：一張圖有明確教學問題；一段高亮代表可說明的事件。
- **Every claim has a route to evidence**：必要時能回到 source／spec／限制，不把網站模擬當作真實 QEMU trace。

### Context-specific hierarchy

- **首頁（Read）**：WUWAOS 的具體定位 → 系統剖面圖 → 選定元件解說 → 繼續閱讀／原始碼；不是同等重量的 3–6 張特色卡。
- **章節（Read）**：一個清楚的問題 → 主要圖解 → 按順序的技術論述 → source／limits／further reading。既有七段結構及長篇內容完整保留。
- **Lab（Operate）**：主要系統模型 → 操作控制 → 當前狀態／事件結果 → compact source inspector；不能讓參數表單比機制本身醒目。

閱讀欄寬以原規範約 76–84ch 為參考；大型架構圖可加寬，但不要連帶拉長所有正文。空間用來區分內容層級，不是以巨大空白或多張面板堆疊營造「高級感」。

### Distinctive visual signature

- 原創、清楚的 **User space / Kernel / Hardware 剖面架構**。
- 節點固定位置，選取後在旁側顯示對應說明與來源；事件路徑才出現高亮。
- 專有名詞、暫存器、位址欄位與路徑以節制的 monospaced 技術標註呈現。
- 視覺主角是概念的結構關係；不是漸層、貼紙、數據徽章、玻璃卡或動畫。

## Colors

### Current coded palette — light theme

以下為 `docs/.vitepress/theme/style.css` 既有設計變數；這些是**現有實作**，不是本草案聲稱新調整的配色。

| Role | CSS token | Value | Use |
| --- | --- | --- | --- |
| Canvas | `--os-canvas` | `#f4f6f8` | 頁面底色，降低大面積對比壓力 |
| Surface | `--os-panel` | `#ffffff` | 可閱讀白色內容表面 |
| Ink | `--os-ink` | `#202b36` | 標題、正文、重點資料 |
| Secondary text | `--os-muted` | `#536271` | 補充敘述、說明 |
| Boundary | `--os-line` | `#d6dee4` | 需要辨識的真實分區邊界 |
| Accent | `--os-accent` | `#23665e` | 主要互動及可信操作狀態 |
| Accent surface | `--os-accent-soft` | `#e3f0ed` | 已選取背景，搭配明確文字狀態 |
| Focus | `--os-focus` | `#176e9b` | Keyboard focus outline |
| Warning / Error | `--os-warning` / `--os-error` | `#805600` / `#a62d31` | 有真實語意的警告／錯誤 |

### Concept colors — stable semantic roles

沿用 `docs/visual-system.md` 已定義的角色，不允許按當頁審美任意調換：CPU／排程 amber、Memory teal、Kernel／權限 indigo、Process／Context violet、I/O blue、Storage ochre、Fault coral。相應 token：`--os-cpu`、`--os-memory`、`--os-kernel`、`--os-process`、`--os-io`、`--os-storage`、`--os-fault`。

- **顏色必須有語意**；不得讓彩色元件數量代替資訊層級。
- 同一角色跨章節一致；也要搭配名稱、圖例、線型或符號，不能單靠顏色判斷。
- 深色主題沿用 `style.css` 的 `.dark` token 覆寫。若重新設計深色外觀，需獨立檢查可讀性，不能只做簡單反相。
- 避免 purple glow、彩虹漸層、AI beige、假的綠色「live」狀態點。即使已有類似裝飾，也應先檢查是否傳遞資訊。

## Typography

### Existing type stacks

- **Interface / Traditional Chinese**：`'Segoe UI', 'Microsoft JhengHei', system-ui, sans-serif`（沿用 `--vp-font-family-base`）。
- **Code / Technical labels**：`'Cascadia Code', Consolas, monospace`（沿用 `--vp-font-family-mono`）。
- 不為了 AI 生成的「編輯風格」加入無關斜體襯線字，不任意安裝字型或 CDN 依賴。

### Functional hierarchy

- 文章正文目標 **16–18px**、行高 **1.6–1.8**，容納中英文混排。
- 重要圖例、按鈕、側欄、互動狀態與 SVG 真正顯示後的標籤，以 **14–16px** 為基準；必要資訊不得為了塞版面而低於 14px。
- 標題放大必須對應真實資訊重要性；一個畫面只保留一個明確的最高層重點。
- 英文關鍵術語初次出現可配繁體中文譯名；`satp`、`sfence.vma`、`sepc`、`PTE`、`TrapFrame`、路徑與程式碼使用原字串與 monospace。
- 不用無意義的全大寫 eyebrow、幽靈色細字、過度裝飾性 italic 取代內容。
- 長指令和十六進位字串可局部捲動／換行設計；不能因應窄版而縮到難以閱讀。

## Elevation

### Flat by default, boundaries by meaning

- 大部分教材內容保持平面，靠字級、留白、距離、對齊與語意色面形成層次。
- 需要表示分區時才用淺邊框；不將每個句子、數字與標籤放在自己的卡片內。
- 現有半徑：`--os-radius-control: 6px`、`--os-radius-panel: 10px`、`--os-radius-workspace: 16px`。遵循既有 scale，不新增任意的 24px/32px rounded blob。
- 現有面板陰影：`--os-shadow-panel: 0 1px 2px rgb(25 42 55 / 5%)`。浮層、可操作工作區或需要表達遮擋關係時才適度使用。
- **禁止單邊裝飾性強調線／色條**（包含 pseudo-element）。改用整面柔和底色、文字、圖示與真正的分區邊界。

## Components

### 1. System Atlas — 首頁辨識主角

- 以分層關係顯示 User / Kernel / Hardware，不描繪不存在的元件。
- `architecture-board` 是互動圖本體；`module-detail` 是所選元件的短說明、章節入口及 source 位置。
- Hover 提示可點、Click 和 Keyboard 確認選取；selected 狀態除了顏色還應有邊界／名稱／狀態文字。
- 一次「事件路徑」只顯示可辨識的真實／簡化關係；提供 caption 標明**非 QEMU 即時監測**。
- 畫面寬度不足時改為架構圖與說明上下排列，先保留可讀文字，再安排局部水平捲動的圖面。

### 2. Teaching Diagram — A/B/C/D 類型

繼承 `docs/visual-system.md`：

- **A 靜態圖**：直接閱讀結構、名稱、權限邊界；沒有為裝飾而設的播放按鈕。
- **B 漸進圖**：上一個／下一個／重設，當前步驟與圖上高亮完全同步。
- **C 事件圖**：資料狀態改變才動；沒有永遠循環的假硬體傳輸。
- **D 模型操作**：有限且有說明的輸入；對照實際核心配置，不讓模型操作被誤認為改動 OS。

每張圖須有簡潔 caption、可讀 SVG 文字、必要的替代描述與 source mapping。節點不應因每一步更換解說而跳位置。

### 3. Reading Chapter — 文件主版型

- 顯示目前章節與閱讀位置；保留 sidebar／搜尋／章節導覽。
- 每段內容以小標、段落、圖表與程式碼的閱讀節奏組織，不把長文刪掉換成卡片摘要。
- `Concept`、`Mechanism`、`In WUWAOS`、`Limitations` 使用一致而簡潔的語言；標籤須幫助辨識真實差別。
- 將 source code、教學假設與例外情形清楚分離，不用彩色提示箱淹沒正文。

### 4. Lab Controls & Inspector

- 真按鈕才呈現按鈕外觀；狀態燈必須對應可定義的狀態；靜態 tag 不可偽裝成可點擊 chip。
- 每次操作均有：`Action → state change → visible difference → explanation → relevant source`。
- Step controls、Reset、disabled、loading/error 使用一致語意；`aria-pressed`、`focus-visible`、文字／`aria-live` 依實際元件需求處理。
- 一次只突出一項目前事件；選擇不同狀態不應使整頁發生顯著 layout shift。

### 5. Responsive & Motion

- 行動版優先保留主標、主圖、操作及解說；不得將關鍵資訊藏入 hover。
- SVG 可以局部放大／捲動；整頁不得出現橫向溢出。
- 動畫支援中斷與重設；用 `opacity`／`transform` 或恰當 SVG 屬性，避免多餘 reflow。
- 基礎互動沿用現有 `--os-motion-fast: 140ms` 作參考；複雜圖解依內容節奏定義。**Motion expresses a change of state, never an ambiance.**
- 不自動播放；`prefers-reduced-motion` 下保留相同資訊與狀態差異。

## Do's and Don'ts

### Always

1. **先寫清楚讀者當頁的任務**，再決定圖、文、控制的優先順序。
2. **圖解承載教學主角**；能以固定圖解決的內容，不做整頁 simulator。
3. **先核對 source** 再畫箭頭、標註呼叫、宣稱機制；區分 Implemented／Teaching model／Observed／Planned。
4. **以必要的最少元素完成任務**；保留既有內容及驗證紀錄，不以刪除技術資訊換取極簡。
5. **每個互動元素都符合外觀預期**；鍵盤與 touch 皆能操作。
6. **先看實際畫面**：桌面、手機、亮／暗模式、最長文案、最大標籤；檢查 rendered SVG text。
7. **重大設計選擇先可視化並確認**；依 `AGENTS.md` 的決策、Git 與驗收程序執行。

### Never

1. 不使用通用 SaaS Hero＋三張平權 feature cards 作為 WUWAOS 的預設首頁。
2. 不製造假 telemetry、假 live dot、虛構 QEMU trace、不可點的假按鈕。
3. 不用紫色霓虹／巨型光暈／米白斜體雜誌／粗黑陰影／貼紙拼貼等 AI 模板潮流代替產品辨識度。
4. 不用單邊色條、卡片套卡片、重複色彩徽章或滿版漸層裝飾。
5. 不以極小字體、縮圖或被遮蔽的標籤換取整齊。
6. 不把所有章節變成單一巨型互動元件；不重寫 framework 或更動 OS 核心只為達到視覺概念。
7. 不以 `npm run docs:build` 代替瀏覽器視覺、可用性或真實 OS runtime 驗證。

### Acceptance test for each redesigned page

讀者是否能快速回答：**這頁在說什麼？目前要看哪裡？點擊會發生什麼？這項機制在 WUWAOS 何處實作？哪部分只是教材示意？**

任何一題不清楚，先 `/critique`、`/clarify` 或 `/distill`，不先加動畫。

---

本系統保留現有 `docs/visual-system.md` 的專業技術契約。此文件是 Impeccable 可讀的六段式視覺上下文；是否更新實際 tokens、風格與元件，必須先通過專案審查。
