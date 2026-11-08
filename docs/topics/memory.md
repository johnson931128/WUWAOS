<script setup>
import Sv39Explorer from '../.vitepress/theme/components/Sv39Explorer.vue'
</script>

# 06 · 實體頁與 Sv39

<Sv39Explorer />

## Overview

使用者程式看到的位址，不必等於資料真正所在的 RAM 位址。Page table（分頁表）讓 CPU 轉換位址，也決定能不能讀、寫與執行。

WUWAOS 使用 RISC-V Sv39（39-bit 虛擬位址分頁模式），以 4 KiB 為一頁。對低位 user address，39-bit VA 拆成三個 9-bit VPN index 和一個 12-bit 頁內位移。每張頁表有 512 個 8-byte PTE，正好用一個 4 KiB page 存放。核心目前只建立 4 KiB leaf mapping，不支援大頁。

## Architecture

### 虛擬位址欄位

```text
虛擬位址：VPN[2] 9 bits | VPN[1] 9 bits | VPN[0] 9 bits | 頁內位移 12 bits
           L2 索引       L1 索引       L0 索引          page offset
```

`kernel/mm/vm.c` 的 `vm_walk` 在各層用 `(address >> (12 + 9 * level)) & 511` 取得索引。前兩層有效 non-leaf PTE 指向下一張表；L0 的有效 leaf PTE 含實體頁框編號與權限旗標。轉譯時保留 VA 的低 12-bit offset，將它加到 leaf 指向的 physical frame base。

### 三級表的資料路徑

每張表 4096 bytes，每個 PTE 8 bytes，所以有 512 個條目；9 bits 正好可選 0–511。root 從 `satp` 指定的頁表開始，VPN[2]、VPN[1]、VPN[0] 依序在 L2、L1、L0 選 entry。中間層的 valid PTE 是下一層表格的位址，不是最終資料 frame；只有 leaf PTE 給出該頁的 physical frame。

## Step-by-step

輸入十六進位 VA，或選固定 preset，按步驟檢視欄位拆解、L2 → L1 → L0 路徑、PTE 旗標與 frame/offset 組合。互動器使用 BigInt 計算 index 與 PA，避免 JavaScript Number 對 64-bit 位址精度不足。

三個固定 mapping 對應程式碼頁、使用者堆疊頁及 Heap 教學頁，提供 RXU/RWU 的 PTE 範例。頁表節點、leaf PTE 與 frame base 都是人工指定的教學資料，用來展示算式；它們不是 WUWAOS 啟動後的真實 RAM 或某個 process 的 page table。示例只查這三頁，其他範圍回報超限、非 canonical 或未映射，不虛構任意 kernel mapping。

對合法低位 user VA，bit-field 公式為：

```text
VPN[0] = (VA >> 12) & 0x1ff
VPN[1] = (VA >> 21) & 0x1ff
VPN[2] = (VA >> 30) & 0x1ff
offset = VA & 0xfff
PA     = PTE.PPN × 4096 + offset
```

例如 preset `0x00401234` 有 `VPN[2]=0`、`VPN[1]=0`、`VPN[0]=4`、`offset=0x234`。教學 leaf mapping 指向假設 frame base `0x81000000`，所以示意 PA 是 `0x81000234`。這個數字只是 preset 運算結果，不代表該 RAM 位址被配置或可供真實程式讀取。

## Implementation

### 配置實體頁

`kernel/mm/page.c` 將核心結尾至 FDT 之前的整頁串成 Free list（空閒串列），排除韌體、核心、固定堆疊與頂端保留區。

配置時取串列頭，標記 allocated bitmap（已配置位元圖），清零 4096 bytes 後回傳位址。回收檢查範圍、對齊與重複釋放，再放回串列頭。串列操作 O(1)，清零成本與頁大小成正比。

它回答「哪個空閒實體頁可以給我」，不決定頁框不足時要淘汰誰；後者屬於 VM 的置換政策。

### 使用者位址配置

| 區域 | 虛擬範圍 | 權限 |
| --- | --- | --- |
| 程式碼與唯讀資料 | 0x400000–0x404000 | RXU（可讀、可執行、使用者可存取） |
| 可寫資料 | 0x404000–0x405000 | RWU（可讀寫、使用者可存取） |
| 使用者堆疊 | 0x800000–0x801000 | RWU |
| Heap（動態資料區） | 0x1000000 起，最多 1 MiB | `sbrk` 保留後可按需讀寫 |

每個行程有私有頁表與使用者頁。核心與裝置映射可共用，但沒有 U 權限，使用者不能直接存取。RISC-V 這裡採分頁，沒有加入 IA-32 圖中的 segmentation（分段）單元。

### 權限與核心複製

