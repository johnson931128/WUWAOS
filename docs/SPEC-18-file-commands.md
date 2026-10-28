# Spec 18：ls、rm、cp 與 cls

## 目標與範圍

使用者希望在 WUWAOS 使用常見 Linux 風格命令，並確認 cls 清除畫面與終端捲動歷史、保留磁碟資料。保留現有 ls，新增無參數 ls 列根目錄；實作 rm、cp、cls，更新 spec、程式碼導覽與網站。僅支援絕對路徑與一般檔案，沒有 cwd、選項、空白檔名、遞迴刪除或複製到目錄。未經同意不新增測試程式碼。

## 命令與架構

- `ls` 相當於 `ls /`；`ls /absolute/path` 沿用現有目錄列舉。
- `rm /absolute/file` 經 SYS_FS_REMOVE 複製檢查路徑後，尋找父目錄和目錄項目；拒絕目錄、根目錄、`.`、`..` 與不存在的檔案。將最後一項移到刪除位置並縮短目錄，然後釋放檔案直接／間接區塊和 inode bitmap，供後續重用。
- `cp /absolute/source /absolute/destination` 經 SYS_FS_COPY 複製檢查兩條路徑，來源與目的皆為一般檔案路徑；目的檔若存在會覆寫，但比較 inode 編號拒絕複製到相同檔案（包含不同寫法指向同 inode）。不接受目錄作為目的地或遞迴複製。
- cp 先配置未公布的新 inode，逐 512 bytes 從來源資料區複製到新資料區，全部完成並儲存 inode 後才新增／替換目錄項目；覆寫成功後才回收舊目的 inode。空檔、二進位資料、直接與單層間接區塊皆適用；建置階段失敗會嘗試回收新 inode。覆寫需同時容納新、舊兩份資料，磁碟不足時目的檔不先清空。
- `cls` 是 shell 內建，透過既有 write 系統呼叫輸出 ESC [2J（清畫面）、ESC [3J（清捲動歷史）、ESC [H（游標回左上）；實際歷史清除取決於宿主終端支援，不清除 WUFS／RAM 資料或使用者輸入佇列。[Microsoft 的 Windows Console 文件](https://learn.microsoft.com/en-us/windows/console/clearing-the-screen)說明 2J 與 3J 的用途。
- WUFS 尚無 journal（交易日誌）或崩潰恢復。目錄項目可跨磁區，磁碟錯誤或斷電在更新中途可能留下重複目錄項或未回收區塊；回報錯誤不代表一定沒有可見變更。若發布階段寫入失敗，保守保留新 inode，避免把可能已被連結的區塊釋放。底層現有配置失敗路徑並非完整交易，亦可能洩漏。不能宣稱斷電原子性。
- cp 使用一個 512-byte 核心緩衝區，不依賴 cat 的 2048-byte 顯示上限。每個區塊可能掃描磁區 bitmap，完整操作成本取決於檔案大小與可用磁碟區塊數。

## 驗收條件

1. WSL 建置成功；使用獨立 QEMU 磁碟，驗證建立／列舉、複製內容、覆寫、相同檔案與目錄拒絕、刪除、資源重用與重啟持久性。
2. 驗證跨 inode 與目錄項目磁區、空檔與超過直接區塊範圍的二進位檔案；確認來源未受影響、錯誤時不誤刪目的檔。
3. 檢查 cls 實際輸出控制序列與終端畫面，獨立記錄是否能觀察宿主 PowerShell 的捲動歷史。
4. 更新 README、CODE_GUIDE、主題網站與驗收紀錄，適當驗證後 commit/push、發布網站。

## 來源

`notes/Chapter 13 file system Interface.md` 的 directory entries（目錄項目）、檔案刪除與樹狀目錄章節；現行 WUFS 結構見 SPEC-06／SPEC-10，命令並非完整 Linux 相容介面。

## 驗收紀錄

2026-10-28，在 WSL Ubuntu／Clang 18／QEMU 8.2.2／OpenSBI 1.3 的獨立 WUFS 與 Swap 映像執行，未動使用者原始磁碟，也未新增測試程式碼。

- make 通過；`ls` 可見 bin／boot／dev／home，建立檔案、覆寫、複製已有目的檔後讀回 hello_wuwa。別名 `/home/user/notes/../notes/a.txt` 指向來源時回 -22，來源仍完整；目的目錄與 rm 目錄回 -21，不存在檔案回 -2。空檔複製回 0 bytes。
- 建立 20 個目錄項，刪除多項並確認其餘名稱；目錄第 18 項 offset=504，橫跨兩個 512-byte 磁區，cp 覆寫再 rm 該項後，尾項移入且內容正確。另刪除、建立與回收 inode 25（offset=464，跨磁區），相鄰檔案仍可讀。
- cp `/boot/kernel.elf` 到普通檔案共 338440 bytes，主機離線讀取獨立 WUFS 後比對 SHA-256：來源及副本均為 `78b6ca7cea3c8e9155d0f2b9588d6089661c88f57fbe105d748a8ff3db5a0c0a`。檔案跨多個間接指標。重新啟動後刪除大型副本，檢查其原有 668 個資料／索引區塊有 667 個空閒、1 個被後續小檔重用，原 inode 28 亦被新檔重用。
- 複製磁碟映像並將可用區塊縮至 11，cp 大檔覆寫既有小檔回 -28，原目的檔仍為 hello_wuwa；離線觀察剩餘 11 個區塊與 30 個已分配 inode，均恢復到操作前數量。這是空間不足路徑驗證，不代表磁碟中途 I/O 錯誤具備交易原子性。
- `cls` 後 QEMU 畫面清除、提示字元回到頂端，序列記錄包含 ESC[2J、ESC[3J、ESC[H，檔案仍可讀。嘗試開啟獨立 Windows PowerShell 做原生捲動歷史檢查時，被本機產品政策阻擋；未能實測實際 PowerShell scrollback 是否清除。
- 不支援相對路徑、目錄複製、遞迴 rm、選項、引號及空白檔名；未注入斷電或部分磁區 I/O 失敗。程式碼與磁碟映像紀錄留在被忽略的 build/ 供本機查閱。
