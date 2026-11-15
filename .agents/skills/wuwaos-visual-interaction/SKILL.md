---
name: wuwaos-visual-interaction
description: Plan, critique, and refine the WUWAOS VitePress/Vue documentation website as a source-traceable operating-system atlas. Use for homepage or chapter redesign, SVG teaching diagrams, typography, information hierarchy, interaction, and responsive/a11y review. Preserves source truth and requires design approval before UI changes.
---

# WUWAOS — Traceable OS Atlas Design Skill

## Mission

設計目的不只是「做漂亮的技術網站」，而是讓 OS 機制在同一條路徑上**可理解（Understand）→ 可觀察（Observe）→ 可追溯（Verify）**。

**North Star：The Traceable OS Atlas — 可追溯的作業系統圖鑑。**

將 WUWAOS 視為一台可以展開的機器：以真實的元件位置、資料流、狀態與 source mapping 做視覺語言。不以 generic SaaS card、科幻光效、假的監控圖或浮誇互動取代內容。

### Decision hierarchy

1. **Root `AGENTS.md` = 最高優先級**：重要設計決策先提交可比較的視覺提案並取得明確確認；未授權不得新增依賴、改 UI、commit/push/deploy、開 branch/PR/worktree。
2. **`PRODUCT.md` = 產品目的與讀者任務**：產品定位、頁面類型、實作與證據邊界。
3. **`DESIGN.md` = 視覺契約**：字級、token、圖解及互動規則；不把草案誤當已施作程式碼。
4. **`docs/visual-system.md` = 技術圖解契約**：A/B/C/D 類型、語意色彩與驗收範圍。
5. **Current source = 系統事實**：真實 source 優先於舊教材敘述，並區分已驗證與 NOT_RUN。

本 Skill **不取代** Impeccable upstream skill；它是 WUWAOS 專屬的產品脈絡與執行檢核。若 Impeccable 未安裝，仍能依此流程執行；不能假稱已執行 `/impeccable` 指令或 detector。

## Before touching the UI: discover, verify, and propose

### Step 1 — Identify one surface and one reader job

先讀實際 route、元件及內容，不由截圖猜測全站結構。標記 Impeccable surface mode：

| Surface | Mode | One job | Anti-goal |
| --- | --- | --- | --- |
| Homepage | **Read**（系統圖作主角） | 看懂 WUWAOS 架構並選下一條學習路線 | Marketing carousel／數據 dashboard |
| Chapter | **Read** | 掌握一個機制並找到 source | 把完整教材縮成卡片摘要 |
| Stateful lab | **Operate** 支援 Read | 操作、觀察狀態、解釋因果 | 參數面板比學習內容醒目 |
| Specs / code mapping | **Read** | 驗證實作與邊界 | 隱去限制與 NOT_RUN |

每次工作寫出一個任務句：「進入這頁的讀者，應能 ___，並知道 ___。」

### Step 2 — Establish factual boundaries

閱讀目標章節、`docs/CODE_GUIDE.md`、對應真實源碼、`planning/plan_current.md`。需要時參考：

- `kernel/proc/`、`arch/riscv/` — thread、process、scheduler、trap。
- `kernel/mm/` — Sv39、fault、replacement、swap。
- `kernel/io/`、`kernel/fs/`、`drivers/virtio/` — I/O、FD/OFD、WUFS、device。
- `docs/.vitepress/theme/components/`、`docs/.vitepress/theme/style.css` — 網站目前真實 Vue 與 CSS。

建立**Claim → Source / Spec → Status** 對照。所有圖解與文案依事實標示為：

- `Implemented in WUWAOS`
- `Teaching model / simplified`
- `Observed in QEMU`（只在已有實際 evidence 時）
- `Planned / not implemented`

不能畫進 xv6/Linux 的獨有結構，不能把可調教學 quantum 當成實際 kernel quantum，不得宣稱網頁正在讀取真實 QEMU/RAM/TLB 狀態。

### Step 3 — Draft a brief before proposing layout

使用簡短、可評估的 Brief：

```md
Surface / route:
Mode: Read | Operate
Reader job:
Primary question:
One focal element:
Necessary proof/content:
Source-of-truth paths:
Must preserve:
Explicitly out of scope:
Viewport and accessibility constraints:
Design metaphor and its limitation:
Acceptance criteria:
```

