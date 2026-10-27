# 10 · Shell 與 fan 編輯器

核心提供能力，使用者程式把能力組合成操作。WUWAOS 的 shell 與 fan 都是 U-mode（使用者模式）程式，透過 syscall 操作行程、終端與檔案。

## Shell 如何執行命令

`user/shell.c` 讀入最多 126 個字元，切成最多 4 個 token（以空白分隔的單位），再比對命令。解析為 O(L)，L 是輸入長度；目前沒有引號、腳本與一般背景工作控制。

| 操作 | 主要流程 |
| --- | --- |
| ps／pcb | 進核心查詢行程狀態 |
| ls／cat／write | 以路徑呼叫 WUFS 服務 |
| run counter | fork 子行程，再 exec 指定內建程式 ID |
| writer \| reader | 建立固定管道與兩個端點行程 |
| fan 路徑 | fork → exec fan 並傳入路徑 → 父 shell 等待 |
| vmstat／mem | 查詢與操作使用者記憶體 |

命令和 syscall 沒有一對一關係。以 fan 為例，一次編輯操作可能多次讀取按鍵、輸出控制碼，最後才寫入檔案。

## 三份獨立使用者映像

Makefile 將 shell、app、fan 分別連結為 ELF，再轉成 binary 嵌入核心。`kernel/user_image.S` 保存原始內容，VM 在需要時載入對應頁面。

因此 fan 是真正的獨立行程，但還不是由 `/bin/fan` 磁碟執行檔載入。user/entry.S 將初始參數交給 user_main，返回值再交給 exit。

## fan 的文字資料結構

`user/fan.c` 用連續 byte 陣列存放最多 2048 bytes ASCII／LF 文字，cursor（游標）是陣列中的位移。

例如 `abc` 的游標在 b 前面，插入 X 時，先把後段 `bc` 向右移，再放入 X，得到 `aXbc`。刪除則反方向搬移。插入／刪除最壞 O(N)，對小型文字檔容易理解與管理，不需要複雜樹結構。

上下移動會掃描換行並保留目標欄位；按鍵解碼器逐 byte 處理 ANSI Escape 序列。畫面以逐行快取比較，只重畫改變的行。

## 一個終端上的全螢幕介面

畫面經 UART 輸出 ANSI 游標定位、清行與色彩控制碼，需至少 80×24 終端。它是 TUI（文字使用者介面），沒有 framebuffer（像素畫面緩衝區）或圖形視窗系統。

編輯器持有 raw console，shell 等待子行程，不會同時讀走你的按鍵；背景行程輸出仍可能干擾畫面，尚無完整前景工作隔離。

## 在 QEMU 編輯

```text
fan /home/user/note.txt
```

| 按鍵 | 操作 |
| --- | --- |
| 方向鍵／Home／End | 移動游標 |
| Enter／Tab | 換行／插入空白 |
| Backspace／Delete | 刪除文字 |
| PageUp／PageDown | 捲動畫面 |
| Ctrl+O | 儲存 |
| Ctrl+X | 離開；修改未儲存時詢問 y／n／c |
| Ctrl+L | 完整重畫 |

輸入文字，Ctrl+O 儲存，再 Ctrl+X 返回 shell，使用 `cat /home/user/note.txt` 讀回。新檔直到儲存才建立，父目錄必須存在。

## 限制與參考

UTF-8、超大檔案與不支援的控制字元會拒絕開啟，以免截斷覆寫。既有 Tab 以 `>` 顯示並保留，新 Tab 插入空白。目前沒有 undo、搜尋、Vim 命令模式與原子儲存。

觀念參考：`notes/ch3 Process.md`、`notes/ch12  I_O Systems.md`；終端序列依 [XTerm 文件](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)。設計與既有互動驗收見 [Spec 09](../SPEC-09-fan.md)。
