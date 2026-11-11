# WUWAOS — Agent Guidelines

本檔案適用於 WUWAOS Repository 中的 AI Coding Agents（包含 Claude Code、Codex）。
本專案的 **網站** 是一份清楚、易讀、具圖像與簡單動畫的作業系統教材，不是 Dashboard 或複雜 OS 模擬器。下列 UI 規範主要適用於 `docs/`；未經明確要求，不要修改 OS 核心功能。

## 網站設計方向

- **預設使用淺色／白色視覺風格**：清楚的背景、柔和的色塊、足夠留白及一致的文字顏色。可以保留現有主題機制，不要為了改版增加不必要的深色模式工作。
- **可讀性優先，盡量避免小字**：正文與教學解說以 **16–18px** 為基準，行高約 **1.6–1.8**；重要標籤、圖例、側欄、按鈕與輔助資訊以 **14–16px** 為基準。承載必要資訊的可見文字原則上**不得低於 14px**。標題依層級明顯放大。
- **SVG 圖中的字也算文字**：檢查實際顯示後的尺寸，不能只看 SVG 內標記的 `font-size`。圖縮小後若看不清楚，應增加圖的空間、簡化標籤、分層或允許局部放大／捲動，**不可把文字再縮小來硬塞**。
- 使用清楚的字型層級、對齊、間距與內容分組。中英文、RISC-V 暫存器名、程式名稱與程式碼應易讀；避免整頁都是極小的英文大寫 eyebrow、metadata 或難辨認的淡色文字。
- 優先使用**圖示／小型 SVG 插畫＋清楚標籤**呈現 CPU、RAM、Device、Process、Kernel 等元件。圖示是輔助辨識，不能取代必要文字；以整體架構關係為主角，不要堆滿外觀相同的矩形卡片。
- **禁止裝飾性單邊彩色邊框**：不要替卡片、說明框或面板加粗的 `border-left`、`border-top`、單側色帶或用偽元素做同樣效果。需要強調時改用柔和的整塊背景色、語意標籤、圖示與文字層級。
- 色塊須協調、對比足夠；限制不必要的光暈、漸層、粒子與花俏動畫。保持專業的 Developer Documentation／Technical Textbook 氣質。

## 教材圖解與動畫

- 採取 **一張清楚的固定架構圖＋目前節點／箭頭高亮＋一小段同步解說**。先讓靜態圖容易理解，再加入動態效果。
- Process State、Context Switch、Scheduling、Sv39 Page Table Walk 等概念優先用 SVG 元件呈現；需要時依前面章節的學習順序逐步補充，不必一次改完整站。
- 節點位置盡可能保持穩定；只動畫正在發生的狀態轉移／資料流。必要時使用上一個／下一個／播放／暫停／重設，**不要主動加入複雜參數表單、故障注入或 Dashboard**。
- 解說文字集中、簡短，技術欄位、概念說明與 Source Code Reference 清楚區分。不要把一個簡單觀念拆成許多擁擠的小卡片。
- 架構圖與流程須依 WUWAOS **目前真實原始碼**及 `docs/CODE_GUIDE.md`、`docs/visual-system.md` 等文件核對；區分實作、規劃、教學示意與實際 QEMU Trace，不得直接套用其他 OS 的未實作機制。

## Skills：網站工作前先閱讀

網站新增、重設計或大幅修改 UI 時，**先實際閱讀並使用** Repository 已有的適用 Skills（包含必要 references），不要只口頭列出名稱：

- `.agents/skills/frontend-design/SKILL.md` — 視覺方向、排版與資訊層級
- `.agents/skills/web-design-guidelines/SKILL.md` — UI／UX 審查
- `.agents/skills/accessibility/SKILL.md` — 文字可讀性、鍵盤操作與清楚的焦點
- `.agents/skills/wuwaos-interactive-labs/SKILL.md` — 修改四個互動教學頁面時使用

若 `.agents/skills/motion-prompt/SKILL.md` 已存在，也可閱讀並依實際需求選用少量動效；**檔案不存在時不必為了遵守此規則自行安裝**。Claude Code 若未自動辨識 `.agents/skills/`，直接讀取對應 `SKILL.md`。

## 進階 UI 視覺與流暢度（務必同時重視）

- **先有吸引人的外觀，再安排互動**：每次網頁改版都要先確定整體構圖、排版、留白、字級、圖示、色塊與資訊層級；避免只加動畫、懸浮效果就宣稱設計完成。選擇 2–3 個成熟網站，從不同站點各借鏡一項特點，不要照抄某個產品的版型或品牌。
- 動態操作要自然：Hover／Click／Focus 的回饋、所選節點與解說切換、可中斷的動畫都應清楚平穩。動畫應有目的、節奏一致、不改變圖中固定節點位置，也不造成版面跳動。
- **網站 UI 工作額外先讀** `.agents/skills/wuwaos-visual-interaction/SKILL.md`、`references/site-patterns.md` 與 `references/design-review.md`，在設計與互動之間作完整取捨。必要時參考其署名的 UI/UX Pro Max 規則。
- 遇到使用者反映「卡頓／不絲滑」時，先判斷是視覺轉場不自然，還是有實際效能問題。前者先改善動效設計；後者參考 `.agents/skills/performance/SKILL.md`，並以實際瀏覽器證據診斷，不可憑感覺宣稱性能提升。
- 當次任務未明確要求安裝新套件時，優先使用既有 Vue／CSS／SVG；`motion-v`、其他重型框架或套件只能在具體需求成立後再考慮。

## 實作範圍、Git 與驗證

- 保留現有 VitePress／Vue／SVG 技術及文件、導覽與原始碼連結；除非另外指定，不要為設計而遷移框架、重寫後端或修改 `kernel/`、`arch/`、`drivers/`、`user/`。
- 先處理當次指定的頁面，避免擅自擴大至其他章節；不需要檢查一大堆無關模式。介面仍需保持基本響應式、正常互動及可閱讀。
- 視覺修改後執行現有文件建置（`npm run docs:build`，環境需要時先 `npm ci`）。有瀏覽器時確認實際字體大小、圖解、溢出及操作；未執行的檢查標為 `NOT_RUN`，不可宣稱通過。
- **只在 `main` 工作**。禁止擅自建立 Branch、Pull Request、Worktree 或 Force Push；不得覆蓋未提交修改。是否 commit、push 或部署依當次任務要求執行。
