# 視覺教材設計規範

## 編排與資訊層級

首頁為系統地圖與學習路線。十二個主章以 Concept Overview、Architecture Diagram、Step-by-step Explanation、Implementation Details、Source Code Mapping、Failure Cases / Limitations、Further Reading 排列。既有長文、表格、指令、媒體與歷史證據保留；詳細文字位於主要圖像之後，不以刪除內容換取留白。

## 圖像語言

| 類型 | 適用情境 | 控制 |
| --- | --- | --- |
| A 靜態架構 | 系統分層、PCB、磁碟布局、模組關聯 | 說明／來源、放大檢視；沒有為互動而播放的按鈕 |
| B 漸進圖 | Boot、Trap、Context、Sv39 walk、FD sharing | 同圖逐步高亮，前後／重設，顯示步驟與目前 source |
| C 動態系統圖 | RR、DMA、Swap、journal | 事件推進才改變狀態，動作發生時才有資料流效果 |
| D 可操作模型 | quantum 比較、VA 選擇、錯誤情境 | 有限、可解釋的輸入；顯示模型假設，不能暗示修改了 OS |

色彩固定表示角色：CPU／排程為 amber，memory／frame 為 teal，kernel／權限為 indigo，process／context 為 violet，I/O 為 blue，storage／持久性為 ochre，錯誤為 coral。顏色搭配名稱、線型、標記或圖例，不能單靠紅綠判斷。深色模式改變表面與文字對比，角色意義不變。

SVG 使用固定 viewBox、可讀文字與原創座標，箭頭方向表達實際呼叫／資料方向，邊界表示 privilege／ownership。必要時局部放大，不用縮小字體塞入整張圖。每張圖有 caption、替代說明、source 對照；SVG title/id 應為每個元件實例唯一。

## 排版與互動

- 字型沿用系統中文字型，程式名用 monospace；章節文字寬約 76–84ch，圖解可使用較寬範圍。首頁採專用教材版型，保留搜尋、sidebar 與章節導覽。
- 播放／暫停、前後步驟、重設為共用元件。預設不 autoplay；切換情境、參數及離開頁面時清除 timer。步驟是純資料／計算結果，不用動畫時間猜測 OS 狀態。
- SVG 大圖以有標題的可捲動容器與放大／縮小／還原操作呈現；手機優先堆疊文字與細節。只允許圖像局部捲動，整頁不能水平溢出。
- `prefers-reduced-motion` 停用移動／轉場，保留每步的完整靜態圖。按鈕具有文字、disabled／pressed 狀態與 focus-visible，敘述以 aria-live 更新。
- Scheduling 教學允許修改 quantum，但顯示「WUWAOS 實際固定 50 ms」；Sv39 位址選擇僅查教學表，不讀實際 QEMU RAM。

## 技術正確性與驗收

圖像先對 source：`kernel/proc/`、`arch/riscv/`、`kernel/mm/`、`kernel/io/`、`kernel/fs/` 與 `drivers/virtio/`。不得為了圖像方便把 xv6 的 trampoline、SMP lock、xv6 block cache 或其他 OS 功能畫進 WUWAOS。

網站驗收涵蓋建置、desktop/mobile、light/dark、控制行為、參數改動、scenario reset、搜尋、歷史／媒體保留與 console。只新增教學元件，不新增測試程式碼；由實際瀏覽器操作記錄結果。OS runtime 仍為既有 NOT_RUN，不由網站驗收推論。
