# 03 · 例外與系統呼叫

User Mode（使用者模式）程式不能直接控制所有硬體。它以 syscall（系統呼叫）請核心提供服務；CPU 偵測到非法存取或缺頁時，也會進入核心。

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

`kernel/trap.S` 先切換堆疊，將一般暫存器和 CSR（控制與狀態暫存器）存入 288-byte TrapFrame（暫存器保存結構），再呼叫 C 的 `trap_handler`。

原始碼：[kernel/trap.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/trap.c)。

CPU 不會自動幫核心保存全部一般暫存器。若不先保存，C 處理函式就可能破壞使用者原有資料。C 最後回傳要恢復的 TrapFrame，組合語言還原後用 `sret` 返回使用者模式。

## 一次系統呼叫

```text
shell 呼叫 API → a7 放呼叫編號、a0… 放參數
  → ecall → trap_entry → trap_handler → syscall_dispatch
  → 服務函式 → a0 放結果 → sret
```

`user/api.c` 的 `syscall3` 負責暫存器約定，`include/syscall_numbers.h` 定義編號，`kernel/syscall.c` 分派。Shell 的 `cat`、`fan` 是使用者層命令名稱，不等於各自都有同名 syscall。

ecall（環境呼叫指令）長度固定 4 bytes；核心將 sepc 加 4，讓返回時從呼叫後繼續。缺頁補頁成功則保留原 sepc，重試同一指令，兩者不可混用。

## 如何觀察

在 `wuwaos>` 執行 `ps`，命令經 API 進核心，讀取真實 PCB，再輸出結果。執行 `mem alloc 1`、`mem read 0`，則可配合 `vmstat` 觀察合法首次存取的[缺頁處理](./paging.md)。

計時器中斷也走同一入口：scause 最高位表示中斷、低位原因 5 表示監督模式計時器。先清除 pending，再讓原行程排到隊尾，依 [RR 排程](./scheduling.md) 選擇下一個；保留 sepc 與 a0，不當作 syscall 返回值改寫。

核心自身出錯目前會印出陷阱資訊並停止；使用者非法存取則結束該行程。目前已有計時器驅動的 50 ms RR，但 UART 仍輪詢，也沒有完整訊號機制或核心內搶占。

觀念參考：`notes/ch2 Operating-System Structures.md` 的系統呼叫與雙模式。早期基礎驗收見 [Spec 01](../SPEC-01-user-syscalls.md)，目前缺頁路徑見 [Spec 10](../SPEC-10-demand-paging.md)。