**不准直接從「要怎麼排三張卡」開始**。先回答「讀者需要理解什麼、哪項資訊應是主角、哪個操作能讓事實更清楚」。

### Step 4 — Show real options; await decision

根據根目錄 `AGENTS.md`，先交付至少一個 Wireframe、Mockup、可比較截圖或具體參考網址。若有多種合理方案，給 **2–3 種真正不同的編排**，清楚列出：

`what changes → why it helps → what it risks → what stays intact`

不擅自選擇、實作、提交或部署。若需求已指定特定方向，不重複詢問相同決策；新冒出的重要選擇才返回確認。

## Design composition

### Prefer hierarchy, not component quantity

- 整頁一個資訊主角；先看見機制，後看見操作與補充。
- 首頁主角是 **WUWAOS 系統剖面圖**；章節主角是具體架構／資料流；lab 主角是一個同步的系統狀態。
- 元件群組依空間與語義排列；同層資訊可以並置，不同層資訊不強迫放入相同大小卡片。
- 不套用固定 `Hero + three cards + CTA`；不使用 nested-cards、膠囊狀 badge soup、假的綠色 live 點。
- 保留原始碼映射、長篇技術說明、表格、歷史 Spec、圖片／影片與下載資訊；優化閱讀順序，不以刪掉內容換整潔。

### Typography and space

- 正文目標 16–18px、行高 1.6–1.8；可見重要標籤與實際渲染 SVG 文字 >=14px。
- 沿用既有字型堆疊與 `--os-*`、`--vp-*` tokens；不得隨意引進新字型、固定深色模式或重配所有語意顏色。
- 寬圖可以加寬，正文保持舒適閱讀寬度。優先改 layout／層級／局部縮放，**不縮字硬塞**。
- 色彩只在分辨角色、狀態、風險或焦點時出現；同一個語意跨章一致。
- 除非有明確的結構意義，不要增加單邊裝飾性彩線、光暈、漸層、卡片嵌卡片及無作用裝飾。

### Diagrams as first-class teaching material

- 一張**固定架構圖**先獨立可讀；之後才考慮 active node/edge、高亮路徑或 timeline。
- 每個箭頭回答「從哪裡到哪裡、傳的是什麼、何時傳」。不要把美學連線當成真實資料流。
- 保持節點座標穩定；當前事件高亮，其他關係降噪。
- A=靜態、B=循序、C=事件狀態、D=有限模型；依 `docs/visual-system.md` 選擇，不對每張圖都加 Play。
- 每張圖有圖題、caption、文本替代描述、足夠可讀的 technical labels 與 source pointer。

## Interaction contract

針對每個可操作元件，定義：

`Trigger → Immediate feedback → State transition → Explanation/source update → End / reset / focus state`

- 外觀像 Button 的東西必須真能操作；靜態標籤不能冒充 tab；狀態燈不可無來源地顯示「正在運作」。
- Selected、disabled、loading、error 與 focus states 都必須有可見且可理解的表達。
- 控制元件支援 click/tap/keyboard；可用 `aria-pressed` 和 `aria-live`，但必須依語意確實實作。
- 不 autoplay、不無限空轉、不劫持捲動。動效僅表達狀態轉移；reduced motion 保留資訊而非刪掉流程。
- 既有 Vue/CSS/SVG 能完成的效果，不因新潮而導入大型動畫框架；真正需要時依 `AGENTS.md` 取得額外依賴的批准。

## Impeccable-inspired workflow

有安裝 Impeccable 時，以實際安裝版本支援的命令為準。**以下是工作分工，不代表本 repo 已安裝或實際執行**。

| Phase | Tool/command if available | Required outcome |
| --- | --- | --- |
| Context | `init` / `document` | 確認 `PRODUCT.md` 和 `DESIGN.md` 與 current source 一致 |
| Discover | `shape` | 頁面目標、受眾、證據、限制；不先寫程式 |
| Direction | 結構草圖與比較 | 供專案決策者確認的設計方向 |
| Create | 專案既有 Vue/CSS/SVG | 依已核准方向完成最小、真實的 UI |
| Evaluate | `critique` | 分別檢查視覺主次、認知負擔、假 affordances、AI slop |
| Simplify | `distill` / `clarify` / `typeset` | 刪除無用途裝飾，理順文字與層級 |
| Verify | `audit`、`harden` / `polish` | 實際操作、無障礙、響應式、空狀態與邊界案例 |
| Document | `document` | 經確認後將真正採用的視覺規則回寫 `DESIGN.md` |

