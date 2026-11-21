# 08 · 頁面、虛擬記憶體與 COW

程式用[虛擬位址](./glossary.md#virtual-address)存取程式碼和資料。CPU 用這個行程的頁表，把位址換成實體 RAM 的位置。WUWAOS 以 4 KiB 為單位管理，[頁面](./glossary.md#page-frame)是位址空間中的一段，頁框是 RAM 裡放這段內容的位置。

## 從虛擬位址到實體頁框

WUWAOS 使用 [Sv39](./glossary.md#sv39) 三級頁表。最後一級的 [PTE](./glossary.md#pte) 指出所在頁框與允許的操作。CPU 可把查到的轉譯保存在 [TLB](./glossary.md#tlb)。核心撤銷映射、換頁框或修改權限後，需要同步本地與相關遠端 Hart 的 TLB，再重用舊頁框。

保留一段合法記憶體時，未必當下就占 RAM。例如 `sbrk()` 可先擴大 [heap](./glossary.md#heap)，第一次讀寫才觸發[缺頁](./glossary.md#page-fault)。核心先查驗位址和權限，再依來源載入程式映像、提供零填充頁，或從 [Swap](./glossary.md#swap) 讀回既有資料。完整準備好頁框才公布有效 PTE，CPU 再重試原指令。

## 記憶體吃緊時怎麼辦

全系統的預設使用者常駐預算為 128 個頁框，最低可調至 8 個。核心用 [FIFO](./glossary.md#fifo) 選擇最早入駐且符合換出條件的頁框。暫時固定的頁框、含遠端執行中映射的頁框，以及 fork 準備期間受保護的來源都會跳過。

- [Clean 頁](./glossary.md#dirty-page)可用原映像、零頁或有效 Swap 副本重建。
- Dirty 頁需要保存修改。核心撤銷映射並同步 TLB 後寫出；失敗時還原映射，保留 RAM 資料。沒有可用的 Swap 時，仍可丟棄能重建的頁，無法寫出需要保存的頁會形成資源限制。

## 寫入時複製

`fork()` 先建立獨立子頁表，已在 RAM 的頁面由父子共用頁框。原本可寫的 PTE 暫時清除寫權限，並設置 [COW](./glossary.md#cow) 標記。首次寫入觸發缺頁，核心檢查原權限後依頁框引用數處理：

- 引用大於一份時，複製 4 KiB 並將寫入者的 PTE 改到新頁框。
- 只剩一份引用時，恢復原頁框的寫權限。

核心用 `user_copy_to()` 寫入使用者緩衝區時也會先解決 COW。原本唯讀的程式碼維持唯讀。已換出的頁由父子各自持有獨立 Swap 副本。共享頁框換出時整組撤銷映射，必要的資料分別寫入各映射的槽位，失敗就復原整組 PTE。最後一份映射撤銷並完成 TLB 同步後，才歸還頁框。

可用 `vmstat` 查看常駐頁框、COW 缺頁、複製次數和 Swap I/O。`make test-cow` 的既有驗收涵蓋低常駐預算、共享換出、回滾與 2／3 Hart。整機實體配置池耗盡和部分磁區已成功寫入後才失敗仍未驗證。 [VM 實作](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/mm/vm.c) · [COW 規格與量測](../SPEC-35-copy-on-write.md) · [早期分頁規格](../SPEC-10-demand-paging.md)

接著讀[WUFS 與磁碟復原](./09-filesystem.md)。
