# Spec 19：工作目錄與命令處理

## 目標與範圍

讓使用者在 WUWAOS 的 shell 使用 `cd` 系列與 `pwd`，並理解命令如何進入核心。既有檔案命令和 fan 支援相對與絕對路徑，更新主題文件與網站；不新增測試程式碼。

## 設計

每個 PCB（行程控制區塊）持有目前及上一次工作目錄，初始 shell 從 `/` 開始。spawn、fork 與 pipe 子行程複製父目錄，exec 保留。目錄改變只影響該行程；不是全系統共用目錄。`cd` 與 `cd ~` 指向 `/home/user`，`cd ~/path` 由 shell 展開，`cd -` 回前一個目錄並顯示結果；`pwd` 顯示目前目錄。目錄不存在或不是目錄時保留原設定。

核心新增 chdir 與 getcwd 系統呼叫，檔案及 fan 執行呼叫將相對路徑接上呼叫行程的目錄，保留 `.`、`..` 讓 WUFS 逐層查找。`cd` 成功查找並確認是目錄後才正規化儲存路徑，避免 `missing/..` 錯誤通過。`ls` 無參數列出目前目錄。絕對路徑仍從根目錄查找。完整合併後路徑最多 127 bytes；shell 無引號或含空白路徑解析。

## 步驟與驗收條件

1. 更新 PCB、路徑合併與目錄檢查，提供使用者 syscall。
2. 加入 shell 命令，更新 help、程式碼導覽、主題網站與 README。
3. 建置後以獨立磁碟在 QEMU 操作，檢查 `pwd`、`cd`、`cd ..`、`cd -`、`cd ~/path`、相對檔案操作、不存在及非目錄錯誤、子行程繼承及 exec、fan 保存與重啟後檔案保留；原磁碟不被驗證操作修改。

## 驗收紀錄

Clang 建置通過；使用獨立 WUFS 與 swap 磁碟在 QEMU 互動確認上述目錄切換、錯誤保留目錄、mkdir/touch/write/cp/ls/cat/rm、spawn 的 PCB、fork/exec 的 counter PCB、fan 相對路徑保存及重新啟動後讀回；亦確認原本的 writer | reader 可運作。新 shell 重啟從 `/` 開始。尚未逐一涵蓋所有長路徑邊界與所有終端。

參考使用者筆記 `notes/Chapter 13 file system Interface.md` 中的目錄與路徑觀念；此處敘述的是 WUWAOS 自己的簡化實作，不宣稱 Linux 相容。

## 提示字元更新

shell 每次等待命令前讀取目前工作目錄：根目錄顯示 `wuwaos/>`，`/note` 顯示 `wuwaos/note>`；讀取失敗則顯示 `wuwaos?>`。切換成功或失敗後提示字元應反映行程實際位置。

驗證：使用獨立磁碟在 QEMU 確認根目錄、`cd note`、切換失敗、`cd ..`、`cd` 與 `cd -` 的提示字元。