### Two-pass critique — mandatory even without Impeccable

**Pass A: Product promise test**

1. 3–5 秒內能否指出當前頁面主題與下一步？
2. 主要的概念圖或內容是否先於裝飾吸引注意？
3. 每個互動是否都能正確反映狀態與後果？
4. 一個有疑問的技術宣稱能否追到 source 或明確標示是 model？
5. 不啟動動畫、以鍵盤或窄螢幕閱讀時是否仍可完成學習任務？

**Pass B: AI slop and UI debt test**

- 無意義的 3–6 欄 feature cards、card-in-card、面板過度包裝、裝飾性 chip。
- 缺少實際狀態的 live indicator、假按鈕、假 telemetry、僅 hover 才看見的重要資訊。
- 不屬於專案既有視覺契約的漸層字、光暈、隨機 border radii、無意義的 giant hero。
- 中文閱讀過窄或過寬、SVG 縮小字、低對比註解、錯誤的中英文與程式碼換行。
- 讓圖解和狀態說明互相衝突、誤標 WUWAOS vs 示意模型的情況。

以 P0/P1/P2/P3 記錄問題，**優先處理會誤導技術理解或無法完成操作的問題**，不要只為了達到 detector 的零警告而改動有意義的例外。

## Skills to load and source reading

網站 UI 工作需依 `AGENTS.md` 實際閱讀本地適用 Skills，而不是只在最終報告提名字：

1. `.agents/skills/frontend-design/SKILL.md` — 視覺與排版。
2. `.agents/skills/web-design-guidelines/SKILL.md` — Web convention 與 UI review。
3. `.agents/skills/accessibility/SKILL.md` — Keyboard、focus、文字對比、reduced motion。
4. 本 Skill 的 `references/site-patterns.md` 與 `references/design-review.md` — 目前參考網站與驗收規則。
5. 有狀態教學時 `.agents/skills/wuwaos-interactive-labs/SKILL.md` 與 `references/OS-LAB-CONTRACT.md`。
6. 只有需要時讀 `.agents/skills/motion-prompt/SKILL.md` 與 `.agents/skills/performance/SKILL.md`；流暢度與效能要分別驗證。

參考案例只借一個具體特點；不照搬品牌，也不因參考站很美就改掉 WUWAOS 的真實資訊架構。

## Verification and handoff

每次經授權實作 UI 後：

- `npm run docs:build`（在 repository 根目錄，依現有 package script 執行）。
- **實際瀏覽器**檢視桌面／手機、light／dark、字級、SVG、選取、鍵盤焦點、reset、reduced motion、source link、空狀態與溢出。
- 檢查讀者能否在一次操作後說出「什麼改了／為何改／source 在哪裡」。
- 分開報告 **Docs build**、**Browser UI**、**Accessibility**、**OS Runtime**；沒做的都要標 `NOT_RUN`，不能以 build success 代替 QA。
- 列出：改動檔案、已核准的決策、保持不變的教材證據、已測試情境、未驗證範圍及殘留問題。
- 除非當次明確授權，**不 commit、不 push、不部署、不新增分支、不變更 kernel/driver/user**。

## When quality is in conflict

遵循以下順序：

**技術正確性 > 學習任務完成度 > 無障礙與可讀性 > 視覺層級 > 動效精緻度 > 裝飾。**

例如：為了讓 Sv39 逐級 walk 清楚，可以放大畫布或改成上下兩段，但不可縮字、刪除 PTE 層級或畫出不存在的 TLB 行為。設計選擇應具備可追溯理由，就像 OS 的程式碼一樣。

## Attribution & scope

本檔為 WUWAOS 專案原創指引，借鑑 Impeccable 的 **design context、surface modes、shape → critique → distill / polish** 方法與 Gary Chen 影片中的操作直覺、視覺層級、克制、情境適配原則；沒有複製或安裝 upstream skill。相關參考：

- https://impeccable.style/docs/context/
- https://impeccable.style/docs/critique/
- https://impeccable.style/docs/document/
- https://www.youtube.com/watch?v=QuQ2FOznK18
