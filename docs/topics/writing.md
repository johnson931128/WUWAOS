# 11 · 維護文件網站

你平常只要修改 Markdown（`.md`）文章。VitePress 負責把標題、表格與程式碼轉成可閱讀的網頁，不需要先寫 HTML 或 CSS。

## 安裝與預覽

主機需要 Node.js 與 npm。本次文件工具為 VitePress 1.6.4；請以 repository 的 lockfile 安裝相同依賴。

`package.json` 另固定 Vite 為 6.4.5，避開 VitePress 預設 Vite 5 依賴的已知開發伺服器問題；升級時也要重新確認建置、預覽與依賴檢查。

在專案根目錄 PowerShell 執行：

```powershell
npm.cmd ci
npm.cmd run docs:dev
```

打開終端印出的 Local 網址，預設為 `http://127.0.0.1:5173/`。如果連接埠已占用，以實際印出的網址為準。修改文章並儲存後，瀏覽器會更新。

`npm.cmd ci` 用於首次安裝或同步 lockfile；平常直接執行 docs:dev 即可。重新安裝前先停止預覽，避免 Windows 因使用中的 esbuild.exe 無法替換而回報 EPERM。

這些指令在 Windows 主機執行，不是在 WUWAOS 的 `wuwaos>` 輸入。按 Ctrl+C 停止預覽。

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

文章不內嵌 OS 程式碼，不匯入 C／組合語言來源檔。需要閱讀原始碼時，使用指向 GitHub 已提交版本的連結；尚未提交的實作要明確標示，不能建立不存在的檔案連結。架構文字、函式名稱、流程圖與操作指令仍保留在網站。

一般內部連結使用相對 Markdown 路徑，例如 `[記憶體](./memory.md)`；VitePress 會轉成網站連結並檢查不存在的頁面。

## 現行文件與歷史規格

`docs/topics/` 解釋目前實作；`docs/SPEC-*.md` 保存各階段原始規格與驗收紀錄。新功能完成後要更新主題章節，再於[歷史索引](../history.md) 加入新規格，不要把舊驗收數字改成新版本結果。

課程筆記的檔案名稱以文字標示。本站沒有直接發布 `notes/` 全文；若未來要加入下載，應先確認內容與發布範圍。

## 公開網站與發布權限

公開網址為 [WUWAOS 實作紀錄](https://johnson931128.github.io/WUWAOS/)。GitHub repository 是唯一的文章與網站程式碼來源，GitHub Pages 只提供靜態內容。公開網站沒有編輯、上傳、登入後台、來源切換或接收內容寫入的 API，也沒有部署憑證。

修改與發布必須具備 GitHub repository 寫入權限；單純知道網址、下載網頁或 fork repository，不會取得修改本站的權限。瀏覽器仍必須下載生成後的 HTML／CSS／JavaScript，不能把這些前端產物隱藏。

維護者確認文件後，在本機 PowerShell 執行發布工具：

```powershell
powershell -File tools/publish-docs.ps1
```

工具先把文件、網站設定與發布腳本保存到 johnson931128/WUWAOS 的 codex/documentation 分支，再把靜態產物發布到 gh-pages，不接受網頁訪客傳入來源或目的地。使用獨立 Git index（暫存索引）與明確的文件路徑清單，不切換 main、不提交 OS 原始碼、不更動既有暫存內容，也不 force push。

## Google 搜尋

網站提供靜態 HTML、canonical（正式網址標記）及 sitemap.xml（網站地圖），沒有要求登入或禁止索引。這些讓搜尋引擎有機會抓取，不代表 Google 已收錄或保證排名。

專案子路徑下的 robots.txt 不是網域根目錄的爬蟲規則；根目錄 robots.txt 不存在時，Google 仍可抓取。若要主動提交網站地圖，可在 Google Search Console 驗證網站後提交 `https://johnson931128.github.io/WUWAOS/sitemap.xml`。本次不代替使用者登入 Google 或宣稱已提交。

官方參考：[VitePress 文件](https://vitepress.dev/guide/what-is-vitepress)、[部署說明](https://vitepress.dev/guide/deploy)。
