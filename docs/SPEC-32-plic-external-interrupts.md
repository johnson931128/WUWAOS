# Spec 32：PLIC 外部中斷派發骨架

狀態：演算法已獲同意，第一階段程式碼已實作；編譯與 QEMU 驗證待使用者執行。

## 本階段範圍

建立裝置 IRQ 到 supervisor trap 的通知通道，先以已初始化的 WUFS／swap VirtIO block 裝置接入。磁碟請求仍同步輪詢完成；不在本階段加入睡眠／喚醒、核心內搶占、鍵盤／GPU IRQ 或 SMP。

實作順序為 FDT 探索、supervisor-only MMIO 映射、註冊驅動 callback、設定 PLIC、開啟 `sie.SEIE`。保留 kernel 的全域中斷關閉規則，外部事件可在返回 U-mode 後處理。

## FDT 與映射

- 新增有界、免配置的 FDT 讀取器：DTB 最多 1 MiB、128 nodes、32 層，檢查 token、區塊範圍、名稱結尾及 properties-before-children 順序。
- 從 `riscv,plic0`／`sifive,plic-1.0.0` 的 `reg`、`riscv,ndev`、`interrupts-extended` 找到啟動 hart 的 supervisor external context，沒有寫死 context 編號。
- VirtIO 路由依 MMIO `reg`、有效 `interrupt-parent` phandle 與 `interrupts` 建立，不依磁碟掛載順序或固定 IRQ ID。
- 只映射 priority、所選 enable bank 與 threshold／claim context 所需的三個頁面，PTE 不含使用者存取權。
- 僅支援 QEMU virt 的 identity bus、CPU interrupt controller 單 cell 與 VirtIO MMIO 單 source 綁定。格式不符、來源重複或缺少可用 PLIC 時保留原輪詢模式。

## 派發與確認

`trap_handler` 識別最高位為中斷且原因碼為 9 的 supervisor external interrupt：

1. 從目前 context 的 claim register 讀取 IRQ ID；零表示沒有待處理來源。
2. 有 callback 時，呼叫驅動確認裝置事件，再將同一 ID 寫回 PLIC complete。
3. 沒有 callback 時，先 complete，再關閉該來源的 enable bit。PLIC 對已停用來源的 completion 會忽略，因此不能先 mask 再 complete。
4. 不合法 ID 不可用於陣列或暫存器索引；記錄錯誤並關閉 SEIE，保留輪詢備援。
5. 每次最多領取 32 個來源。達到上限時關閉 SEIE 並記錄 budget 計數，避免立即重入形成中斷風暴；目前只有同步磁碟來源，正常處理不應耗盡此上限。
6. 外部中斷不增加 `sepc`。U-mode 返回使用既有 `process_resume`；kernel trap 直接返回原 frame，不假設排程器目前有行程。

VirtIO block callback 讀取 `InterruptStatus`、保留已觀察的通知位元、寫入 `InterruptACK`，並使用 I/O fence 保證裝置 ACK 先於 PLIC complete。後續 [Spec 33](./SPEC-33-block-request-ownership.md) 已將完成檢查集中到共用 collector，輪詢與 callback 都能呼叫；callback 不輪詢、不輸出、不切換行程或釋放請求記憶體，`transfer` 仍持有緩衝區直到完成。執行期間調整磁碟容量不在支援範圍。

## 可觀察的驗證方式

在專案根目錄執行下列其中一種方式，自動編譯後啟動：

```powershell
# 日常 CLI；會依原有流程準備正常磁碟
.\Start-WUWAOS.cmd

# 使用獨立測試磁碟，離開後也會檢查磁碟內容
.\Test-FsDisk.cmd valid
```

開機應出現 `[irq] PLIC context=... sources=... registered=...`。預設 WUFS／swap 皆可用時，應註冊兩個來源。若看到 `retaining polling`，代表相容性備援啟動，不能當成本階段 IRQ 驗證通過。

在客體 shell 執行：

```text
ps
ls /
ps
```

`ps` 會多列出：

```text
IRQ: PLIC=enabled external=... claims=... unhandled=0 invalid=0 budget=0
  IRQ ... WUFS handled=... spurious=...
  IRQ ... swap handled=... spurious=...
```

應確認 `PLIC=enabled`，`ls /` 正常返回，第二次 `ps` 的 WUFS `handled` 比第一次增加；`unhandled`、`invalid`、`budget` 保持零。計數是通知／claim 次數，不等於磁區請求數；多筆同步 I/O 在返回 U-mode 前可能合併成一次通知。開機已完成的 I/O 也可能讓第一次計數大於零。

再反轉磁碟掛載順序，使用同樣操作確認角色與 IRQ 路由仍正確：

```powershell
.\Test-FsDisk.cmd valid --reverse-disks
```

按 Ctrl+A，放開後按 X 離開 QEMU。測試工具的 `PASS` 只確認磁碟內容，IRQ 計數仍須人工確認。GUI 可用 `.\Start-WUWAOS.cmd gui` 回歸滑鼠、鍵盤和 Terminal；這些裝置仍走原有輪詢。原有 `blank`／`damaged`／`tail` 案例可繼續回歸磁碟保護。

## 尚待驗證與下一階段

尚未執行本版核心編譯、一般／反轉磁碟 QEMU 操作、GUI 回歸或中斷風暴／未知來源故障注入。文件建置不代替這些驗證。

使用者已要求保留測試提醒並繼續實作；本項尚未驗證。[Spec 33](./SPEC-33-block-request-ownership.md) 先整理每裝置請求所有權與完成檢查。真正的原子檢查／等待／喚醒，仍需處理上層緩衝區、swap 路徑、kernel 鎖與排程器安全中斷時機；不能宣稱已消除忙等。

## 程式碼與規格

- [kernel/fdt.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/fdt.c)、[kernel/irq.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/irq.c)：FDT 及 PLIC 派發。
- [kernel/trap.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/trap.c)、[kernel/block.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/block.c)：外部陷阱及裝置 ACK。
- [RISC-V PLIC claim](https://docs.riscv.org/reference/plic/v1.0.0/plic-claims.html)、[PLIC completion](https://docs.riscv.org/reference/plic/v1.0.0/plic-completion.html)。
- [QEMU virt 平台](https://www.qemu.org/docs/master/system/riscv/virt.html)、[VirtIO 1.3](https://docs.oasis-open.org/virtio/virtio/v1.3/virtio-v1.3.html)。
