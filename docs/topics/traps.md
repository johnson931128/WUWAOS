<script setup>
import TrapFlow from '../.vitepress/theme/components/TrapFlow.vue'
</script>

# 03 · 例外與系統呼叫

User Mode（使用者模式）程式不能直接控制所有硬體。它以 syscall（系統呼叫）請核心提供服務；CPU 偵測到非法存取或缺頁時，也會進入核心。

## 從使用者程式進入核心，再返回

切換情境，沿固定架構逐步閱讀。系統呼叫、合法缺頁與計時器中斷共用入口，但處理原因、返回位置及暫存器更新不同。

<TrapFlow />

## 三種情境的返回差異

| 情境 | 來源與原因 | 核心處理 | 正常返回位置 |
| --- | --- | --- | --- |
| 使用者系統呼叫 | U-mode；`scause = 8` | `syscall_dispatch` 將結果寫入 `a0` | `sepc + 4`；exit 不返回 |
| 合法缺頁 | U-mode；`scause = 12 / 13 / 15` | `vm_handle_fault` 檢查權限並補頁 | 補頁成功後重試原 `sepc` |
| 計時器中斷 | U-mode；中斷位為 1、原因碼 5 | `timer_interrupt`、`thread_preempt` 與 `thread_resume` | 保存的 `sepc`；保留 `a0` |

上述返回皆以行程仍允許繼續執行為前提。非法缺頁、補頁失敗、exit 或返回邊界處理 pending kill，可能使行程結束；不能把它們畫成成功返回使用者。

## CPU 如何進入入口

| 名稱 | 用途 |
| --- | --- |
| stvec（監督模式陷阱向量暫存器） | 指定陷阱入口與入口模式 |
| scause（監督模式陷阱原因暫存器） | 記錄發生哪種事件 |
| sepc（監督模式例外指令位置暫存器） | 保存返回／重試時的指令位置 |
| stval（監督模式陷阱額外資訊暫存器） | 缺頁時通常提供相關虛擬位址 |
| sscratch（監督模式暫存用途暫存器） | WUWAOS 用於切換使用者與核心堆疊 |

本核心的 stvec 採 Direct mode（共同入口模式），所有委派給監督模式的陷阱先到同一個 `trap_entry`，再由軟體依原因分派。不是每種例外都有一個硬體向量表項。

## trap.S 與 trap.c 的關係

`arch/riscv/trap.S` 先切換堆疊，將一般暫存器和 CSR（控制與狀態暫存器）存入 288-byte TrapFrame（暫存器保存結構），再呼叫 C 的 `trap_handler`。

原始碼：[arch/riscv/trap.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/trap.c)。

CPU 不會自動幫核心保存全部一般暫存器。若不先保存，C 處理函式就可能破壞使用者原有資料。C 最後回傳要恢復的 TrapFrame，組合語言還原後用 `sret` 返回使用者模式。

陷阱處理期間若要切換行程，另由 swtch.S 保存核心呼叫上下文，經獨立排程器堆疊切換；之後恢復同一行程的 C 呼叫，再回到它自己的 trap.S 返回路徑。TrapFrame 與 KernelContext 的分工見[行程管理](./processes.md#核心函式如何暫停與恢復)。

## 一次系統呼叫

```text
shell 呼叫 API → a7 放呼叫編號、a0… 放參數
  → ecall → trap_entry → trap_handler → syscall_dispatch
  → 服務函式 → a0 放結果 → sret
```

`user/lib/api.c` 的 `syscall3` 負責暫存器約定，`include/syscall_numbers.h` 定義編號，`kernel/syscall/syscall.c` 分派。Shell 的 `cat`、`fan` 是使用者層命令名稱，不等於各自都有同名 syscall。

ecall（環境呼叫指令）長度固定 4 bytes；核心將 sepc 加 4，讓返回時從呼叫後繼續。缺頁補頁成功則保留原 sepc，重試同一指令，兩者不可混用。

## 如何觀察

在 `wuwaos/>` 執行 `ps`，命令經 API 進核心，讀取真實 PCB，再輸出結果。執行 `mem alloc 1`、`mem read 0`，則可配合 `vmstat` 觀察合法首次存取的[缺頁處理](./paging.md)。

計時器中斷也走同一入口：scause 最高位表示中斷、低位原因 5 表示監督模式計時器。先清除 pending，再讓原行程排到隊尾，依 [RR 排程](./scheduling.md) 選擇下一個；保留 sepc 與 a0，不當作 syscall 返回值改寫。

目前原始碼也識別原因碼 9 的 supervisor external interrupt，交由 PLIC claim／驅動 callback／complete 流程處理；保留 `sepc`。U-mode 中斷返回經既有 thread_resume，kernel 來源則直接還原 frame，不假設一定有目前行程。`ps → ls / → ps` 可人工比較 WUFS IRQ 計數；設計與驗證方式見 [Spec 32](../SPEC-32-plic-external-interrupts.md)，合併後的編譯與 QEMU 檢查範圍見[整合紀錄](../merge-2026-11-10.md)。

核心自身出錯目前會印出陷阱資訊並停止；使用者非法存取則結束該行程。目前已有計時器驅動的 50 ms RR，UART 仍輪詢，也沒有完整訊號機制或核心內搶占。新的外部 IRQ 骨架不改變這些前提。

觀念參考：`notes/ch2 Operating-System Structures.md` 的系統呼叫與雙模式。早期基礎驗收見 [Spec 01](../SPEC-01-user-syscalls.md)，目前缺頁路徑見 [Spec 10](../SPEC-10-demand-paging.md)。

## 規格版本與驗收邊界

本章依目前 `arch/riscv/trap.S`、`arch/riscv/trap.c`、`kernel/syscall/syscall.c` 與 `kernel/proc/process.c` 整理，不將歷史規格的限制當成現行架構。

- [Spec 01](../SPEC-01-user-syscalls.md) 記錄最初單行程及三個基本 syscall；本章沿用 Direct mode、TrapFrame 與 ecall 返回規則，不表示目前只有那些服務。
- [Spec 10](../SPEC-10-demand-paging.md) 記錄按需分頁與當時的同步 I/O 驗收。後續核心等待與取消邊界已演進，需合併閱讀[換頁與 Swap](./paging.md)及[Block Async I/O](./block-async.md)。
- [Thread 階段 A](../thread-stage-a.md) 記錄 PCB／TCB 分工；目前 trap frame 與排程狀態由 Thread 管理，Process 持有位址空間等資源。
- [外部中斷規格](../SPEC-32-plic-external-interrupts.md)與[整合紀錄](../merge-2026-11-10.md)保留各自實際驗證範圍；本頁圖解不擴大那些驗收結論。

此元件是固定教學步驟，不讀取 QEMU、PCB 或硬體暫存器。本次頁面改版不重跑 OS／CLI 操作驗收，相關 runtime 檢查標為 **NOT_RUN**。
