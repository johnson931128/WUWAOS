# 02 · 從使用者程式進入核心

<script setup>
import TrapFlow from '../.vitepress/theme/components/TrapFlow.vue'
</script>

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

在雙重模式保護下，使用者程式處於無特權的 [U-mode](./glossary.md#privilege-mode)，既不能直接碰硬碟，也不能修改頁表。那麼當程式需要讀檔或印字時，它要如何「安全地」把控制權交給核心？而當發生缺頁或計時器響起時，CPU 又要如何把現場保存下來、處理完再毫髮無傷地回到原本那條指令？
</div>

## 恐龍書核心理論：雙重模式、API 與系統呼叫

恐龍書第 2 章把使用者程式存取核心服務的過程拆成兩層：

- **API（應用程式介面）**：高階語言或使用者函式庫提供的介面（例如 C 的 `read()`、`write()`）。開發者平常呼叫的是 API，而不是手寫組合語言。
- **System Call（系統呼叫）**：真正跨越特權邊界的硬體機制。使用者函式庫會把呼叫編號與參數放進約定好的暫存器，再執行一條陷阱指令（在 RISC-V 是 `ecall`），觸發硬體[陷阱](./glossary.md#trap)切入 S-mode。

<div class="tb-box theory">
<span class="tb-label">課本實務提醒：系統呼叫的成本</span>

每一次系統呼叫都要經歷「保存暫存器 → 切換至核心態 → 檢查使用者指標與權限 → 執行服務 → 還原暫存器並 `sret`」。因此課本以「複製檔案（`cp`）」為例強調：若每次 `read()` 只讀 1 byte，1 MiB 檔案就會跨界一百萬次；實務上會使用適當大小的緩衝區（Buffer）來攤提跨界成本。
</div>

## 系統呼叫如何往返（WUWAOS 實例）

在 WUWAOS 中，使用者程式呼叫 `read()` 或 `write()` 時，會依以下五步完成一次完整往返：

1. **準備參數（U-mode）**：`user/lib/api.c` 依 WUWAOS 自訂 ABI，將前三個參數放入 `a0`–`a2`，呼叫編號放入 `a7`，接著執行 `ecall`。
2. **切換堆疊並保存現場（`arch/riscv/trap.S`）**：CPU 跳到 `stvec` 指向的 `trap_entry`。組合語言用 `sscratch` 換成該執行緒的核心堆疊，將所有通用暫存器與關鍵 CSR 存成 288-byte 的 [TrapFrame](./glossary.md#trap-frame)。
3. **辨識原因並分派（`arch/riscv/trap.c` → `kernel/syscall/syscall.c`）**：`trap_handler()` 讀取 `scause`。若是使用者 `ecall`（`scause = 8`），先將 `sepc += 4` 越過 `ecall` 指令，再交給 `syscall_dispatch()`，回傳值寫回 `frame->a0`。
4. **可阻塞的核心執行**：若需要等待磁碟或 Pipe，呼叫者可停在核心函式內，由[排程器](./04-scheduling.md)切換其他執行緒；事件完成後回到原核心堆疊繼續。
5. **還原並返回（`trap_restore`）**：核心把 `sepc`、`sstatus` 與暫存器還原，最後執行 `sret` 回到 U-mode。

### 程式碼實例 1：原子切換堆疊（`arch/riscv/trap.S`）

當程式從 U-mode 觸發陷阱時，不能直接信任使用者堆疊 `sp`（它可能已溢位或指向非法位址）。WUWAOS 利用 RISC-V 的 `sscratch` 暫存器在單一條指令內完成 `sp` 與核心堆疊頂端的交換：

```asm
trap_entry:
    # U-mode 時 sscratch 存放核心堆疊頂端；S-mode 內再進 trap 時為 0
    csrrw sp, sscratch, sp
    bnez sp, 1f
    csrrw sp, sscratch, sp
1:
    addi sp, sp, -TF_SIZE       # 在核心堆疊上開出 TrapFrame 空間
    sd ra, 8(sp)
    ...                         # 保存通用暫存器與 sepc / sstatus / scause / stval
    mv a0, sp
    call trap_handler           # 進入 C 語言分派
```

### 程式碼實例 2：系統呼叫分派（`kernel/syscall/syscall.c`）

進入 C 語言後，`syscall_dispatch()` 直接從 TrapFrame 取出 `a7`（`registers[17]`）作為系統呼叫編號，並將 `a0`–`a2`（`registers[10..12]`）轉交給對應的核心子系統：

```c
void syscall_dispatch(TrapFrame *frame) {
    const uint64_t number = frame->registers[17]; // a7: syscall number
    long result;
    switch (number) {
    case SYS_WRITE:
        result = io_write((long)frame->registers[10], frame->registers[11],
                          frame->registers[12]);
        break;
    case SYS_READ:
        result = io_read((long)frame->registers[10], frame->registers[11],
                         frame->registers[12]);
        break;
    case SYS_EXIT:
        process_exit((long)frame->registers[10]);
        return;
    ...
    }
    frame->registers[10] = (uint64_t)result;      // 結果寫回 a0
}
```

## 進入核心後從哪裡繼續？三種陷阱的 `sepc` 規則

不是所有進核心的事件都要把 `sepc` 加 4！這是 OS 設計最容易搞混的細節之一：

| 陷阱事件 | 關鍵暫存器 | 核心處理 | 返回位置（`sepc`） |
| --- | --- | --- | --- |
| 使用者 `ecall` | `scause = 8` | 呼叫 `syscall_dispatch()` | 一般 **`sepc + 4`** 越過 `ecall`；成功的 `exec()` 改跳新程式入口，`exit()` 直接終止。 |
| 取指／讀寫缺頁 | `scause = 12 / 13 / 15`、`stval` | `vm_handle_fault()` 驗證權限並補頁或執行 [COW](./08-virtual-memory.md#寫入時複製) | 補頁成功後 **保留原 `sepc`** 重試該指令；非法存取則終止行程。 |
| 計時器或外部中斷 | `scause` 最高位為 1 | 時間片到期可切換執行緒，外部 IRQ 交給裝置驅動 | 原執行緒恢復時 **保留原 `sepc`**，從被打斷的指令無縫接續。 |

## 逐步看 CPU 如何返回

切換「系統呼叫」、「合法缺頁」與「計時器中斷」，觀察三條路徑如何處理 `TrapFrame` 與 `sepc`。

<TrapFlow />

原始碼與延伸：[陷阱入口 (`arch/riscv/trap.S`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/arch/riscv/trap.S) · [Trap Handler (`arch/riscv/trap.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/arch/riscv/trap.c) · [Syscall Dispatcher (`kernel/syscall/syscall.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/syscall/syscall.c) · [陷阱章節補充](../topics/traps.md)
