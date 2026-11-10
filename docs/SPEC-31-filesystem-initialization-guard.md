# Spec 31：WUFS 非空白磁碟保護與固定驗證入口

日期：2026-11-05。

狀態：程式碼與操作工具已實作；使用者於 2026-11-05 確認本項驗證完成。

## 問題與目標

原本 kernel 的 `fs_init` 與主機的 `make_disk.py` 都會在 WUFS magic 不符時進入格式化。格式標記不符可能代表檔頭損毀或其他格式，不能作為清除既有資料的依據。

本階段將自動初始化限制為可確認空白的媒體。空白的定義是全部 bytes 皆為零，不包含全 `0xff` 或只有第一個磁區為零的映像。

## Kernel 判斷流程

1. 初始化時清除掛載狀態，取得容量並讀取第一個磁區。
2. magic 為 `WUFS0001` 時，沿原有 superblock 欄位檢查與掛載流程處理。
3. magic 不符且容量回報為 `UINT32_MAX` 時，拒絕自動格式化。底層容量介面會飽和到這個值，不能據此保證已覆蓋完整裝置。
4. 其他未知格式由 `disk_is_blank` 逐磁區只讀掃描：任何非零資料代表非空白；任何讀取錯誤代表無法判定。
5. 僅完整掃描成功且全部為零，才能呼叫 formatter。格式化失敗時清除掛載狀態。

掃描函式的回傳契約為 `1` 全零、`0` 發現資料、`-1` 讀取失敗。最壞時間 O(磁碟 bytes)，額外 kernel stack 空間 512 bytes。正常 WUFS 掛載不做全碟掃描。

這是開機期間的檢查，檢查與格式化之間不能有其他寫入者。工程註解記錄了此條件、回傳契約、完整掃描原因與容量截斷限制。

原始碼：[kernel/fs.c](https://github.com/johnson931128/WUWAOS/blob/800a805/kernel/fs.c)。

## 主機映像工具

一般啟動會先經過 `make disk-image`，因此只修改 kernel 不足以保護映像。`tools/make_disk.py` 現在對既有映像採以下規則：

- magic 相符：依原有 WUFS 更新流程處理。
- magic 不符但整個映像皆為零：可建立新 WUFS。
- magic 不符且存在任何非零資料：以錯誤結束，不寫回映像。
- 映像不存在：可建立新的 WUFS 映像。

主機工具沿用整個檔案讀入記憶體的實作；kernel 則使用單一磁區緩衝區。兩者都不能把已清零的檔頭當成整個映像為空白。

原始碼：[tools/make_disk.py](https://github.com/johnson931128/WUWAOS/blob/main/tools/make_disk.py)。

## 固定測試入口

在含本次修正與 portable 工具環境的專案根目錄，使用 PowerShell 執行：

```powershell
.\Test-FsDisk.cmd valid
.\Test-FsDisk.cmd blank
.\Test-FsDisk.cmd damaged
.\Test-FsDisk.cmd tail
```

這是專案內的 CMD 入口，不是需要先定義的 PowerShell 函式。每次執行會自動編譯 `all` 與 `build/kernel-disk.elf`，不需要另貼底層 Bash 或 Make 環境設定指令。

工具會在 `build/fs-check-案例-隨機名稱` 建立新的測試磁碟與 swap，直接交給 QEMU，不使用平常的 WUFS／swap 映像。它不經 `disk-image` target，以免主機預先格式化而測不到 kernel；也不使用 `-snapshot`，避免磁碟寫入被暫存 overlay 隱藏。

| 案例 | 初始資料 | 預期 kernel 訊息 | 結束後磁碟 |
| --- | --- | --- | --- |
| `valid` | 正常 WUFS | `Mounted existing WUFS volume` | SHA-256 不變 |
| `blank` | 16 MiB 全零 | `Initializing blank disk as WUFS` | SHA-256 改變且開頭為 `WUFS0001` |
| `damaged` | 正常 WUFS 的第一個 512-byte 磁區被清零 | `Unknown or damaged filesystem on nonblank disk; refusing format` | SHA-256 不變 |
| `tail` | 全零映像只在最後一個 byte 放 `0xa5` | 同上，拒絕格式化 | SHA-256 不變 |

請人工確認開機訊息，只執行 `help`／`ls` 等讀取操作。按 **Ctrl+A**，放開後按 **X** 結束 QEMU，工具才會比較前後雜湊並顯示 `PASS` 或 `FAIL`。非空白拒絕掛載案例仍應能進入 shell，檔案系統操作則可能返回未掛載錯誤。

`PASS` 只代表工具所檢查的磁碟內容條件，不能替代掛載訊息及 shell 行為的人工確認。每次使用新磁碟，空白案例可重複執行；工具會保留測試檔案供檢查。

原始碼：[Test-FsDisk.cmd](https://github.com/johnson931128/WUWAOS/blob/main/Test-FsDisk.cmd)、[tools/test_fs_disk.py](https://github.com/johnson931128/WUWAOS/blob/main/tools/test_fs_disk.py)。

## 驗證狀態與限制

- 使用者於 2026-11-05 確認本項驗證完成；實際執行由使用者負責，尚未留存逐案例終端輸出及雜湊值。本確認不延伸為下列故障注入或主機工具分支已通過的證據。
- 主機工具的拒絕覆寫分支需另用獨立非空白映像執行，確認錯誤退出且前後雜湊不變；四個 QEMU 案例不會代測該分支。
- 尚未完成讀取錯誤、容量飽和與格式化中途失敗的故障注入驗收。底層 I/O 逾時仍依原有停止核心策略處理。
- 不新增 journal、fsck、檔案救援或完整 superblock／inode 一致性檢查；已辨識的 WUFS 仍使用既有驗證範圍。
- 文件網站的建置與發布只驗證文件產物，不代表執行了核心或互動測試。
- 本次修正及固定測試入口在 portable v1 之後；既有 Release ZIP 不隨文件網站更新而改變。
