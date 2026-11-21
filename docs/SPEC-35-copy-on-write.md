# Spec 35 · Fork 的寫入時複製

## 位址空間與所有權

每個行程有獨立的使用者頁表。`vm_fork()` 先準備子頁表、heap 與 stack 位置，並透過一個 4 KiB 核心暫存頁複製父行程已換出的頁面。每個交換槽位由一個 `VmPage` 持有。從未配置的頁繼續按需載入，父行程的常駐映射在準備期間受置換保護。

所有可失敗的步驟完成且呼叫者未取消後，`vm_clone_impl()` 在相同虛擬頁索引安裝子 PTE。已常駐的程式碼也可共享。對原本可寫的頁，父子 PTE 都清除 `W` 並設定 `PTE_COW`（Sv39 `RSW[0]`）。原始權限以 `permissions()` 查詢，唯讀頁保持原權限。發布 Ready 之前完成父頁表的 TLB 同步。準備失敗會銷毀子頁表及已完成的 Swap 副本，父映射仍然有效。

`frame_refs` 以實體頁框索引，計算有效使用者映射的數量。`retain_frame()` 在新頁框取得第一份引用時增加全域 `resident`。`release_frame()` 只在最後一份引用消失時歸還頁框。`VmPage` 仍各自記錄 `physical`、`slot`、`dirty`、`pins` 與 `arrival`。同一共享頁框的映射具有相同 `arrival`。`pcb` 顯示的是個別行程的映射數，`vmstat` 的 `resident` 是系統的某一刻共有幾個不同常駐頁框。頁表與裝置緩衝區繼續由底層實體配置器獨立管理。

## 寫入與釋放

- Store Page Fault 使用 `scause=15` 與 `stval`，先驗證存取範圍和原始寫權限。`resident_page()` 遇到 `PTE_COW` 時交給 `resolve_cow()`。Kernel Copyout 也經由此路徑，避免直接寫入共用 RAM。
- 若引用多於一份，先 pin 來源、取得新頁框，再複製 4 KiB。若只剩一份引用，直接恢復寫權限。成功時清除 COW 位元、設定 `W/A/D`、標記 dirty 並同步 TLB，然後才可放掉舊引用。缺頁成功由 trap handler 使用原 `sepc` 重試。配置失敗時，該 COW 來源映射和資料仍有效。
- `sbrk` 縮小、`exec()` 切換舊映像，以及退出時的 `vm_destroy_impl()` 都經 `release_page()` 撤銷 PTE、同步 TLB、歸還引用與槽位。Fork 的單 TCB 限制來自 [Thread 規格](./SPEC-34-threads.md)。

## FIFO、Swap 與 SMP

FIFO 選最早入駐且可置換的實體頁框。若任一映射被 pin、屬於正在準備 fork 的來源，或該位址空間正在其他 Hart 執行，就跳過整個頁框。`collect_eviction_mappings()` 用相同頁索引收集所有引用並檢查 PTE。乾淨映射可使用原本的重建來源，或保留自己的 Swap 副本。子行程若繼承父行程已換入的資料且自己沒有 backing，會標記 dirty，以便換出時保留完整內容。

`evict_frame()` 先預留所需槽位，再撤銷整組 PTE 並完成本地 `sfence.vma` 和 SBI Remote SFENCE。持有 VM Gate 完成所有需要的磁碟寫入，成功才提交各映射的槽位並釋放頁框。寫入失敗會恢復所有 PTE、再同步 TLB，並歸還本次新增槽位。若既有槽位寫了一部分，恢復後的 dirty RAM 仍是有效版本。換入時各映射獨立配置頁框，成功讀完才安裝 PTE。磁碟等待允許其他 Hart 進入核心，觸及撤銷映射的行程會等 VM Gate。位址空間切換仍使用既有的 `satp` 與 TLB 同步。

套用到唯讀頁的寫入依原路徑回 `ERR_ADDRESS`。缺乏可用頁框或 Swap Slot 時可能回 `ERR_NO_MEMORY`，磁碟出錯可能回 `ERR_IO`。目前共享僅由 fork 建立，Swap Slot 不共享，亦無 ASID 最佳化。

## 如何觀察與重現

`vmstat` 的 `shared_frames` 和 `free_frames` 是即時數量。其他欄位自開機累計，`fork_shared` 是成功建立的子共享映射數，對應避免的立即複製頁數。`cow_faults` 與 `cow_copyouts` 分別計數硬體 trap 和核心寫入碰到 COW 的次數，`cow_copies` 與 `cow_reuses` 分別計數實際複製和原地解除 COW。`shared_evictions` 和 `shared_rollbacks` 以共享實體頁框為計數單位。

在 WSL 和既有 Clang／QEMU CLI 工具鏈下執行：

```sh
make test-cow
python3 -B tools/test_thread_stage_a.py
python3 -B tools/test_kernel_cli.py
python3 -B tools/benchmark_cow.py --runs 5
```