CPU 依 PTE 檢查指令／資料存取。核心替使用者搬資料時，也要先檢查合法範圍，再由 `user_copy_from`／`user_copy_to` 逐頁載入與複製，不能信任使用者傳來的任意位址。

映射改變後以 `sfence.vma`（分頁轉譯同步指令）使舊轉譯失效；不能在 CPU 仍可能使用舊轉譯時把頁框交給別人。

### 在 QEMU 觀察

執行 `ps` 看底層可配置頁數，再執行 `vmstat` 看使用者常駐頁。QEMU RAM 仍是 128 MiB，但核心、頁表與保留區會占用一部分；兩份統計的範圍不同。

目前建立行程不預先填滿所有使用者頁，下一章說明[按需分頁](./paging.md)。早期動態配置驗收在 [Spec 08](../SPEC-08-memory.md)，載入時機以 [Spec 10](../SPEC-10-demand-paging.md) 為準。

## Source mapping

| 程式位置 | 實際責任 |
| --- | --- |
| [vm.c · `vm_walk`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c) | 拒絕 `address >= USER_LIMIT`；L2/L1 以 `12 + 9 * level` 取 index，L0 取最後 PTE；遇到中途 leaf 回傳無效 |
| [vm.c · `vm_map` / `vm_create`](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c) | 組 PPN 與 V/R/W/X/U/A/D flags、建立 Sv39 root、共用核心表分支並保留 user 區 |
| [vm.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.h) | user entry/data/stack 主要位址與 VM 對外操作介面 |
| [vm_limits.h](https://github.com/johnson931128/WUWAOS/blob/main/include/vm_limits.h) | `USER_HEAP_BASE=0x1000000`、最大 256 heap pages；此配置不等於每頁都已實體配置 |
| [page.c / page.h](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/page.c) | 實體 page Free list、bitmap、配置清零與回收 |
| [process.c](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c) 與 [context.h](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/context.h) | 行程持有私有 root、派發時切換頁表；context switch state 與頁表內容是不同責任 |

PTE 旗標在 source 中為 `V=bit 0`、`R=bit 1`、`W=bit 2`、`X=bit 3`、`U=bit 4`、`A=bit 6`、`D=bit 7`。table PTE 只含下一層頁表 PPN 與 V；`vm_map` 的 leaf 同時設定權限與 V/A/D。互動器展示的固定 leaf flag 僅供計算教學，不能推論目前 runtime 中某個 PTE 的狀態。

## Failures / Limitations

### 位址範圍與未映射情境

Sv39 只允許符合 39-bit canonical 規則的位址；WUWAOS 的 `USER_LIMIT` 再將 user walker 限於低位且小於 `2^38` 的範圍。示意工具遇到不合法/超範圍值或未列在固定 mapping 的頁面時顯示錯誤，不會替它捏造 leaf 或 PA。

大頁 leaf 在 L2/L1 層可由架構支援，但 WUWAOS `vm_walk` 遇到中途 leaf 會拒絕；本站示例只呈現 L0 4 KiB leaf。此工具沒有讀取 QEMU RAM，不模擬真實 page-table allocation、TLB 命中／miss、權限 fault、swap 狀態或任意 kernel mapping。

### Stack guard

Process 與 scheduler 的核心 stack 下方各有一頁 4 KiB 未映射 guard，藉由真正的頁面存取故障阻止向下越界。這些頁面位於常駐 kernel storage，不加入 free list；初始化也不清除已取消映射的 guard。尚無 emergency trap stack，若越界時連 TrapFrame 都無法保存，仍可能遞迴 fault；本輪未執行 overflow 測試。

## Further Reading

- 本地課程筆記：[notes/ch9 Main Memory.md](../../notes/ch9%20Main%20Memory.md) §9.3「分頁機制與基本方法」及 §9.4「階層式分頁」：page/offset、頁框與多層 page table 是教科書概念。筆記中 32-bit/雙層位址數值不能直接套用 RISC-V Sv39；此處 9/9/9/12、2^38 limit 與 4 KiB leaf 由 WUWAOS source 決定。
- [RISC-V Privileged Architecture: Sv39](https://github.com/riscv/riscv-isa-manual/blob/main/src/supervisor.adoc)：`satp`、canonical virtual address、PTE 與 page fault 規則。
- [MIT xv6 RISC-V Book · Page tables](https://mit-pdos.github.io/xv6-riscv-book/mem.html)：以逐層 index 解釋硬體 page-table walk；xv6 的虛擬位址區配置不等同 WUWAOS。
- [OSTEP · Advanced Page Tables](https://research.cs.wisc.edu/wind/OSTEP/vm-smalltables.pdf)：多層頁表與空間成本。
- 相關的空間回收政策見[按需分頁與 Swap](./paging.md)；分頁表轉譯與頁面置換是不同步驟。
