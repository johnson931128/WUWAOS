# 後續實作計畫

目前整體狀態：規劃完成，第 1 階段完成，第 2–6 階段未開始。Spec 33 的原始碼分層工作已完成。

## 1. 多 TTY 與 session 物件化

- 進度：完成；目前 UI 仍是單一 Terminal view。
- 實作內容：`kernel/io/tty.c` 管理 16 個 endpoint slot：slot 0 是固定 UART，slot 1–15 可建立 GUI session。每個 endpoint 獨立保存 input/output ring、active 狀態、raw owner、CR/LF 狀態、consumer/context 與引用數。handle 的低 32 位編碼 slot+1，高 32 位為 generation；generation 不回繞，達上限的 slot 不再配置。`OFD_TTY` 保存 handle；同一 OFD 的 dup/fork 增加 OFD refs，只有 OFD 首次建立時 retain TTY，最終 close 時 release TTY。GUI 的 fd 0 使用可讀 OFD，fd 1/2 共用可寫 OFD；核心診斷只按 fd 1 的 TTY endpoint 路由。process/session service 位於 kernel，UI 只提供 output consumer 和視圖 context。
- 解決觀念與演算法：把可變 endpoint 身分與儲存 slot 分開，handle 每次重用都帶新世代，避免舊 FD/consumer 對新 session 生效。endpoint 結束時先標 inactive、解除 consumer 並清除輸入/raw 狀態；既有 OFD refs 仍可安全釋放，但讀寫會回錯，slot 直到最後一個 ref 離開才回收。callback 呼叫後重新驗證 handle、active 與 consumer/context，避免 callback 關閉或重用 slot 後寫到新 endpoint。輸入、輸出 ring 都以每端點 read/write counters 索引；輸出先進 endpoint ring，再由已綁定 consumer 分段批次 drain，滿載回短寫／錯誤，不跨端點 fallback。raw owner 與 canonical CR/LF 過濾狀態也依 endpoint 查找。
- 核心流程虛擬碼：

  ```text
  resolve(id):
      slot = low32(id) - 1; generation = high32(id)
      if slot 越界或 endpoint 未使用或 generation 不符: 回 stale/error
      return endpoint[slot]

  create_session(consumer, context):
      找未使用且 generation 未達上限的 slot
      generation += 1; 初始化獨立 queues/mode/consumer; refs = UI owner 1
      回傳 (generation << 32) | (slot + 1)

  attach_process(id):
      建立 input OFD、output OFD，各 retain(id) 一次
      全部成功後再替換 fd 0/1/2；fd 1 和 fd 2 指向同一 output OFD
      失敗則釋放新 OFD，舊 fd table 保持不變

  close_session(id):
      先 drain 可交付 output
      endpoint.active = false; consumer = null; 清 input/raw owner
      阻塞讀者輪詢後以 ERR_IO 喚醒；UI 發現 inactive 後 release owner ref
      UI owner 與所有 OFD refs 都釋放時才清 slot；下次重用 generation 再加一
  ```

- 驗收結果：增量 `make -q` 完成，無待重建目標；QEMU GDB 以兩個真實 shell endpoint 驗證 generation ID、各自 input ring、raw owner/CRLF 隔離、fd 1 跨端點路由、dup/fork OFD ref 計數。兩個 shell 均進入 `WAIT_IO` 後關閉 A，GDB 觀察 A 以 `-5` 喚醒、B 仍為 `WAIT_IO`；舊 handle 失效，slot 最後釋放後重用時 generation 遞增。GUI QMP 驗收確認 pipe、檔案重導向與讀回、Fan 儲存/退出、workstate 返回、視窗隱藏後重開保留 session，以及 shell exit 後新 session。UART-only QEMU 確認 shell、pipe 與檔案重導向/讀回。停機後 host 唯讀驗證兩個 scratch WUFS metadata 均有效，`tty.txt` 與 `uart.txt` 內容為 `persist\n`。原始 `build/wuwaos.img` 與 swap hash 未變。
- 驗收記錄：`build/tty-phase1-runtime-gdb.log`、`build/tty-phase1-gui-session.log`、`build/tty-phase1-gui-apps.log`、`build/tty-phase1-session-lifecycle.log`、`build/tty-phase1-pipe.log`、`build/tty-phase1-uart-serial.log`；互動畫面為 `build/tty-phase1-hidden.png`、`build/tty-phase1-reopen-same.png`、`build/tty-phase1-new-session.png`、`build/tty-phase1-pipe-redirect.png`、`build/terminal-fan-active.png`、`build/terminal-fan-saved.png`、`build/terminal-workstate-running.png`、`build/terminal-query-routing.png` 及 `build/wuwaos-terminal-workstate-return.png`。
- 限制：GDB fork/ref 檢查確認 fork 增加 OFD references，但該子行程在父 session close 測試前被清除，因此未驗證「leader 結束、存活 child 繼續持有同一 endpoint OFD」整段生命週期。UI 尚未支援同時顯示多視窗；本階段只完成核心多 endpoint 物件和單一 view 綁定。

## 2. 統一事件等待與非同步 I/O

- 進度：未開始。
- 實作內容：統一 I/O 完成、逾時與取消的事件表示，以及阻塞與喚醒路徑。
- 方法與完成判準：沿用單 hart、核心不可搶佔假設；驗證阻塞操作能依完成、逾時或取消一致恢復，且沒有忙等或錯誤喚醒。

## 3. GPU 錯誤降級與裝置復原

- 進度：未開始。
- 實作內容：處理 GPU 錯誤並保留 dirty 區域以便重畫，建立裝置復原與明確診斷路徑。
- 方法與完成判準：DMA／佇列確認安全停止後才 reset/recover，否則保守停用裝置或停機；驗證故障不會誤報成功、不會重用裝置仍在存取的緩衝區，並且磁碟提交後不盲目重試。

## 4. 資源所有權、引用回收與核心堆疊保護

- 進度：未開始。
- 實作內容：整理 TTY、事件與同步資源所有權／引用計數，驗證 exit/exec 清理，為核心堆疊加入 guard page。
- 方法與完成判準：透過實際頁表映射變更實作 guard page；驗證最後一個引用離開後回收資源，越界核心堆疊存取會觸發保護故障。

## 5. 視窗管理

- 進度：未開始。
- 實作內容：在 TTY/session 分離後加入多視窗管理：建立／關閉、焦點、拖曳、縮放、遮擋與合成。
- 方法與完成判準：讓各視窗分別綁定 endpoint；驗證視窗顯示、焦點輸入及畫面更新互不混淆。

## 6. 檔案原子替換、錯誤語意與統一診斷

- 進度：未開始；此工作不屬於第 1 階段。
- 實作內容：整理檔案安全暫存與原子替換路徑、寫入錯誤語意，統一核心與裝置診斷資訊。
- 方法與完成判準：替換結果只呈現完整舊版或完整新版；提交後回錯時，明確回報結果不確定狀態及診斷資訊。

## 進度維護

每完成一個步驟後，更新本檔與 [`plan_current.md`](./plan_current.md) 的進度。依使用者要求，本檔須記錄實際採用的解決觀念／演算法、必要虛擬碼、驗收結果與限制。未執行或未完成對應 runtime 驗證的項目不得記為完成；若當前條件無法進行 runtime 驗證，需明確保留未完成狀態並記錄限制。