原始碼入口：[VM](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c) · [Process](https://github.com/johnson931128/WUWAOS/blob/main/kernel/proc/process.c) · [Trap](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/trap.c) · [Hart](https://github.com/johnson931128/WUWAOS/blob/main/arch/riscv/hart.c)

## 2026-11-22 QEMU 驗證

| 情境 | 結果 |
| --- | --- |
| 常駐頁共享與父子寫入隔離 | `PASS`。讀取共享頁、父寫與子寫、重複寫、跨多個子行程引用、單一引用原地恢復、Lazy Page 及 RX 拒寫。 |
| Kernel Copyout、exec 與退出 | `PASS`。pipe `read()` 寫入子行程的 COW buffer，父資料維持原值。子行程 exec 與退出後，整機空閒頁框數回到同組磁碟乾淨開機後退出的基準。 |
| 低常駐預算與共享頁換出 | `PASS`。8 框預算觸發整組換出，父子各自換入後仍讀到原值，寫入互相隔離。 |
| Swap 寫入與 COW 配置失敗 | `PASS`。唯讀 Swap 觸發整組映射回復。24 框預算用盡且無可置換頁框時，COW Copyout 回 `ERR_IO`，共享頁框與複製計數保持原值，父子仍讀到原資料。 |
| Fork 準備中途失敗 | `PASS`。24 槽位的 Swap 用盡前已成功複製部分子槽位，fork 回 `ERR_NO_MEMORY` 後，父資料仍可讀寫，引用、槽位與空閒頁框數回到呼叫前。 |
| 2／3 Hart | `PASS`。父子在共同時間點開始對同一批虛擬頁寫入不同值，各自讀回正確內容。既有 SMP probe 另有真實平行派送斷言。 |
| 既有回歸 | `PASS`。Thread Stage A、Kernel Mutex 與 Thread Core 主機測試，以及 CLI 的單 Hart 直接／Flash 開機、2／3 Hart 直接／Flash 開機、WUFS redo 重播與管線。 |

整機實體配置池耗盡、已成功寫入部分磁區或部分別名後才遭遇故障、GUI runtime 與長時間 SMP 壓測為 `NOT_RUN`。唯讀 Swap 案例涵蓋 VM 已撤銷映射後收到寫入錯誤的復原路徑。

## 效能比較

基準是重新建置的 `74fe07e`。兩版使用同一份 ELF、QEMU 8.2.2 `virt`、單 Hart、128 MiB、128 框預算與相同的 WUFS／Swap 設定。每組預熱一次、量測五次，共 120 筆。程式在 `fork()` 前後及 `wait()` 後讀取 `rdtime`，表列中位數。Fork 新增頁框只計使用者資料框，頁表配置另由核心管理。立即複製版的新增頁框數等於 fork 前的個別行程常駐頁數。複製量計 eager 複製或實際 COW 拆分，並包含因堆疊而複製的頁。全部樣本 Swap 讀寫皆為 0。

表內皆為「立即複製 → COW」。

| Heap 頁數 | 子行程工作 | Fork（µs） | 總耗時（µs） | Fork 新增使用者頁框 | 複製量（KiB） | Page Fault |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 4 | 只讀 | 2251 → 473 | 3576 → 2616 | 8 → 0 | 32 → 4 | 0 → 1 |
| 4 | 立即 exec | 2188 → 390 | 13830 → 7436 | 8 → 0 | 32 → 4 | 1 → 2 |
| 4 | 寫一頁 | 2648 → 904 | 6116 → 6139 | 8 → 0 | 32 → 8 | 0 → 2 |
| 4 | 寫全部 Heap 頁 | 2595 → 350 | 6087 → 2227 | 8 → 0 | 32 → 20 | 0 → 5 |
| 16 | 只讀 | 1665 → 692 | 3214 → 4281 | 20 → 0 | 80 → 4 | 0 → 1 |
| 16 | 立即 exec | 988 → 575 | 6170 → 6934 | 20 → 0 | 80 → 4 | 1 → 2 |
| 16 | 寫一頁 | 1809 → 334 | 5361 → 1913 | 20 → 0 | 80 → 8 | 0 → 2 |
| 16 | 寫全部 Heap 頁 | 1424 → 518 | 2898 → 2560 | 20 → 0 | 80 → 68 | 0 → 17 |
| 48 | 只讀 | 2042 → 337 | 3590 → 2047 | 52 → 0 | 208 → 4 | 0 → 1 |
| 48 | 立即 exec | 1936 → 408 | 9995 → 7386 | 52 → 0 | 208 → 4 | 1 → 2 |
| 48 | 寫一頁 | 2517 → 326 | 5238 → 2011 | 52 → 0 | 208 → 8 | 0 → 2 |
| 48 | 寫全部 Heap 頁 | 2335 → 316 | 6547 → 3773 | 52 → 0 | 208 → 196 | 0 → 49 |

48 頁全部寫入時，COW 最後仍複製 196 KiB，並多出 49 次寫入缺頁。只讀或立即 exec 各複製 4 KiB，皆由 Heap 以外的寫入造成。12 組的 fork 中位數都下降，總耗時有三組上升。部分時間範圍重疊，例如 16 頁只讀的總耗時，基準是 2789–5287 µs，COW 是 1785–5736 µs。QEMU 的主機排程會影響時間，這些數據適合解讀複製量與缺頁成本，無法推廣為每種工作負載的總耗時保證。重跑會在 `build/cow-benchmark-*` 輸出 `samples.csv` 和 UART 紀錄。
