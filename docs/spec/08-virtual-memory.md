# 08 · 頁面、虛擬記憶體與 COW

<script setup>
import CowFlow from '../.vitepress/theme/components/CowFlow.vue'
</script>

程式用[虛擬位址](./glossary.md#virtual-address)存取程式碼和資料。CPU 用這個行程的頁表，把位址換成實體 RAM 的位置。WUWAOS 以 4 KiB 為單位管理，[頁面](./glossary.md#page-frame)是位址空間中的一段，頁框是 RAM 裡放這段內容的位置。

## 一個位址為什麼可以有兩份資料

虛擬位址和實體位址之間隔著頁表。假設父行程和子行程都讀取同一個虛擬頁面的某個位元組。若兩份頁表指向同一個實體頁框，兩邊看到相同內容。若其中一份頁表改指另一個頁框，虛擬位址與頁內偏移保持原樣，讀到的內容已能分開。這是行程隔離和 COW 都依賴的機制。

Sv39 把 4 KiB 頁面的位址拆成三級各 9 位元的頁索引與 12 位元頁內偏移。假設要翻譯某個頁內偏移為 `0x123` 的位址，硬體先用虛擬頁索引找到物理頁框，再把相同的 `0x123` 加到頁框起點。TLB 快取轉譯和權限，頁表改動後必須同步相關的快取項目。 [頁表與 TLB 同步](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c)

## 從虛擬位址到實體頁框

WUWAOS 使用 [Sv39](./glossary.md#sv39) 三級頁表。最後一級的 [PTE](./glossary.md#pte) 指出所在頁框與允許的操作。CPU 可把查到的轉譯保存在 [TLB](./glossary.md#tlb)。核心撤銷映射、換頁框或修改權限後，需要同步本地與相關遠端 Hart 的 TLB，再重用舊頁框。

保留一段合法記憶體時，未必當下就占 RAM。例如 `sbrk()` 可先擴大 [heap](./glossary.md#heap)，第一次讀寫才觸發[缺頁](./glossary.md#page-fault)。核心先查驗位址和權限，再依來源載入程式映像、提供零填充頁，或從 [Swap](./glossary.md#swap) 讀回既有資料。完整準備好頁框才公布有效 PTE，CPU 再重試原指令。

## 記憶體吃緊時怎麼辦

全系統的預設使用者常駐預算為 128 個頁框，最低可調至 8 個。核心用 [FIFO](./glossary.md#fifo) 選擇最早入駐且符合換出條件的頁框。暫時固定的頁框、含遠端執行中映射的頁框，以及 fork 準備期間受保護的來源都會跳過。

- [Clean 頁](./glossary.md#dirty-page)可用原映像、零頁或有效 Swap 副本重建。
- Dirty 頁需要保存修改。核心撤銷映射並同步 TLB 後寫出；失敗時還原映射，保留 RAM 資料。沒有可用的 Swap 時，仍可丟棄能重建的頁，無法寫出需要保存的頁會形成資源限制。

### 缺頁、TLB miss 與置換是三個時刻

TLB 找不到轉譯時，CPU 可以查頁表。如果頁表已有有效且允許此次存取的映射，指令可繼續。需要核心準備頁面或重新處理權限時，才會進入缺頁流程。處理缺頁時正好需要新的頁框且達到常駐預算，才可能連帶觸發頁框置換。

FIFO 用入駐順序挑選頁框。教材另外可以比較 OPT、LRU、Second Chance 等策略使用的資訊。WUWAOS 會跳過目前不適合換出的候選頁框，並在需要保存且可換出時才寫入 Swap。

## 寫入時複製

`fork()` 先建立獨立子頁表，已在 RAM 的頁面由父子共用頁框。原本可寫的 PTE 暫時清除寫權限，並設置 [COW](./glossary.md#cow) 標記。首次寫入觸發缺頁，核心檢查原權限後依頁框引用數處理：

- 引用大於一份時，複製 4 KiB 並將寫入者的 PTE 改到新頁框。
- 只剩一份引用時，恢復原頁框的寫權限。

例如父子一開始在某頁都讀到 7。子行程第一次寫入 9 時，硬體依 PTE 觸發例外。若該頁框仍有父子的兩份引用，核心複製頁框並只更新子行程的映射。重試後子讀到 9，父仍讀到 7。若稍後父行程也寫入同一虛擬頁，只剩父行程引用原頁框，核心便可原地恢復寫入權限。 [COW 缺頁處理](https://github.com/johnson931128/WUWAOS/blob/main/kernel/mm/vm.c#L344)

<CowFlow />

核心用 `user_copy_to()` 寫入使用者緩衝區時也會先解決 COW。原本唯讀的程式碼維持唯讀。已換出的頁由父子各自持有獨立 Swap 副本。共享頁框換出時整組撤銷映射，必要的資料分別寫入各映射的槽位，失敗就復原整組 PTE。最後一份映射撤銷並完成 TLB 同步後，才歸還頁框。

可用 `vmstat` 查看常駐頁框、COW 缺頁、複製次數和 Swap I/O。`make test-cow` 的既有驗收涵蓋低常駐預算、共享換出、回滾與 2／3 Hart。整機實體配置池耗盡和部分磁區已成功寫入後才失敗仍未驗證。 [VM 實作](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/mm/vm.c) · [COW 規格與量測](../SPEC-35-copy-on-write.md) · [早期分頁規格](../SPEC-10-demand-paging.md)

## 動手觀察

- [Sv39 位址轉譯](../topics/memory.md)可以選擇虛擬位址，逐層查看頁表索引。
- [FIFO 置換與 Swap](../topics/paging.md)可以看頁框成功寫出，或在磁碟寫入失敗後恢復原映射。

## 恐龍書與課堂筆記

恐龍書第 10 版第 9–10 章與 §11.6。IA-32／PAE、分段、其他置換策略與配置方法提供架構對照。

- [主記憶體、分段、分頁與 TLB](../theory/09-main-memory.md)
- [IA-32 分段、分頁與 PAE 算例](../theory/096-ia32.md)
- [隨需分頁、COW、頁面置換與配置](../theory/10-virtual-memory.md)
- [Swap 空間管理](../theory/11-storage.md)
- [期末名詞表與複習例子](../theory/final-glossary.md)
