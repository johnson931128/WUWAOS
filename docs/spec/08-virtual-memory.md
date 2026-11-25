# 08 · 頁面、虛擬記憶體與 COW

<script setup>
import CowFlow from '../.vitepress/theme/components/CowFlow.vue'
</script>

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

如果兩個程式都以為自己從位址 `0x400000` 開始執行，它們在實體 RAM 裡豈不是會撞在一起？而且當實體記憶體不夠裝下所有行程、或 `fork()` 剛複製完一個大行程馬上就 `exec()` 覆蓋掉時，該如何既做到行程隔離，又不浪費寶貴的實體 RAM？這就是**虛擬記憶體（Virtual Memory）**、**按需分頁（Demand Paging）**與**寫入時複製（Copy-on-Write）**要聯手解決的問題。
</div>

## 恐龍書核心理論：分頁、按需載入與頁面置換

恐龍書第 9–10 章把虛擬記憶體拆解成三個環環相扣的機制：

1. **分頁與多級頁表（Paging）**：將位址空間切成長度固定的 4 KiB [頁面（Page）](./glossary.md#page-frame)，實體 RAM 切成同樣大小的頁框（Frame）。每個行程擁有獨立頁表，由硬體 MMU 與高速快取 [TLB](./glossary.md#tlb) 負責把虛擬位址轉成實體位址——兩個行程都能用 `0x400000`，卻對應到完全不同的實體頁框。
2. **按需分頁（Demand Paging）**：程式啟動或呼叫 `sbrk()` 擴大 [heap](./glossary.md#heap) 時，核心不急著立刻配置實體頁框；等到程式第一次存取該頁面觸發[缺頁例外（Page Fault）](./glossary.md#page-fault)時，核心才動態補頁，再重試同一條指令。
3. **頁面置換與 Swap**：當實體頁框吃緊時，透過置換演算法（如 [FIFO](./glossary.md#fifo)）挑出受害頁框。未修改的 Clean 頁可直接丟棄（日後重讀映像或補零），修改過的 [Dirty 頁](./glossary.md#dirty-page)則必須先寫到磁碟 [Swap](./glossary.md#swap) 區保存。

## 從虛擬位址到實體頁框（WUWAOS Sv39 實例）

WUWAOS 採用 RISC-V 64 位元的 [Sv39](./glossary.md#sv39) 三級頁表（`L2 → L1 → L0`，每頁 4 KiB），並於 `kernel/mm/vm.h` 明確切分使用者虛擬位址空間：

```c
#define USER_ENTRY              0x400000UL  // 使用者程式碼起始位址
#define USER_STACK              0x800000UL  // 主執行緒使用者堆疊
#define USER_THREAD_STACK_BASE  0x900000UL  // 額外執行緒堆疊區（附防護頁）
```

每次核心撤銷映射、換出頁框或修改 [PTE](./glossary.md#pte) 權限後，都必須同步本地與相關遠端 Hart 的 TLB，確認沒有舊快取殘留後才重用該頁框。

## 記憶體吃緊時怎麼辦

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 常駐預算與回滾保護</span>

- **常駐頁框上限**：全系統預設使用者常駐預算為 **128 個頁框**（可透過 `vm_control` 調低至 8 個以測試置換壓力）。
- **FIFO 換出與跳過規則**：核心挑選最早入駐的受害頁框，但會自動跳過暫時固定（Pinned）、含遠端執行中映射，或正在 `fork` 保護中的頁框。
- **Swap 失敗回滾**：Dirty 頁寫往 Swap 時，核心會先撤銷映射並同步 TLB 再寫出；若磁碟寫入失敗，核心會**還原原映射並保留 RAM 資料**，絕不靜默遺失使用者修改。
</div>

### 缺頁、TLB miss 與置換是三個時刻

TLB 找不到轉譯時，CPU 可以查頁表。如果頁表已有有效且允許此次存取的映射，指令可繼續。需要核心準備頁面或重新處理權限時，才會進入缺頁流程。處理缺頁時正好需要新的頁框且達到常駐預算，才可能連帶觸發頁框置換。

FIFO 用入駐順序挑選頁框。教材另外可以比較 OPT、LRU、Second Chance 等策略使用的資訊。WUWAOS 會跳過目前不適合換出的候選頁框，並在需要保存且可換出時才寫入 Swap。

## 寫入時複製

傳統 `fork()` 逐頁複製整個父行程記憶體既慢又浪費（因為子行程常常緊接著就呼叫 `exec()`）。透過 **Copy-on-Write（[COW](./glossary.md#cow)）**，`fork()` 只建立獨立子頁表，已在 RAM 中的頁面先由父子共享同一頁框，並把原本可寫的 PTE 暫時清除寫權限、打上 COW 標記：

- 只要雙方都只「讀取」，完全不需要複製記憶體。
- 任何一方首次「寫入」時觸發 Store Page Fault，核心檢查原權限與頁框引用計數：
  - **引用 > 1**：分配新 4 KiB 頁框並複製內容，寫入者的 PTE 改指新頁框並恢復可寫。
  - **引用 = 1**：只剩自己使用，直接恢復原頁框的寫權限，連複製都省了。

例如父子一開始在某頁都讀到 7。子行程第一次寫入 9 時，硬體依 PTE 觸發例外。若該頁框仍有父子的兩份引用，核心複製頁框並只更新子行程的映射。重試後子讀到 9，父仍讀到 7。若稍後父行程也寫入同一虛擬頁，只剩父行程引用原頁框，核心便可原地恢復寫入權限。 [COW 缺頁處理](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c#L344)

<CowFlow />

核心透過 `user_copy_to()` 寫入使用者緩衝區時也會先解決 COW。原本唯讀的程式碼頁永遠維持唯讀；已換出的頁由父子各自持有獨立 Swap 副本。可用 Shell 指令 `vmstat` 即時觀察常駐頁框、COW 缺頁、複製次數與 Swap I/O。

## 動手觀察

- [Sv39 位址轉譯互動工具](../topics/memory.md)：輸入任意虛擬位址，逐層拆解 `VPN[2] / VPN[1] / VPN[0] / offset` 與葉節點 PTE 權限。
- [FIFO 置換與 Swap 實驗室](../topics/paging.md)：實際比較 Clean 頁丟棄、Dirty 頁換出，以及 Swap 磁碟寫入失敗時的原子回滾。

原始碼與驗收：[VM 實作 (`kernel/mm/vm.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/mm/vm.c) · [位址佈局 (`kernel/mm/vm.h`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/mm/vm.h) · [COW 規格與量測](../SPEC-35-copy-on-write.md) · [早期分頁規格](../SPEC-10-demand-paging.md)
