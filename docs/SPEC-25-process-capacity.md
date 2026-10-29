# Spec 25：擴充行程容量

## 目標與範圍

將 WUWAOS 同時存在的行程欄位從 4 擴充為 16（含 shell），保留固定 PCB、首次符合的線性尋找與現行 RR；確認建立、額滿錯誤、狀態、資源回收及既有 I/O／同步功能。行程 Zombie 仍占欄位直到回收。未經同意不加入測試程式碼。

## 設計

`PROCESS_LIMIT` 為單一上限，核心 PCB、每欄 16 KiB 的固定核心堆疊、VM 的每行程資訊及 workstate 快照都依其調整；每行程仍按需配置使用者頁。建立時由陣列前端找第一個 Unused，最壞 O(N)；Ready 佇列及時間片 RR 演算法不變。建立失敗需保有既有失敗回收行為，exit 先 Zombie，wait 回收 VM 和 PCB。VM 多保留一份 exec 用空間，仍是固定配置。

workstate 以每頁最多 8 筆顯示，多頁時 n／p 切頁、q 返回；即使只顯示一頁，快照包含所有行程，計算欄寬及數據不省略其他行程。兩份快照存放在 shell 的靜態資料區，而非 4 KiB 使用者堆疊，以避免上限增加時堆疊耗盡。終端較窄時仍可能換行。

## 執行與驗收

建置並在獨立 QEMU WUFS／swap 磁碟，建立 15 個 worker 加 shell，確認第 17 個行程無法建立；workstate 可切頁顯示所有 PID；回收一部分後再建立、喚醒與全部回收，觀察 free PCB、實體頁與 swap 統計。另確認 demo sync、pipe、fan 或現有檔案命令、VM 操作正常；檢查 workstate 在一般終端大小下可操作。更新 README、文件網站、程式碼導覽，保留歷史規格原狀。

## 限制

固定行程表與堆疊不是動態配置，並不意味著 16 個 CPU 同時運作；仍為單核心。同步物件、pipe 及可使用記憶體另有獨立限制。驗收範圍見下方紀錄。

參考 `notes/ch3 Process.md` 的行程狀態和 PCB，以及 `notes/ch5 CPU schedueling.md` 的就緒佇列；此處為 WUWAOS 具體實作。

## QEMU 驗收紀錄

WSL Clang 建置通過，使用獨立 `build/capacity-check.img` 與 `capacity-swap.img`，未使用使用者原磁碟。shell 加 15 個 worker 可同時存在，第 17 個行程回 -11；`ps` 顯示 Free PCB slots: 0。`workstate` 實際顯示 16 個行程的兩頁，n／p 切頁、q 返回。kill／wait 回收兩個，再建立 PID 17／18 重用欄位；全部喚醒及回收後剩 shell、空閒 PCB 為 15。另在 PCB 全滿時成功對 counter fork 後 exec；15 個子行程回收後 free pages 31401（初始 31402，shell 額外載入一頁），swap slots 0。

`demo sync`、writer | reader、mkdir／touch／write／cat 均成功；常駐預算設 8 並填 12 頁後，swap_in=7、swap_out=14，讀回第 0 頁首／中／尾為 65，mem free 後 heap 資源回收；保留 2 個 shell swap backing slots，並非聲稱 swap 全部清空。較窄終端、記憶體耗盡及各種 I/O 故障未測。
