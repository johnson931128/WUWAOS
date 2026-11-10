# 12 · 維護文件網站

一般說明使用 Markdown（`.md`）；需要展示狀態、資料移動或資源所有權時，在文章內嵌 repository 管理的 Vue 元件。VitePress 負責產生靜態頁面，互動在讀者瀏覽器中執行，不需要另外的網站後端。

## 章節與視覺化規約

每個主題依「實際問題 → 筆記觀念 → 模組與資料位置 → 可操作示意 → 欄位／參數／函式 → 錯誤與限制」編排。先說明為何需要設計，再列出名稱與程式碼對照，避免只展示動畫而沒有解釋。

- 大型概念、狀態機與資料搬移使用 HTML/CSS/Vue，採明確的教學步驟資料與 DOM 區塊，不依賴 Mermaid 自動排版。Mermaid 只用於程式碼依賴和小範圍流程，既有歷史規格的圖保留。
- 動畫預設停止，提供前後單步、播放／暫停、重設與適合該題的情境切換。切換與離頁時停止 timer；文字狀態不可只用顏色表示，需支援鍵盤、窄螢幕與 reduced-motion。
- 模型上明示「教學示意」，例子中的 PID、offset 或磁區不冒充真實 trace；時序為步驟而非效能量測。依 source 說明實際限制，不把動畫走完當成 OS 驗收成功。
- 使用者筆記可依授權直接引用，標記檔名與章節，並將引用概念與 WUWAOS 的實作差異分開。筆記中引用第三方圖片不代表本站取得公開轉載權；沒有明確授權則重新繪製概念。新靜態生成圖片須先交使用者審閱再發布。
- 互動元件放在 `docs/.vitepress/theme/components/`，章節以局部 script setup import 使用，按章節將元件和文字一起 commit。Markdown HTML 僅用於 repository 作者管理的內容，不建立線上上傳或任意內容執行入口。

維護時同時更新函式／參數表、source 連結與模型步驟。這一輪依使用者要求只做必要文件建置與部署狀態確認，瀏覽器互動／視覺驗收標為 NOT_RUN，不新增測試程式碼。

## 安裝與預覽

主機需要 Node.js 與 npm。本次文件工具為 VitePress 1.6.4；請以 repository 的 lockfile 安裝相同依賴。

網站與 OS 原始碼都在 repository 的 `main` 分支。若要另外建立工作目錄，可使用 `git clone --branch main --single-branch git@github.com:johnson931128/WUWAOS.git WUWAOS-docs`，之後進入該目錄再執行本章命令。

`package.json` 另固定 Vite 為 6.4.5，避開 VitePress 預設 Vite 5 依賴的已知開發伺服器問題；升級時也要重新確認建置、預覽與依賴檢查。

在專案根目錄 PowerShell 執行：

```powershell
npm.cmd ci
npm.cmd run docs:dev
```

打開終端印出的 Local 網址，預設為 `http://127.0.0.1:5173/`。如果連接埠已占用，以實際印出的網址為準。修改文章並儲存後，瀏覽器會更新。

`npm.cmd ci` 用於首次安裝或同步 lockfile；平常直接執行 docs:dev 即可。重新安裝前先停止預覽，避免 Windows 因使用中的 esbuild.exe 無法替換而回報 EPERM。

在 Windows PowerShell 執行這些指令。按 Ctrl+C 停止預覽。

## 新增一個主題

1. 在 `docs/topics/` 新增 `.md` 檔，以既有章節為範本。
2. 在 `docs/.vitepress/config.mts` 的 chapters 加入標題與檔名。
3. 寫清楚問題、目前設計、來源檔、QEMU 操作與限制。
4. 執行建置，並在瀏覽器確認導覽與文章顯示。

```powershell
npm.cmd run docs:build
npm.cmd run docs:preview
```

preview 預設為 `http://127.0.0.1:4173/`，顯示建置後的靜態網站。產物位於 `docs/.vitepress/dist/`，不納入 Git。

## 原始碼只連到 GitHub

文章不整段匯入 C／組合語言來源檔。教學可列出函式簽名、參數定義和必要虛擬碼，完整實作則連到 GitHub 已提交版本。尚未提交的實作要明確標示，不能建立不存在的檔案連結。架構、互動模型、函式名稱與操作指令保留在網站。

一般內部連結使用相對 Markdown 路徑，例如 `[記憶體](./memory.md)`；VitePress 會轉成網站連結並檢查不存在的頁面。

## 現行文件與歷史規格

網站現在支援 Mermaid：Markdown 的 mermaid 圍欄會在瀏覽器轉成流程圖。圖表語法仍由 GitHub 文件來源管理，套件隨網站建置並由 GitHub Pages 提供，不呼叫外部圖表服務；深淺色切換會重畫。一般 C／組合語言原始碼仍只提供 GitHub 連結。

`docs/topics/` 解釋目前實作；`docs/SPEC-*.md` 保存各階段原始規格與驗收紀錄。新功能完成後要更新主題章節，再於[歷史索引](../history.md) 加入新規格，不要把舊驗收數字改成新版本結果。

課程筆記的檔案名稱以文字標示。本站沒有直接發布 `notes/` 全文；若未來要加入下載，應先確認內容與發布範圍。

## 公開網站與發布權限

公開網址為 [WUWAOS 實作紀錄](https://johnson931128.github.io/WUWAOS/)。GitHub repository 是唯一的文章與網站程式碼來源，GitHub Pages 只提供靜態內容。公開網站沒有編輯、上傳、登入後台、來源切換或接收內容寫入的 API，也沒有部署憑證。

修改與發布必須具備 GitHub repository 寫入權限；單純知道網址、下載網頁或 fork repository，不會取得修改本站的權限。瀏覽器仍必須下載生成後的 HTML／CSS／JavaScript，不能把這些前端產物隱藏。

維護者確認文件後，在本機 PowerShell 執行發布工具：

```powershell
powershell -File tools/publish-docs.ps1
```

先將文件與網站設定的來源變更提交並推送到 `main`，再執行發布工具。工具只把建置後的靜態網站發布到 `gh-pages`，不會替使用者提交來源變更或推送其他分支，也不接受網頁訪客傳入來源或目的地。工具使用獨立 Git index（暫存索引），不切換分支、不更動既有暫存內容，也不 force push。

## Google 搜尋

網站提供靜態 HTML、canonical（正式網址標記）及 sitemap.xml（網站地圖），沒有要求登入或禁止索引。這些讓搜尋引擎有機會抓取，不代表 Google 已收錄或保證排名。

專案子路徑下的 robots.txt 不是網域根目錄的爬蟲規則；根目錄 robots.txt 不存在時，Google 仍可抓取。若要主動提交網站地圖，可在 Google Search Console 驗證網站後提交 `https://johnson931128.github.io/WUWAOS/sitemap.xml`。本次不代替使用者登入 Google 或宣稱已提交。

官方參考：[VitePress 文件](https://vitepress.dev/guide/what-is-vitepress)、[部署說明](https://vitepress.dev/guide/deploy)。
