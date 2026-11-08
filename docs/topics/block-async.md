<script setup>
import BlockAsyncLab from '../.vitepress/theme/components/BlockAsyncLab.vue'
</script>

# Block Async I/O

`block_read()` 和 `block_write()` 看起來是一般函式，但底層裝置工作可能跨越多次排程才完成。本章把兩件事分開看：**裝置 request 可以非同步進行；目前公開的 Block 呼叫仍會等到完成再返回。**互動元件中的每個 frame 都是依照程式碼整理的教學示意，只呈現事件順序，不是 QEMU trace，也不代表真實時間比例。

<BlockAsyncLab />

## 問題：如何安全地等待一個慢裝置？

最簡單的程式可以提交請求後一直讀裝置狀態，直到完成；這會讓 CPU 花時間忙等。另一種做法是提交工作後讓目前行程等待，CPU 去執行其他 READY 行程；scheduler 輪詢完成狀態，完成後通知等待者。WUWAOS 的 Block request 採用後者的排程方式，但裝置完成目前由 scheduler polling 觀察，並非 VirtIO block 硬體中斷直接喚醒 caller。

使用者課程筆記[《ch12 I_O Systems》§12.2.4](https://github.com/johnson931128/WUWAOS/blob/main/notes/ch12%20%20I_O%20Systems.md#L1072) 說明：「大量資料搬移時，不要讓 CPU 一個 byte 一個 byte 親自搬」；DMA 讓裝置直接在 I/O device 與 main memory 間傳資料。筆記也指出：「DMA 不是完全不用 CPU。」OS 仍要設定工作與處理完成。這是課程中的 DMA 概念。WUWAOS 的實作以 descriptor 指向 BlockDevice 內的靜態 request 欄位與 bounce buffer；這章只說明這條 block request 路徑，不把課本中的通用 DMA controller 流程誤當作 WUWAOS 的中斷流程。

## 一筆 request 的實際路徑

1. caller 進入 `block_read()`／`block_write()`；runtime 啟動後，若當前 context 不允許等待，提交會回 `ERR_BUSY`。
2. `submit_request()` 取得該裝置的 `KernelGate`，進入 cancel-defer，準備持久 request header、descriptor、status、bounce buffer 與 completion event snapshot。
3. WRITE 先將 512 bytes 從 caller buffer 複製到 bounce。READ 的 descriptor 則讓裝置把資料 DMA 到 bounce。driver 發布 available ring entry，通知 VirtIO。
4. caller 呼叫 `process_wait_event()`。其 PCB 保存 `KernelEventWait`，行程變成 `PROCESS_WAITING`，`wait_reason` 為 `WAIT_EVENT`。scheduler 可排程其他 READY 行程。
5. 裝置完成時先更新 used ring。這本身不會呼叫 WUWAOS event API，也不會直接把 caller 喚醒。scheduler 稍後在 poll context 執行 `block_poll()`，檢查 used entry、status 與長度，再將 request 設為 `DONE` 並通知 completion event。
6. event 通知將 caller 轉成 READY。caller 再次被排程後回到自己的 kernel stack，收取結果；成功 READ 才將 bounce 複製至 caller buffer。最後 request 回到 `IDLE`，釋放 gate 和 cancel-defer，函式才返回。

scheduler 的 poll context 不可等待 gate 或呼叫可能阻塞的 request。當沒有 READY 行程但還有 event/UI/Block poll 工作時，scheduler 使用有限 timer deadline 與 WFI，醒來後再輪詢；這仍是 polling 路徑。

## 教科書概念與目前實作

| 主題 | 課程概念 | WUWAOS 目前實作 |
| --- | --- | --- |
| 非同步 | I/O 與 CPU 工作可重疊；等待者讓出 CPU | 裝置 request 提交後可與其他行程重疊，但 `block_read/write/flush` 等待到收尾才返回 |
| 完成通知 | Interrupt-driven I/O 可由裝置中斷通知 CPU | Block 完成透過 scheduler 的 `block_poll()` 觀察 used ring，再送 `KernelEventSource`；不是 block IRQ |
| DMA/buffering | 裝置可直接和記憶體搬資料；OS 設定與管理 buffer | descriptor 使用 `BlockDevice` 的靜態 header/status/bounce；READ/WRITE 都以 512-byte bounce 承接資料 |
| 啟動期間 | OS 可用 polling 完成早期工作 | `block_init_from_fdt()` 掛載檢查前、process runtime 尚未啟動時，`submit_request()` 在呼叫路徑內直接 `poll_device()`；不會等待 process event |
| syscall | 非同步 API 可以把完成交還給呼叫者 | 這條 block API 不會在提交後立即返回 request ID；caller 睡眠、醒來、collect 後才返回 syscall/kernel call |

參考實作：[block driver](https://github.com/johnson931128/WUWAOS/blob/main/drivers/virtio/block.c)、[process/event wait](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c)、[event source](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/event.c)、[gate](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/gate.c)、[scheduler poll hook](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c)。連結指向 `main` 分支原始碼。

## 重要欄位

| 欄位 | 所在結構 | 用途與生命週期 |
| --- | --- | --- |
| `header` | `BlockDevice` | VirtIO block request 的 type、reserved、sector；位於長期裝置結構中 |
| `descriptor[]` | `BlockDevice` | 描述 header、可選 data buffer、status 的地址、長度與方向旗標 |
| `avail_ring` / `used_ring` | `BlockDevice` | driver 發布 request；裝置回報完成；只有 poller 收取 used entry |
| `status` | `BlockDevice` | 裝置回報狀態；driver 驗證後轉成 0 或 `ERR_IO` |
| `bounce[512]` | `BlockDevice` | 容量 512 bytes、16-byte 對齊的持久資料緩衝；在 collect 或確認 reset 前不能重用 |
| `request_state` | `BlockDevice` | `IDLE`、`IN_FLIGHT`、`RESETTING`、`DONE` 四種狀態 |
| `operation` / `request_sector` | `BlockDevice` | 記錄 READ/WRITE/FLUSH 與目標 sector，供完成處理與診斷使用 |
| `deadline` / `reset_deadline` | `BlockDevice` | 單筆 I/O deadline 與 reset 確認期限 |
| `gate.owner` / `owner_pid` / `depth` | `KernelGate` | 限制同一裝置 request 的單一 owner；owner 取得後持有到 collect 完成 |
| `completion` | `KernelEventSource` | 單調遞增 sequence；poller 通知完成，喚醒符合 source/generation 的等待者 |
| `event_wait` | caller 的 `Process` PCB | 記錄 source、generation、sequence、result 等；`process.state=PROCESS_WAITING` 與 `wait_reason=WAIT_EVENT` 是不同欄位 |
| `cancel_defer_depth` | caller 的 `Process` PCB | 已提交 request 後避免 kill 取消等待並讓裝置仍使用中的 buffer 被提前回收；不會禁止 scheduler 切換 CPU |

Gate 排隊和提交後等待的取消邊界不同：在 gate 還沒取得時，`kernel_gate_enter()` 的等待可被 kill 中斷；成功取得 gate、進入 cancel-defer 並提交後，kill 只設 `kill_pending`，不取消 event wait。caller 必須等 request 到 `DONE` 並安全 unwind，`process_kernel_call_return()` 才在返回邊界執行 `process_exit(-9)`。

## 函式介面與結果

以下簽名以目前 header 為準。sector 是 512-byte sector index；`buffer` 至少可讀／寫 512 bytes，且在呼叫期間保持有效。`block_flush()` 不使用資料 buffer。

| 函式 | 參數 | 成功回傳 | 代表性錯誤 |
| --- | --- | --- | --- |
| `int block_read(uint32_t sector, void *buffer)` | filesystem 裝置上的 sector 與目的 buffer | `0` 並複製 bounce 到 buffer | `ERR_IO`、`ERR_BUSY`、`ERR_TIMED_OUT` |
| `int block_write(uint32_t sector, const void *buffer)` | filesystem 裝置上的 sector 與來源 buffer | `0` | `ERR_IO`、`ERR_BUSY`、`ERR_TIMED_OUT`；錯誤可能無法確認資料是否已寫入 |
| `int block_flush(void)` | 無；flush filesystem 裝置快取 | `0` | 不支援時 `ERR_IO`；timeout/error 時寫入持久性可能不確定 |
| `int block_swap_read(uint32_t sector, void *buffer)` | swap 裝置 sector 與目的 buffer | `0` 並複製 bounce | `ERR_IO`、`ERR_BUSY`、`ERR_TIMED_OUT` |
| `int block_swap_write(uint32_t sector, const void *buffer)` | swap 裝置 sector 與來源 buffer | `0` | `ERR_IO`、`ERR_BUSY`、`ERR_TIMED_OUT` |
| `void block_poll(void)` | 無；poll 已初始化的裝置 | 無回傳值 | 不等待；由 scheduler/poll context 呼叫 |
| `long process_wait_event(KernelEventSource *source, uint32_t generation, uint64_t sequence, uint64_t deadline, long timeout_result)` | event 版本與序號、可選 deadline、timeout 結果 | 通知結果；sequence 已變可立即回 0 | `ERR_BUSY`、`ERR_IO`、傳入的 timeout result |
| `void event_source_notify(KernelEventSource *source, long result)` | 要通知的 source 與結果 | 無回傳值；sequence 增加並通知等待者 | source 無效或關閉時不通知 |
| `int kernel_gate_enter(KernelGate *gate)` / `int kernel_gate_leave(KernelGate *gate)` | 裝置 gate | `0` | `ERR_BUSY`、`ERR_INTERRUPTED`、`ERR_INVALID`、`ERR_IO` |

錯誤常數在 [`include/syscall_numbers.h`](https://github.com/johnson931128/WUWAOS/blob/main/include/syscall_numbers.h)：`ERR_INTERRUPTED=-4`、`ERR_IO=-5`、`ERR_BUSY=-16`、`ERR_TIMED_OUT=-110`、`ERR_UNCERTAIN=-200`。`ERR_UNCERTAIN` 是核心提供的錯誤碼；Block timeout/reset 路徑會以診斷 uncertain 欄位標記 WRITE/FLUSH 結果不確定，而提交函式仍保留原錯誤結果（例如 `ERR_TIMED_OUT`）。

核心函式簽名可見：[block.h](https://github.com/johnson931128/WUWAOS/blob/main/drivers/virtio/block.h)、[process.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.h)、[event.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/event.h)、[gate.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/gate.h)。

## 錯誤、timeout 與資源安全

- **有效 IOERR**：used entry 的 id、長度等欄位有效，但 device status 非零。driver 將 request 設為 `DONE`，event 傳回 `ERR_IO`，caller collect 後回 `IDLE`。這條路徑不需要 reset。
- **Malformed completion**：used index delta、entry id、status 或長度驗證失敗時，driver 呼叫 `begin_reset(..., ERR_IO)`。這與有效 IOERR 不同，因 queue ownership 或 completion 結構本身不可信。
- **Request timeout**：deadline 到時仍無 used entry，開始 reset 並設 `RESETTING`。在裝置確認 reset 前，queue 與 buffer 仍可能被裝置持有。
- **Reset 確認成功**：`poll_reset()` 觀察裝置 status 為 0，先標記裝置 offline，將保存的原結果放入 `request_result`，再通知 completion。caller 恢復後 collect；WRITE/FLUSH 診斷標記 uncertain，因 timeout 不能證明裝置沒執行寫入。
- **Reset 未確認**：超過 reset deadline 仍非 0 時，`reset_fail_stop()` 診斷 `reset-unconfirmed`，關閉 SIE/`sie` 並進入 `kernel_idle()`。不通知 caller、不釋放 gate、不把 buffer 或 queue memory 清空重用，因裝置可能仍存取它們。
- **Kill 延後**：已提交 I/O 後，cancel-defer 讓 event wait 不被 kill 提前取消。待 DONE 收取、request memory 不再由裝置使用、gate/defer 正常 unwind 後，kernel call 返回邊界才終止行程。cancel-defer 保護取消邊界，不是關閉排程。

## 原始碼對照與簡化虛擬碼

重要實作函式：

| 函式 | 職責 |
| --- | --- |
| `submit_request()` | gate/defer、準備 bounce 與 descriptor、發布 queue、等待 event、collect 並清理 |
| `poll_device()` / `block_poll()` | 收取 used ring、檢查完成或 timeout；`block_poll()` 逐一輪詢裝置 |
| `begin_reset()` / `poll_reset()` | 要求 reset、等待確認；確認後 offline 並通知 |
| `reset_fail_stop()` | reset ownership 不確定時保留資源並停機 |
| `process_wait_event()` | 在 caller PCB 登記等待後讓出；恢復時取回 wait result |
| `process_event_notify()` | 將匹配的等待行程轉為 READY |
| `kernel_gate_enter()` / `kernel_gate_leave()` | 取得／釋放有 owner 的裝置 gate |
| `process_kernel_call_return()` | 在安全返回邊界處理延後的 kill |

```text
submit_request(device, sector, buffer, op):
    gate_enter(device.gate)
    cancel_defer_enter()
    if op == WRITE: copy(buffer, device.bounce)
    prepare(device.header, device.descriptor, device.status)
    (generation, sequence) = snapshot(device.completion)
    device.request_state = IN_FLIGHT
    publish(device.avail_ring)
    notify_virtio()

    while device.request_state != DONE:
        if runtime_not_started:
            poll_device(device)          // boot/setup 階段同步輪詢
        else:
            process_wait_event(completion, generation, sequence)
            refresh_snapshot()

    result = device.request_result
    if result == 0 and op == READ: copy(device.bounce, buffer)
    device.request_state = IDLE
    gate_leave(device.gate)
    cancel_defer_leave()
    return result
```

實際 source：[submit_request](https://github.com/johnson931128/WUWAOS/blob/main/drivers/virtio/block.c#L414)、[poll_device / block_poll](https://github.com/johnson931128/WUWAOS/blob/main/drivers/virtio/block.c#L360)、[reset_fail_stop](https://github.com/johnson931128/WUWAOS/blob/main/drivers/virtio/block.c#L326)、[kill 與事件等待](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c#L600)、[kernel-call return 邊界](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c#L661)、[event wait](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c#L684)、[gate owner](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/gate.c)、[event sequence](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/event.c)。GitHub 行號連結以目前 `main` 為準。

## 建議操作步驟

1. 選 **Read 成功**，逐步觀察 RUNNING → PROCESS_WAITING/WAIT_EVENT → READY → caller collect；確認 READ 先到 bounce，最後才 copy 到目的 buffer。
2. 在 **Write 成功** 中查看提交前的 copy，並注意裝置完成、used ring 被寫入、poller 發 event 是不同步驟。
3. 切到 **裝置回報 IOERR**，確認有效錯誤 completion 走 DONE/collect，不走 reset。
4. 比較兩個 timeout 情境：reset 確認後回傳錯誤並 offline；reset 未確認則 fail-stop，queue/buffer 不釋放。
5. 查看 **等待中 kill defer**：cancel-defer 沒有阻止 scheduler 切換；它只保證已提交 request 安全完成並 unwind 後，才在 kernel-call return 邊界處理 kill。

互動程式預設停在第一 frame；播放、逐步前後、重設或切換情境時都會清理既有 timer，離開元件時也會停止 timer。Reduced-motion 偏好會停用 CSS 動畫／轉場；互動敘述與狀態不依賴顏色判讀。
