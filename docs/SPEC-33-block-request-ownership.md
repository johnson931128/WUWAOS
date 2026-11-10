# Spec 33：磁碟請求所有權與完成檢查

> 2026-11-10 合併註記：本文記錄本機分支的同步原型。現行程式已移植到遠端模組架構，磁碟使用 KernelGate、event、bounce buffer 與有界 reset/offline；IRQ 與 polling 共用 `poll_device`。FS／VM 使用遠端 KernelGate；本機 mutex 留在 `kernel/proc/kmutex.*` 與主機測試作為獨立原型，未接入 runtime。現況與驗證見[合併紀錄](./merge-2026-11-10.md)。

狀態：已實作，核心編譯與 QEMU 驗證待使用者執行。使用者同意先繼續實作，Spec 32 的 PLIC 測試仍保留為待辦，沒有視為通過。

## 為什麼先處理這一層

原本 block driver 用全域 `active` 配合巨集選擇磁碟。同步、單核心且 kernel 關中斷時能運作，但若在等待磁碟時切換行程，另一條路徑可能修改 `active`。請求恢復後不能再依賴這個全域選擇器。

本項將初始化、MMIO 位址設定、提交、完成與錯誤診斷全部改成明確傳入 `BlockDevice *`；WUFS 與 swap 各自保存 queue、請求 header、status、索引及狀態。每個裝置仍只允許一筆未釋放請求，固定使用 descriptor head 0。

這是等待／喚醒的前置機制。本項仍同步等待，沒有加入行程睡眠、核心搶占或多 hart 支援，也不能宣稱已解決 lost wakeup。上層 FS／VM 的操作、借用緩衝區與行程終止生命週期，仍須在開放排程前另行保護。

## 請求狀態與演算法

```text
IDLE
  -> 檢查 SIE=0、無舊請求、used index 已消耗
  -> 填入 descriptor/header/status
  -> INFLIGHT，先發布 descriptor，再發布 avail index，最後 notify
  -> polling 或 IRQ callback 呼叫同一個 collect_completion
  -> COMPLETE
  -> 呼叫端取得結果，釋放借用緩衝區的所有權，回到 IDLE

逾時、所有權衝突或損壞完成資料 -> FAILED -> 停止核心
```

`collect_completion` 先讀取裝置發布的 used index，沒有變化便返回；有變化時執行 acquire fence，再檢查 used element 與 DMA status：

- 使用 16-bit 差值處理 index 回繞。只有一筆 outstanding request，因此差值必須是 1。
- descriptor head 必須是 0；完成長度不得超過本次可寫入範圍。
- status 必須已從初始 `0xff` 改成支援的 OK／IOERR／UNSUPP。
- 此 modern VirtIO 驅動要求成功 read 完成 512-byte data 加 1-byte status；write 完成 1-byte status。錯誤回覆允許較短長度。
- 完成索引只由此函式推進；狀態改為 COMPLETE 後，重複 IRQ／輪詢不會再消耗一次。

IRQ callback 先 ACK 裝置，再呼叫 collector；collector 不輸出、不睡眠、不切換行程，也不讓記憶體被重用。錯誤只標記 FAILED，由持有請求的同步呼叫端輸出診斷並停止核心。合法裝置 I/O 錯誤會完成請求並返回 `-1`；協定損壞則不返回，避免仍在 DMA 的記憶體被重用。

目前 kernel SIE 保持關閉，所以磁碟傳輸通常由輪詢先完成，IRQ 在回到 U-mode 後才處理。collector 兩個入口的互斥依賴單 hart、SIE=0；未來改變這個前提時必須重設同步策略。

## 保留的測試提醒

以下皆待使用者執行，沒有代跑核心編譯或 QEMU：

- [ ] Spec 32：正常磁碟順序的 IRQ 註冊、handled 增加、異常計數為零。
- [ ] Spec 32：反轉磁碟順序仍能找到正確 WUFS／swap IRQ。
- [ ] 本項：正常與反轉順序下，`ps` 顯示兩個磁碟 `state=idle`，各自 `submitted=completed`，`io_errors=0`、`protocol_errors=0`。
- [ ] 本項：`ls /` 前後 WUFS submitted／completed 同步增加，讀取正常。
- [ ] 回歸：`blank`、`damaged`、`tail` 磁碟保護案例與 GUI Terminal。
- [ ] 邊界：used index 回繞、重複通知、錯誤 descriptor ID／長度／index／status、I/O 錯誤與逾時；目前沒有故障注入入口，正常開機不能代替這些驗證。

```powershell
.\Test-FsDisk.cmd valid
.\Test-FsDisk.cmd valid --reverse-disks
```

兩次開機都在客體執行 `ps`、`ls /`、`ps`，依序觀察 IRQ 與 Block 計數，再按 Ctrl+A、放開後按 X 離開。測試工具的 PASS 仍只代表磁碟雜湊檢查，不代表 IRQ 或請求生命週期已驗證。

## 下一步的限制

真正讓磁碟等待切出 CPU 前，還需處理 FS／VM 上層互斥與借用緩衝區、swap 換頁期間的再入、等待者被 kill 時的 DMA 生命週期、原子檢查／等待／喚醒，以及無 Ready 行程時安全接收外部中斷的方式。不能直接在現有輪詢迴圈加入 `process_block_sync()`。

## 參考

- [Spec 32：PLIC 外部中斷派發骨架](./SPEC-32-plic-external-interrupts.md)。
- [VirtIO 1.2：used ring 與 block device operation](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html)。本驅動僅支援 modern MMIO，不沿用 legacy 忽略 used length 的行為。
