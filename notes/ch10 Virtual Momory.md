---
title: ch10 Virtual Momory

---

# 9.6.1 IA-32 Architecture：IA-32 架構
## 前言
IA-32 系統中的記憶體管理主要分成兩個部分：**segmentation（分段）** 與 **paging（分頁）**。整體流程可以想成一條位址翻譯流水線：CPU 一開始產生的是 **logical address（邏輯位址）**，這個位址先交給 **segmentation unit（分段單元）**；分段單元會把邏輯位址轉換成 **linear address（線性位址）**。接著，線性位址再交給 paging unit（分頁單元），最後由分頁單元產生真正能用來存取主記憶體的 physical address（實體位址）。

因此，在 IA-32 中，完整的位址轉換可以寫成：

CPU 產生 logical address
→ segmentation unit 轉成 linear address
→ paging unit 轉成 physical address
→ 存取 physical memory

這裡很重要的一點是：**segmentation unit 加上 paging unit，合起來就是 IA-32 記憶體管理單元 MMU 的核心功能**。 換句話說，MMU 不是只做分頁，也不是只做分段，而是把兩者串起來完成整個位址翻譯。

## IA-32 中 logical address 到 physical address 的轉換
![image](https://hackmd.io/_uploads/HkMJ0aR1fe.png)
圖 9.21 展示的是整體大架構。CPU 並不會一開始就直接產生實體位址，而是先產生 logical address。這個 logical address 進入 segmentation unit 後，會被轉換成 linear address。接著 linear address 再進入 paging unit，最後才得到 physical address，並用這個 physical address 存取 physical memory。

這張圖的關鍵不是細節，而是順序：

CPU 看到的是 logical address。
segmentation unit 產生的是 linear address。
paging unit 產生的是 physical address。
真正送到主記憶體的是 physical address。

所以在 IA-32 架構中，**linear address** 是 **segmentation** 和 **paging** 之間的中間產物。它不是 CPU 一開始直接產生的，也不是最後真正丟到記憶體匯流排上的實體位址。

## 9.6.1 IA-32 Segmentation：IA-32 分段機制
IA-32 架構允許單一 segment 最大可以到 **4 GB**。每個 process 最多可以擁有 **16 K 個 segments**。這個數字不是隨便來的，因為 IA-32 的 segment selector 中有一部分位元用來表示 segment number，而能表示的 segment 數量最後會導向最多 16 K 個 segment。

一個 process 的 logical address space 會被切成兩大部分：

第一部分是最多 **8 K 個 private segments**。這些 segment 是該 process 私有的，只屬於特定 process。關於這些 private segments 的資訊會被存放在 **local descriptor table**，**LDT（區域描述表）** 裡。

第二部分是最多 **8 K 個 shared segments**。這些 segment 可以被所有 processes 共享。關於 shared segments 的資訊會被存放在 **global descriptor table**，**GDT（全域描述表）** 裡。

所以可以把 LDT 和 GDT 想成兩本「segment 資料冊」：

==LDT：記錄某個 process 自己私有的 segments==。
==GDT：記錄所有 processes 都可能共用的 shared segments==。

LDT 和 GDT 中的每一個 entry 都是一個 **8-byte segment descriptor**（8 位元組的段描述子）。這個 descriptor 會記錄某個 segment 的詳細資訊，其中最重要的是這個 segment 的 **base location**（基底位址） 和 **limit**（界限 / 長度限制）。

base location 代表這個 segment 在線性位址空間中的起始位置。
limit 代表這個 segment 的合法範圍有多大。

## 單一 Segment 最大 4 GB 的意義
在 IA-32 的分段機制中，一個 segment 可以描述的最大位址範圍是 4 GB。這代表 segment 內部的 offset 最多可以從 `0` 延伸到 `2³² − 1`，也就是：

2³² bytes = 4,294,967,296 bytes = 4 GB

原因在於 IA-32 的 logical address 中，**offset 欄位是 32-bit**。32-bit 能表示的位址數量就是 2³² 個 byte，因此單一 segment 在理論上最多可以涵蓋完整的 32-bit 線性位址空間。

## Flat Segmentation Model 的意義

在許多現代 32-bit OS 中，segmentation 不再被拿來細分 code segment、data segment、stack segment 的實體範圍，而是把多個 segment 的 base 都設成 0，limit 都設成 4 GB。

這樣一來，segmentation 幾乎不負責真正的記憶體切割，只保留基本的權限與保護功能。真正複雜的記憶體管理，例如虛擬位址到實體位址的映射、page protection、page fault 等，則交給 paging 負責。

所以可以把 IA-32 的現代用法理解成：

segmentation：提供基本位址轉換與保護檢查
paging：負責主要的記憶體映射與管理
## IA-32 segmentation 的位址轉換
![image](https://hackmd.io/_uploads/rk0TgAA1zl.png)
IA-32 的 logical address 不是單純一個數字，而是一組 pair：

**logical address = (selector, offset)**

這裡的 **selector** 是 16-bit，而 **offset** 是 32-bit。selector 的任務是「選出要使用哪一個 segment descriptor」。offset 的任務是「指出在該 segment 內部偏移多少位置」。

**圖 9.22 的流程可以整理成：**
1. CPU 產生 logical address，裡面包含 selector 和 offset。
2. selector 被拿去查 descriptor table，也就是 LDT 或 GDT。
3. 查到對應的 segment descriptor 後，系統取得該 segment 的 base address。
4. 同時，系統會檢查 offset 是否超過該 segment 的 limit。
5. 如果 offset 合法，就把 base address 加上 offset。
6. 最後得到 32-bit linear address。

也就是：**linear address = segment base + offset**
這個過程非常像在找一棟大樓裡的房間。selector 先決定是哪一棟樓，也就是哪一個 segment；segment descriptor 告訴系統這棟樓從哪裡開始、最大範圍到哪裡；offset 則表示從這棟樓的起點往上走幾公尺。只要 offset 沒有走出這棟樓的合法範圍，就可以得到最後的 linear address。

## selector 的內部格式：s、g、p
![image](https://hackmd.io/_uploads/BJS1oN1xMe.png)
課本接著把 16-bit selector 拆成三個欄位：

**s：13 bits**
* s 代表 segment number，也就是要選哪一個 segment descriptor。因為 s 有 13 bits，所以最多可以表示 2¹³ = 8192，也就是 8 K 個 entry。

**g：1 bit**
* g 用來判斷這個 segment descriptor 是在 GDT 還是在 LDT。
    * 如果指向 GDT，就表示這是全域共享的 segment。
    * 如果指向 LDT，就表示這是 process 私有的 segment。

**p：2 bits**
* p 跟 protection（保護）有關，用來處理權限與保護資訊。

所以 selector 雖然只有 16 bits，但其實包含三種資訊：選哪個 segment、查哪張 descriptor table、以及保護相關設定。
## segment register 與 descriptor 快取
IA-32 機器有 **6 個 segment registers（段暫存器）**，因此一個 process 在同一時間最多可以直接定址 6 個 segment。
IA-32 機器有 6 個 segment registers（段暫存器），因此一個 process 在同一時間最多可以直接定址 6 個 segment。

不過，機器不只單純存 6 個 selector。它還有 **6 個 8-byte microprogram registers**，用來保存 LDT 或 GDT 中對應 descriptor 的內容。這裡可以把它理解成一種快取機制。

如果每次記憶體存取都要重新去 LDT 或 GDT 讀 descriptor，效能會非常差。因此 Pentium 會把目前常用的 segment descriptor 快取在這些內部暫存器裡。這樣每次進行記憶體參考時，就不需要反覆從記憶體讀 descriptor，可以大幅減少額外開銷。

這個設計的精神跟 TLB 有點像：都是為了避免每次位址轉換都去慢速記憶體查表。
## linear address 的形成與錯誤檢查
IA-32 的 linear address 長度是 **32 bits**。它的形成方式很直接：segment register 指向 LDT 或 GDT 中的正確 entry，系統從該 entry 取得 segment 的 base 和 limit。

接著會先用 **limit** 來檢查 offset 是否合法。這一步是 protection 的核心。如果 offset 超過 segment 的合法範圍，就代表程式嘗試存取不該碰的記憶體位置，系統會產生 **memory fault（記憶體錯誤）**，並觸發 trap 進入作業系統處理。

如果 offset 是合法的，系統就會把 offset 加到 segment base 上，產生 32-bit linear address。

所以分段機制不只是做位址轉換，也同時做了基本的邊界檢查與保護。這是 segmentation 在 OS 裡很重要的意義：它不只把位址換成另一種形式，也能防止 process 越界存取。
# 9.6.1.2 IA-32 Paging：IA-32 分頁機制
## 前言
前面 IA-32 segmentation 做完後，會得到一個 32-bit linear address（線性位址）。接下來，paging unit 會把這個 linear address 轉換成真正用來存取主記憶體的 **physical address（實體位址）**。

**所以 IA-32 的完整記憶體轉換流程是：**
1. CPU 產生 logical address
2. segmentation unit 轉成 linear address
3. paging unit 轉成 physical address
4. 存取 physical memory

**這一節討論的是第二段，也就是：**
* **linear address** → **physical address**

在 IA-32 中，page size 可以是 4 KB 或 4 MB。一般情況下，標準頁面大小是 4 KB；而 4 MB page 則屬於較大的 page，用來減少 page table 管理成本與 TLB 壓力。

## 4 KB Page：32-bit linear address 的切法
對於 4 KB pages，IA-32 採用 two-level paging scheme（二層分頁架構）。32-bit linear address 會被切成三個欄位：
![image](https://hackmd.io/_uploads/r1cnwCZxGe.png)
**其中：**
* **`p1`：** 10 bits，代表 page directory index
* **`p2`：** 10 bits，代表 page table index
* **`d`：** 12 bits，代表 page offset

為什麼 offset 是 12 bits？因為一個 page 大小是 4 KB：

4 KB = 4096 bytes = 2¹² bytes

所以 page 內部要定位某一個 byte，需要 12 bits 的 offset。剩下的 20 bits 則用來查表。IA-32 把這 20 bits 分成兩段，每段 10 bits，因此形成二層結構。

## Two-Level Paging 的轉換流程
![image](https://hackmd.io/_uploads/Skgtd0-xMg.png)
IA-32 的 4 KB paging 可以想成「先查目錄，再查表，最後進入頁面」。

第一層是 **page directory（頁目錄）**。CPU 會用 linear address 最高的 10 bits，也就是 `p1`，去 page directory 裡找到某個 entry。這個 entry 會指向第二層的 **page table（頁表）**。

第二層是 **page table**。接著 CPU 使用中間的 10 bits，也就是 `p2`，去 page table 裡找到某個 entry。這個 entry 會指向真正的 4 KB **physical page frame**（實體頁框）。

最後，最低的 12 bits，也就是 `d`，會當作 page offset，用來指定在該 4 KB page frame 裡面的哪一個 byte。

**整體流程可以寫成：**

* `linear address[31:22]` → 查 page directory
* `linear address[21:12]` → 查 page table
* `linear address[11:0]` → page offset
最後得到 physical address

**總結**
* **圖 9.23 的意思是：CR3 指向一個 page directory；page directory 裡有 1024 個 entries，每個 entry 可以指向一張 page table；每張 page table 又有 1024 個 entries，每個 entry 對應一個 4 KB physical page frame；最後用 12-bit offset 決定資料位於該 page frame 內的哪個 byte。**

## CR3 Register：目前 process 的 page directory 在哪裡
圖 9.23 中有一個很重要的暫存器：**CR3 register**。

CR3 會指向「目前 process 使用的 page directory」。因為每個 process 通常都有自己的 virtual address space，所以每個 process 也會有自己的 page directory。當作業系統進行 context switch，從 Process A 切到 Process B 時，CR3 通常也會被換掉，指向 Process B 的 page directory。

這也是為什麼不同 process 可以看到「看起來一樣」的 virtual / linear address，但最後映射到完全不同的 physical memory。

例如 Process A 和 Process B 都可以使用位址：
> 0x00400000

但因為 CR3 指向不同的 page directory，所以這個 linear address 最後可能會對應到不同的 physical frame。這就是虛擬記憶體隔離的核心魔法之一

## 為什麼要 Two-Level Paging
如果只用單層 page table，32-bit address space 搭配 4 KB page 時，會需要：
如果只用單層 page table，32-bit address space 搭配 4 KB page 時，會需要：

4 GB / 4 KB = 2²⁰ pages = 1,048,576 個 page table entries

如果每個 entry 是 4 bytes，單一 process 的 page table 就需要：

1,048,576 × 4 bytes = 4 MB

也就是說，每個 process 光是 page table 就可能要吃掉 4 MB。若系統有很多 process，這非常浪費。

Two-level paging 的好處是：**不用一次建立完整 4 GB 位址空間的所有 page tables**。

Page directory 本身只有 1024 個 entries，大小剛好是：

1024 × 4 bytes = 4096 bytes = 4 KB

每個 page table 也是 1024 個 entries，大小也是 4 KB。只有當某段 linear address space 真的需要使用時，作業系統才需要配置對應的 page table。這樣可以大幅節省記憶體。
## Page Directory 和 Page Table 的大小關係
在非 PAE 的 IA-32 4 KB paging 中：
* Page directory 有 1024 entries，因為 p1 是 10 bits，2¹⁰ = 1024。
* 每個 page table 有 1024 entries，因為 p2 是 10 bits，2¹⁰ = 1024。
* 每個 page frame 是 4 KB，因為 offset 是 12 bits，2¹² = 4096 bytes。

所以一個 page table 可以映射：
* 1024 × 4 KB = 4 MB

而一個 page directory 可以指向 1024 個 page tables，因此總共可以映射：
* 1024 × 4 MB = 4 GB

這剛好就是 32-bit linear address space 的大小。

## 4 MB Page：跳過 Page Table 的大頁面模式'
IA-32 也允許使用 **4 MB page**。這時候 page directory entry 裡面有一個欄位叫做 **Page_Size flag**。如果 Page_Size flag 沒有被設定，代表使用標準 4 KB page：
* page directory → page table → 4 KB page

如果 Page_Size flag 被設定，代表這個 page directory entry 直接指向一個 4 MB page frame，不再需要第二層 page table：
* page directory → 4 MB page

在 4 MB page 模式下，linear address 的切法會變成：

**最高 10 bits：page directory index**
**最低 22 bits：4 MB page 內部 offset**
![image](https://hackmd.io/_uploads/ryI23RWlMl.png)

因為：

4 MB = 2²² bytes

所以需要 22 bits 才能表示 4 MB page 內部的 byte offset。

這個設計的好處是效率。少查一層 page table，轉換速度更快，而且 4 MB page 可以讓 TLB 一次涵蓋更大的記憶體範圍。缺點是彈性較差，因為配置單位變大後，容易造成內部浪費。這很像拿超大箱子裝東西，搬運很快，但可能箱子裡有很多空位。

## Page Table 也可以被 Swap 到 Disk
課本接著提到，為了更有效率地使用 physical memory，IA-32 的 page tables 本身也可以被換出到 disk。

這點很有趣，因為 page table 是拿來管理記憶體的資料結構，但它自己也會佔記憶體。如果系統記憶體壓力很大，某些目前不常用的 page table 可以暫時放到磁碟上。

此時 page directory entry 裡會有一個 invalid bit，用來表示這個 entry 指向的 page table 目前不在 memory，而是在 disk。若 page table 在 disk 上，作業系統可以利用 entry 裡剩下的 bits 記錄該 page table 在 disk 上的位置。

當之後真的需要使用這個 page table 時，系統再把它從 disk 載回 memory。

這個機制說明了一個核心觀念：paging 不只是「page frame 可以被換出」，連管理這些 page 的 page table，在某些情況下也可以被作業系統管理與換出。
## PAE：Page Address Extension
![image](https://hackmd.io/_uploads/S1jmT0bxze.png)
為了突破 32-bit 架構只能定址 4 GB physical memory 的限制，Intel 引入了 **PAE（Page Address Extension，頁面位址擴充）**。

PAE 的重點不是讓單一 32-bit process 直接看到超過 4 GB 的 virtual address space，而是讓系統能夠支援更大的 **physical memory**。

PAE 將 paging 結構從原本的 two-level paging 變成 **three-level paging**。新增的一層叫做：
* **page directory pointer table，PDPT**

在 PAE 的 4 KB page 模式中，32-bit linear address 大致會被切成：
* 最高 2 bits：選 PDPT entry
* 接著 9 bits：選 page directory entry
* 接著 9 bits：選 page table entry
* 最低 12 bits：page offset


---

## 先看 page-table entry 到底是什麼
### 前言
Page table 可以想成一個陣列。

每一格叫做 **page-table entry, PTE**。

每個 PTE 的任務是告訴 CPU：

「這個 virtual page 對應到哪一個 physical page frame，還有這一頁有哪些權限設定。」

所以一個 PTE 裡面要放兩種東西：

**第一種：實體頁框的起始位址。**
也就是這個 virtual page 對應到 RAM 裡哪一個 4 KB page frame。

**第二種：控制旗標 flags。**
例如這頁是否存在記憶體、可不可寫、user mode 能不能碰、有沒有被存取過等等。

---

### 為什麼 32-bit entry 夠用？

非 PAE 的 IA-32 原本只能產生 **32-bit physical address**。

也就是 physical address 長這樣：

```txt
32-bit physical address
= 20-bit page frame number + 12-bit page offset
```

為什麼 offset 是 12 bits？

因為 page size 是 4 KB：

```txt
4 KB = 4096 bytes = 2^12 bytes
```

所以頁面內部位置需要 12 bits。

也就是說，一個 physical address 可以拆成：

```txt
[ physical page frame number ][ offset ]
          20 bits              12 bits
```

---

### 但 PTE 不需要存 offset

這是關鍵。

PTE 不需要存 offset，因為 offset 直接來自原本的 linear address 最低 12 bits。

PTE 只需要告訴 CPU：

「這個 page 的實體起點在哪裡。」

而每個 page frame 都是 4 KB 對齊，所以它的起始位址最低 12 bits 一定是 0。

例如某個 page frame 起始位址可能是：

```txt
0x00123000
```

最後三個 hex digits 是 `000`，因為 4 KB 對齊。

所以 PTE 只需要存前面的部分：

```txt
0x00123
```

也就是 **20-bit physical frame number**。

---

### 所以 32-bit PTE 大概長這樣

非 PAE 模式下，一個 PTE 是 32 bits：

```txt
bit 31                    bit 12 bit 11        bit 0
+-------------------------------+-------------------+
| physical page frame base      | flags             |
| 20 bits                       | 12 bits           |
+-------------------------------+-------------------+
```

意思是：

高 20 bits：放 physical page frame 的位置。
低 12 bits：放 present、read/write、user/supervisor 等 flags。

所以原本是 32 bits 的原因可以很粗暴地記成：

**原本 physical address 是 32-bit，而 4 KB page 的 offset 是 12-bit，所以 page frame number 是 20-bit；20-bit frame number 加上一些 flags，剛好包在一個 32-bit entry 裡。**

---

### Page-directory entry 也是同樣概念

Page-directory entry，PDE，也是一個 32-bit entry。

只是它指向的東西不同。

PTE 指向 **4 KB physical page frame**。
PDE 指向 **page table 的實體起始位置**。

但 page table 本身也是放在記憶體中，而且通常也是 4 KB 對齊，所以 PDE 一樣可以用高 20 bits 存 page table 的 base address，低位元放 flags。

所以 PDE 也做成 32-bit。

---

### 那為什麼 PAE 後變 64-bit？

因為 PAE 要讓 32-bit IA-32 可以使用超過 4 GB 的實體記憶體。

原本 32-bit physical address 最多只能編號：

```txt
2^32 bytes = 4 GB
```

PAE 把 physical address 擴大成 **36 bits**。

所以 physical address 變成：

```txt
36-bit physical address
= 24-bit page frame number + 12-bit offset
```

因為：

```txt
36 - 12 = 24
```

也就是說，原本 PTE 只需要放 20-bit frame number。
PAE 後，PTE 需要放 **24-bit frame number**。

frame number 變長了，flags 又還是要放，所以 Intel 乾脆定義新的 PAE paging format：

**每個 PDE / PTE 從 32-bit entry 變成 64-bit entry。**

不是原本那格自己膨脹，而是 CPU 開啟 PAE 後，改用另一套表格格式。

---

### 最直覺版本

非 PAE：

```txt
PTE = 32 bits = 4 bytes

裡面放：
20-bit physical frame number
+ flags
```

PAE：

```txt
PTE = 64 bits = 8 bytes

裡面放：
24-bit physical frame number
+ 更多保留空間 / flags
```

PAE 變 64-bit 的目的，就是讓 entry 可以裝得下更大的 physical frame number，讓實體記憶體可以超過 4 GB。

---


變 64-bit 之後，最重要的後果是：

**PDE / PTE 每一格從 4 bytes 變成 8 bytes。**

也就是 page directory 和 page table 裡面的「每個 entry」變胖了。它可以塞下更長的 **physical page frame number**，所以可以指到超過 4 GB 以外的實體記憶體位置。

---

原本非 PAE 是這樣：

```txt
一個 PDE / PTE = 32 bits = 4 bytes

一張 page table 大小 = 4 KB
所以一張 page table 可以放：

4096 bytes / 4 bytes = 1024 entries
```

所以原本 32-bit linear address 可以切成：

```txt
10 bits | 10 bits | 12 bits
  p1       p2      offset
```

也就是：

```txt
page directory index | page table index | page offset
```

---

PAE 開啟後，entry 變成 64-bit：

```txt
一個 PDE / PTE = 64 bits = 8 bytes

一張 page table 大小還是 4 KB
所以一張 page table 可以放：

4096 bytes / 8 bytes = 512 entries
```

512 個 entry 需要幾個 bits 來選？

```txt
512 = 2^9
```

所以原本的 `10 bits` index 會變成 `9 bits` index。

---

因此 PAE 後，32-bit linear address 不再切成：

```txt
10 | 10 | 12
```

而是切成：

```txt
2 bits | 9 bits | 9 bits | 12 bits
```

也就是：

```txt
PDPT index | page directory index | page table index | offset
```

這也是為什麼 PAE 會多出一層 **PDPT，Page Directory Pointer Table**。

課本寫 PAE 讓 IA-32 從 two-level paging 變成 three-level paging，也就是新增 page directory pointer table；講義也整理成 top two bits 指向 PDPT，page-directory 和 page-table entries 變成 64 bits。 

---

整個 PAE 查表流程變成：

```txt
CR3
 ↓
PDPT，用最高 2 bits 選其中一個 entry
 ↓
Page Directory，用接下來 9 bits 選其中一個 PDE
 ↓
Page Table，用接下來 9 bits 選其中一個 PTE
 ↓
Physical Page Frame
 ↓
用最後 12 bits offset 找 page 裡面的 byte
```

所以 PAE 後的路線是：

```txt
CR3 → PDPT → Page Directory → Page Table → 4 KB Page Frame
```

---

那變 64-bit 到底帶來什麼爽點？

原本 32-bit PTE 裡面只能放 **20-bit physical page number**。再加上 12-bit offset，形成：

```txt
20 + 12 = 32-bit physical address
```

所以最多只能指到：

```txt
2^32 bytes = 4 GB physical memory
```

PAE 後，64-bit PTE 裡面可以放更長的 **24-bit physical page number**。再加上 12-bit offset，形成：

```txt
24 + 12 = 36-bit physical address
```

所以最多可以指到：

```txt
2^36 bytes = 64 GB physical memory
```

課本也明確說，PAE 把 page-directory / page-table entries 從 32 bits 增加到 64 bits，讓 page tables 和 page frames 的 base address 從 20 bits 擴到 24 bits；配上 12-bit offset 後，IA-32 的 physical address space 變成 36 bits，也就是最多 64 GB。

---

但這裡不要搞混：**PAE 沒有讓單一 32-bit process 看到超過 4 GB 的 linear address space。**

因為 linear address 還是 32 bits：

```txt
2^32 bytes = 4 GB
```

PAE 改的是後面那段：

```txt
linear address → physical address
```

也就是：單一 process 的地圖大小還是 4 GB，但這張地圖可以被 OS 映射到更大的實體 RAM 世界裡。整台系統可以用到 64 GB RAM，但單一 32-bit process 通常還是只能看到 4 GB address space。

---

所以最短版就是：

**變 64-bit 之後，每個 PDE / PTE 變成 8 bytes，因此一張 4 KB page table 只能放 512 個 entries，index 從 10 bits 變 9 bits。為了補回 32-bit linear address 的完整範圍，PAE 多加一層 PDPT，用最高 2 bits 來選。真正的好處是 PTE 可以存更長的 physical frame number，讓 physical address 從 32 bits 擴到 36 bits，因此整台機器最多可支援 64 GB RAM。**

# Figure 9.24 Page address extensions
![image](https://hackmd.io/_uploads/ByrDIkGlfg.png)
這張圖是在畫 **PAE 開啟後，IA-32 怎麼把 32-bit linear address 翻成 4-KB page 的位置**。

最上面那條長條是 **32-bit linear address**，被切成四段：

```txt
bits 31~30   bits 29~21      bits 20~12      bits 11~0
    2 bits       9 bits          9 bits         12 bits
    ↓            ↓               ↓              ↓
  PDPT index   page directory   page table     offset
```

流程如下。

一開始 **CR3 register** 不再直接指向 page directory，而是指向 **page directory pointer table，PDPT**。這是 PAE 多出來的第一層。

接著 linear address 的最高 2 bits，也就是 **bits 31~30**，會拿去選 PDPT 裡面的某一個 entry。因為 2 bits 可以表示：

```txt
2² = 4
```

所以 PDPT 裡面有 4 個 entries。

選到 PDPT entry 之後，它會指向某一個 **page directory**。

然後 linear address 的 **bits 29~21**，也就是接下來的 9 bits，會拿去選 page directory 裡面的某一個 entry。因為 9 bits 可以表示：

```txt
2⁹ = 512
```

所以一個 page directory 裡面有 512 個 entries。

選到 page-directory entry 之後，它會指向某一個 **page table**。

接著 linear address 的 **bits 20~12**，也就是再下一段 9 bits，會拿去選 page table 裡面的某一個 entry。這個 entry 會指向真正的 **4-KB physical page frame**。

最後 linear address 的最低 12 bits，也就是 **bits 11~0**，當作 **offset**，用來決定資料在這個 4-KB page 裡面的哪一個 byte。

所以整張圖的查表路線就是：

```txt
CR3
 ↓
page directory pointer table
 ↓  用 bits 31~30 選
page directory
 ↓  用 bits 29~21 選
page table
 ↓  用 bits 20~12 選
4-KB page
 ↓  用 bits 11~0 找 page 內的位置
```

一句話整理：

**圖 9.24 表示 PAE 模式下，32-bit linear address 被切成 2 / 9 / 9 / 12 四段；CR3 先指向 PDPT，接著依序查 page directory、page table，最後用 12-bit offset 找到 4-KB page 裡的實際 byte。**


# ch10
## 第一張圖：從 `hello.exe` 檔案到準備執行
### 解釋
![image](https://hackmd.io/_uploads/HJWUPyGlzg.png)

第一張圖左邊畫的是：

`CPU → MMU → Physical Memory → Disk`

這條線表示程式執行時的資料流與位址轉換關係。CPU 不會直接去 disk 裡面執行 `hello.exe`。CPU 真正能高速存取的是 **physical memory**，也就是 RAM。Disk 只是長期儲存程式檔案的地方。

右邊 disk 裡面放著一個 `hello.exe`。這個 `hello.exe` 是一個 **executable file，可執行檔**。它不是 process，它只是靜態躺在硬碟裡的檔案。圖中 `hello.exe` 裡面分成兩大部分：上半部是 header，下半部是程式內容。

Header 裡面會記錄這個執行檔的重要資訊，例如：

`code segment` 在檔案哪裡、大小多少。
`data segment` 在檔案哪裡、大小多少。
程式進入點 `entry point` 在哪裡。
還有一些其他執行需要的 metadata。

下面的 `code segment` 和 `data segment` 則是程式真正的內容。`code segment` 存放機器指令，`data segment` 存放已初始化的全域變數或靜態資料。

所以第一張圖的核心意思是：

**一個程式在 disk 裡只是 executable file。它裡面有 header、code segment、data segment 等資訊。真正要執行時，OS 必須根據 header 的資訊，把它載入或映射成記憶體中的 process。**

---

### 第一張圖的重點：CPU 不直接跑 disk 裡的程式

CPU 執行程式時，發出的不是「disk address」，而是 memory address。CPU 產生位址後，會經過 **MMU**。MMU 的任務是把 CPU 看到的 virtual address / logical address 轉換成 physical memory 裡的 physical address。

所以程式執行時的概念不是：

`CPU 直接讀 disk 裡的 hello.exe`

而是：

`OS 讀取 hello.exe 的結構`
`OS 建立 process 的 address space`
`需要的部分被放進 physical memory`
`CPU 透過 MMU 存取 memory 中的內容`

這就是為什麼中間會畫出 MMU 和 Physical Memory。MMU 是虛擬記憶體機制的核心硬體；OS 則負責建立 page table、載入程式、處理 page fault。

---

## 第二張圖：OS / Loader 把 executable 建成 process image
### 解釋
![image](https://hackmd.io/_uploads/B1dN_1Mefx.png)

第二張圖比第一張多畫了一個綠色框框，標成 **image for hello process**。這個框框裡面有：

`stack and heap`
`data`
`code`

這表示 `hello.exe` 被執行後，不再只是原本 disk 裡的檔案格式，而是會被 OS 和 loader 整理成一個 **process image**。

Process image 可以理解成「一個 process 執行時應該看到的記憶體樣子」。它通常包含：

**Code/Text 區段**：放程式指令。
**Data 區段**：放全域變數、靜態變數等資料。
**Heap 區段**：給 `malloc()`、`new` 這類動態配置使用。
**Stack 區段**：放函式呼叫、return address、區域變數、參數等。

所以第二張圖右邊紅字寫的「loader 協助 OS 建立 image」就是這個意思：loader 會讀 executable file 的 header，知道哪些 segment 要被放到 process 的 virtual address space 裡，然後建立出 process image。

---

### 第二張圖的紅色箭頭意思

紅色箭頭從 `hello.exe` 指向綠色的 process image，意思是：

**OS 不是把 `hello.exe` 當成一坨不明資料丟進 memory，而是根據 executable header，把它拆解、整理，建立出 process 執行時需要的位址空間配置。**

原本 disk 裡的 `hello.exe` 可能只真的存有 code segment 和初始化 data segment。可是 process image 裡會多出 stack、heap 等執行期間才需要的區域。

也就是說：

`hello.exe` 是靜態檔案。
`process image` 是執行時的記憶體布局。
`process` 則是這個 image 加上 CPU 狀態、page table、PCB 等管理資訊後的動態實體。

---
## 這兩張圖合起來的故事
### 總結
第一張圖在說：`hello.exe` 原本是一個存在 disk 裡的 executable file，裡面有 header、code segment、data segment、entry point 等資訊。CPU 不能直接執行 disk 裡的檔案，必須經過 OS 載入與記憶體管理。

第二張圖在說：當 `hello.exe` 要執行時，OS / loader 會根據 executable file 的資訊，建立出 `hello process` 的 process image。這個 image 包含 code、data、heap、stack。接著透過 virtual memory 機制，這些區域不一定全部立刻進 RAM，而是可以按需載入。

所以整體可以整理成：

`hello.exe on disk`
→ loader 讀 header
→ OS 建立 process image
→ 建立 virtual address space
→ 建立 page table
→ 需要的 page 載入 physical memory
→ CPU 透過 MMU 執行程式

---

### 最核心結論

這兩張圖想建立的概念是：

**程式檔案不等於行程。**

`hello.exe` 是 disk 上的靜態 executable file；當 OS 執行它時，loader 會根據 header 建立 process image，包含 code、data、heap、stack。CPU 執行時透過 MMU 存取 physical memory，而 virtual memory 允許 OS 不必一次把整個 process image 都載入 RAM，只需要在 page 被實際使用時再載入。

## 第三張圖：把 `hello.exe` 變成 process image，並切成 pages
### 解釋
![image](https://hackmd.io/_uploads/ByD6KJzlMg.png)

左邊的 `CPU → MMU → Physical Memory` 表示程式真正執行時的硬體路徑。CPU 產生的是 logical address / virtual address，這個位址會交給 MMU。MMU 根據 page table 做位址轉換，最後才會對 physical memory 存取。

右邊的 disk 裡面，下方還保留原本的 `hello.exe`。這代表 `hello.exe` 原本只是一個躺在磁碟上的 executable file。它裡面有 header，header 記錄 code segment、data segment、entry point 等資訊。這些資訊會告訴 OS / loader：程式的 code 在哪裡、data 在哪裡、程式要從哪個 entry point 開始執行。

上方綠色框框是 **image for hello process**，也就是 `hello.exe` 被拿來執行後，OS 建立出來的 process image。它裡面包含：

`code`：程式機器指令。
`data`：全域變數、靜態資料。
`stack and heap`：執行期間需要的堆疊與動態配置空間。

這個 process image 被水平切成一格一格，旁邊標註「分 page」。意思是 OS 不會把 process image 當成一整坨連續資料處理，而是把它切成固定大小的 pages。每一個 page 都可以獨立被載入 physical memory，或暫時留在 disk。

---

### 第三張圖中的 page table 在做什麼？

綠色 process image 左邊畫了一個 **page table for hello process**。這張 page table 可以理解成「hello process 的地址翻譯表」。

CPU 發出的 logical address 會先被拆成：

page number
page offset

page number 會拿去查 page table。page table entry 會告訴系統：這個 logical page 現在對應到哪裡。

在這張圖中，physical memory 還是空的，所以重點不是「page 已經在 RAM 哪裡」，而是：**OS 已經知道 hello process 的各個 virtual pages 對應到 disk 中 process image 的哪些位置。**

所以圖中的藍色註記「logical address」可以這樣理解：process 看到的是自己的邏輯位址，例如 code 的某一頁、data 的某一頁、stack 的某一頁。這些 logical pages 不一定已經在 RAM 裡，但 OS 可以透過 page table / backing store 資訊知道它們在 disk 的哪裡。

---

### 第三張圖真正想表達的概念

這張圖不是在說 CPU 直接去 disk 執行程式。CPU 不能直接執行 disk 裡的 `hello.exe`。CPU 要執行指令，指令必須在 physical memory 中。

這張圖是在說：

`hello.exe` 先在 disk 中。
OS / loader 根據 executable header 建立 process image。
Process image 被切成 pages。
Page table 記錄 logical pages 對應的位置。
之後 CPU 需要某一頁時，若該頁不在 RAM，就由 OS 把它從 disk 載入 physical memory。

這就是 demand paging 的前置概念：**不是整個 process 一次搬進 RAM，而是需要哪一頁才搬哪一頁。**

---

## 第四張圖：更明確區分 user space 與 system space
### 解釋
第二張圖把 disk 分成上下兩塊。
![image](https://hackmd.io/_uploads/BJsJqJGezl.png)

下方標成 **user space**，裡面有原始的 `hello.exe`。這代表使用者看到的檔案，例如存在檔案系統中的 executable file。它是靜態檔案，裡面有 header、code、data、entry point 等資訊。

上方標成 **system space / OS direct access**，意思是這一區由 OS 管理，使用者程式不能直接任意操作。這裡畫了兩個東西：

左邊是 **page table for hello.exe image**。
右邊是一格一格的 **disk blocks storing image**。

也就是說，當 `hello.exe` 被執行時，OS 會建立一個與 hello process 對應的 image / backing store。這個 image 被拆成多個 disk blocks，每個 block 對應 process image 的某些 page。

---

### 第四張圖中的黑線代表什麼？

第二張圖上方有很多黑線，從 page table 指向右邊的 disk blocks。這表示：

**page table entry 不只是可以指向 RAM 裡的 physical frame；在 page 尚未載入 RAM 時，它也可以讓 OS 找到該 page 在 disk 的位置。**

比較精準地說，硬體 MMU 平常只負責查 page table 並轉成 physical address。若 page table entry 顯示該 page 不在 memory，CPU 會產生 page fault。接著 OS 介入，根據該 entry 或相關資料結構，找到這個 page 在 disk 的 block 位置，把它載入 physical memory，再更新 page table。

所以黑線在概念上代表：

logical page
→ page table entry
→ disk block containing that page

這就是 virtual memory 的關鍵：**virtual page 可以暫時不在 RAM，而是在 disk 上等著被需要。**

---

### 第四張圖中的 `hello.exe` header 作用

下方的 `hello.exe` 裡面標了：

`header: code, data segments, entry point, etc. info.`

旁邊手寫註記像是 `segment offset, length`，意思是 header 會記錄每個 segment 在檔案中的位置與大小。

例如：

code segment 從檔案哪個 offset 開始，長度多少。
data segment 從哪個 offset 開始，長度多少。
entry point 是哪個 virtual address。
哪些區段需要讀入，哪些區段只需要建立空間。

OS / loader 會根據這些資訊建立 process image。也就是從原始 executable file，轉換成 process 執行時會看到的 code、data、heap、stack 佈局。

---

## 兩張圖合起來的完整流程

整個流程可以整理成這樣：

`hello.exe` 原本存在 user space 的 disk 檔案系統中。

程式被執行時，OS / loader 讀取 `hello.exe` 的 header，知道 code、data、entry point 等資訊。

OS 建立 hello process 的 virtual address space，也就是 process image。這個 image 包含 code、data、heap、stack。

Process image 被切成 pages。每個 page 可能對應到 disk 上某些 blocks。

OS 建立 page table，記錄 logical page 和實際位置之間的關係。

一開始不一定把全部 pages 載入 physical memory。可能只載入程式一開始會用到的幾頁。

CPU 執行時產生 logical address，MMU 查 page table。若 page 已在 memory，就轉成 physical address；若 page 不在 memory，就發生 page fault。

OS 處理 page fault，從 disk block 把需要的 page 載入 physical memory，更新 page table，然後程式繼續執行。

---

### 核心重點

這兩張圖想建立的直覺是：

**Executable file 是 disk 上的靜態檔案；process image 是 OS 根據 executable 建立出的執行時位址空間；virtual memory 讓這個 process image 可以被切成 pages，並且不必全部同時存在 physical memory。**

所以 Chapter 10 的精神就是：

程式以為自己擁有完整連續的 address space。
實際上 OS 把 address space 切成 pages。
Pages 可以在 physical memory，也可以暫時留在 disk。
MMU 和 page table 負責位址轉換。
Page fault 負責把尚未載入的 page 從 disk 搬進 RAM。

## 第五張圖：Loader 建好 process，但頁面還沒真正載入 RAM
### 解釋
![image](https://hackmd.io/_uploads/S1K65kGlGe.png)
這張圖的狀態是：`hello.exe` 已經被 OS / loader 處理過，準備變成一個可執行的 process。右邊 disk 下半部仍然有原本的 `hello.exe`，這是靜態的 executable file；右邊 disk 上半部則是 OS 管理的 system space，裡面有 `page table for hello.exe image` 和 `disk blocks storing image`。這表示 OS 已經知道 `hello` 這個 process image 被切成哪些 pages，而且每一個 page 對應到 disk 上哪些 blocks。

左邊 physical memory 裡面可以看到底部有 `kernel`，再上面有 `PCB for hello image`，再上面是 `page table for hello's image`。這代表 OS 已經替 hello process 建立好管理資料結構。PCB 裡面會記錄 process 狀態，例如 program counter、register 狀態、排程資訊、記憶體管理資訊等等；page table 則是 MMU 之後做位址轉換時要查的表。

但是，這張圖中 page table 的 entries 大多還是 `0` 或 invalid。這代表 hello process 的那些 virtual pages 雖然在邏輯上存在，但目前還沒有真的被載入 physical memory。也就是說，OS 已經把「這個 process 應該長什麼樣子」建立好了，但沒有急著把整個 process image 搬進 RAM。

這就是 demand paging 的前置狀態：**先建立位址空間與 page table，但實際頁面等到真的被用到才載入。**

---

### 第五張圖中的 valid / invalid 意義

圖中有特別圈出 `Valid`。在 page table entry 裡，valid bit 是非常重要的欄位。

如果某個 page table entry 是 valid，表示該 virtual page 目前已經在 physical memory 中，MMU 可以根據 page table entry 找到對應的 physical frame。

如果某個 page table entry 是 invalid，代表 CPU 現在不能直接存取這一頁。這可能有兩種情況：第一種是這個位址根本不屬於該 process，是非法位址；第二種是這個 page 屬於該 process，但目前還在 disk，尚未被載入 RAM。Demand paging 用的就是第二種情況。

所以這張圖想表達的是：hello process 已經準備好要執行，但它的 code/data pages 很可能還在 disk 上。當 CPU 一開始跳到 entry point 或 main function，要抓第一條指令時，MMU 查 page table 發現該 page invalid，就會觸發 page fault。

---

### 第五張圖的流程整理

這張圖的流程可以整理成：

`hello.exe` 原本在 disk 裡。
OS / loader 讀取 executable header，知道 code segment、data segment、entry point 等資訊。
OS 建立 hello process 的 process image。
OS 建立 PCB 與 page table。
Page table entries 先標成 invalid，表示那些 pages 還沒進 RAM。
OS 記錄每個 virtual page 對應到 disk 的哪個 block。
Scheduler 之後讓 hello process 開始執行。
CPU 一旦真的碰到尚未載入的 page，就會發生 page fault。

這張圖的意義是：**建立 process 不等於把整個程式全部載入 RAM。Virtual memory 允許 OS 先建立邏輯上的 address space，實體記憶體則按需求慢慢補上。**

---

## 第六張圖：第一次 page fault，OS 把 page 0 載入 RAM
### 解釋
![image](https://hackmd.io/_uploads/B1mfj1GlGe.png)
第六張圖開始進入真正執行階段。左邊 MMU 裡面畫了 TLB，旁邊標註 `page fault for page 0`。這代表 CPU 嘗試存取 hello process 的 virtual page 0，但是 page 0 還沒有在 physical memory 裡。

流程大概是這樣：

CPU 產生某個 virtual address。
MMU 先查 TLB，如果 TLB 裡沒有對應結果，就去查 page table。
Page table entry 顯示 page 0 目前 invalid，也就是不在 RAM。
MMU 無法完成位址轉換。
硬體觸發 page fault trap，控制權交給 OS kernel。
OS 檢查這個位址是否合法。
如果合法，代表只是 page 還沒載入。
OS 到 disk 找到 page 0 對應的 disk block。
OS 找一個空的 physical frame。
把 page 0 從 disk 讀進 physical memory。
更新 page table entry，把它改成 valid，並填入 physical frame number。
之後重新執行剛剛失敗的指令。

```tex
disk blocks 裡存著某個 page 的內容
        ↓
OS 讀取這些 disk blocks
        ↓
把內容組成一個 page
        ↓
放進 Main Memory 的某個 page frame
```


圖中 physical memory 上方出現 `page frame for page 0`，代表 page 0 已經被搬進 RAM。左邊 page table 裡也有一格被標成 valid，表示 page 0 現在可以被 MMU 正常轉換了。

---

### 第六張圖中的 disk 與 memory 關係

右邊 disk 上半部仍然畫著 `disk blocks storing image`，旁邊還有綠色的 `image for hello`。綠色框框裡面有 `code`、`data`、`stack and heap`。這表示 hello process 的完整 image 還是可以存在 disk 上，OS 需要哪一頁時，就從對應的 disk block 讀進 RAM。

這張圖的重點不是「整個 image 被搬進 memory」，而是只有 **page 0** 被搬進來。

也就是說，假設 page 0 是程式一開始執行的 code page，那 OS 只需要先把這一頁載入。其他 code page、data page、stack/heap page 可以繼續留在 disk 或尚未配置，等到真的被存取時再處理。

這就是 demand paging 的精神：**按需載入，不提前浪費 RAM。**

---

### 第六張圖的意義

這張圖在說明 page fault 不是程式錯誤本身，而是 virtual memory 的正常機制之一。當 process 第一次碰到某個尚未載入的 page，page fault 會讓 OS 有機會把該 page 從 disk 搬進 memory。

因此 page fault 在 demand paging 中是一個「延遲載入事件」。它的意義是：

程式可以先開始跑，不必整個 executable 全部進 RAM。
OS 可以節省 physical memory。
不常用的 page 不會佔用 RAM。
程式看到的 address space 仍然像是完整連續的。
實際上只有用到的 page 會逐步載入。

第六張圖就是第一次 demand paging 發生時的畫面：**page 0 被需求觸發，從 disk 搬進 physical memory，然後 page table 被更新成 valid。**

---

## 第七張圖：又發生 page fault，page 2 也被載入 RAM
### 解釋
![image](https://hackmd.io/_uploads/ryc1hkGgGl.png)
第七張圖延續第六張圖。這時 physical memory 裡面已經有 `page frame for page 0`，表示 page 0 之前已經載入。接著又出現 `page frame for page 2`，表示 process 執行到後面時，又存取了 virtual page 2。

一開始 page 2 還不在 RAM，所以 MMU 查 page table 時發現 page 2 的 entry 還是 invalid。於是再次觸發 page fault。OS 接手後，流程跟 page 0 類似：

CPU 存取 virtual page 2。
MMU 查不到有效 mapping。
觸發 page fault。
OS 檢查 page 2 是否屬於 hello process 的合法 address space。
確認合法後，OS 找到 page 2 在 disk 上的位置。
OS 找一個空的 physical frame。
把 page 2 從 disk block 讀入 RAM。
更新 page table，把 page 2 的 entry 設為 valid。
讓剛剛失敗的指令重新執行。

因此第七張圖中 physical memory 同時有 page 0 和 page 2。這表示 process 的 virtual pages 是逐步被載入的。

---

### 第七張圖最重要的觀念：virtual pages 不必連續放在 physical memory

第七張圖還有一個非常重要的記憶體管理觀念：hello process 的 virtual address space 看起來是連續的，例如 page 0、page 1、page 2、page 3……但在 physical memory 裡，這些 pages 不需要連續擺放。

Page 0 可以放在某個 physical frame。
Page 2 可以放在另一個 physical frame。
Page 1 甚至可能還沒載入。
Page 3 可能也還在 disk。

CPU 不需要知道這些細節。CPU 只管產生 virtual address，MMU 透過 page table 轉換成 physical address。只要 page table entry 記錄正確，virtual page 和 physical frame 之間可以任意對應。

所以這張圖的意義是：**分頁讓 process 的邏輯位址空間和實體記憶體位置完全解耦。**

---

## 三張圖合起來的完整故事
### 總結
這三張圖可以串成一個完整的 **demand paging 流程**。

一開始，`hello.exe` 存在 disk。OS / loader 根據 header 建立 hello process 的 process image，並建立 PCB 與 page table。這時 process 的 virtual address space 已經存在，但大部分 pages 還沒有真的載入 physical memory。Page table entries 會先標成 invalid，並由 OS 記錄每個 page 在 disk 的位置。

接著 process 開始執行。CPU 想抓 page 0 裡的第一批指令，但 page 0 還不在 RAM，所以 MMU 觸發 page fault。OS 進入 kernel mode，從 disk 找到 page 0，把它載入某個 physical frame，更新 page table，把 page 0 標成 valid，然後讓程式繼續。

程式繼續跑到某個時間點，又需要 page 2。page 2 同樣尚未載入，所以再發生一次 page fault。OS 把 page 2 從 disk 載入另一個 physical frame，再更新 page table。最後 physical memory 中只放著目前用到的 page 0 和 page 2，其他沒用到的 pages 仍然不用佔 RAM。

---

### 這三張圖的核心意義

這三張圖真正要傳達的不是單純「page fault 很慢」而已，而是 virtual memory 的設計哲學：

**Process 看到的是完整的 virtual address space，但 physical memory 只保存目前真的需要的 pages。**

這樣做有幾個重要效果。

第一，節省 RAM。程式可能很大，但實際執行時常常只用到其中一小部分。未使用的 code 或 data 不需要先載入。

第二，加快程式啟動。OS 不需要先把整個 executable 全部讀進 RAM，程式可以更快開始執行。

第三，支援更大的程式。即使 process 的 virtual address space 比實體 RAM 還大，只要 working set 能放進 RAM，程式仍然可以執行。

第四，實體記憶體配置更彈性。Virtual pages 可以對應到任意 physical frames，不需要連續配置。

第五，為 page replacement 鋪路。當 physical memory 滿了，OS 可以把不常用的 page 換出去，再把需要的 page 換進來。

---

### 一句話總結

第五張圖：OS / loader 建立 hello process 的 PCB 和 page table，但 pages 還沒真的載入 RAM。
第六張圖：process 第一次存取 page 0，發生 page fault，OS 把 page 0 從 disk 載入 physical memory，並更新 page table。
第七張圖：process 之後存取 page 2，再次 page fault，OS 把 page 2 載入另一個 physical frame，顯示 pages 是按需求、非連續地載入 RAM。


# 10.1 Virtual Memory
## 為什麼需要 Virtual Memory
建立 virtual memory 的核心動機：**程式執行時，確實需要在 memory 裡；但整個程式通常不會在同一時間全部被用到**。

CPU 不能直接執行 disk 裡的程式碼。程式碼必須先進入 main memory，CPU 才能 fetch instruction、decode、execute。也就是說，從硬體角度來看，程式要跑，至少「正在執行的那一部分」一定要在 RAM 裡。

可是問題在於，一個完整程式通常很大，而且裡面有很多內容不一定會用到。例如 error handling code，只有發生錯誤時才會執行；一些特殊功能 routine，可能整個程式執行期間都不會碰到；大型資料結構也可能只在特定階段才需要。因此，把整個程式一次全部載入 memory，其實很浪費。

所以這頁提出一個關鍵想法：**能不能讓 partially-loaded program 也可以執行**？ 也就是程式不需要全部在 RAM 裡，只要目前會用到的部分在 RAM，程式就能先跑。這就是 virtual memory 和 demand paging 的基本精神。
## Partially-loaded program 的好處
如果系統允許程式「部分載入」就能執行，第一個好處是：**program 不再受到 physical memory 大小的嚴格限制**。

傳統想法中，如果一個程式大小是 2 GB，但 RAM 只有 1 GB，那這個程式看起來不可能完整載入。但 virtual memory 的觀念是：不需要整個 2 GB 都在 RAM，只要目前 working set，也就是正在活躍使用的那些 pages 能放進 RAM，程式就可以執行。

第二個好處是：**每個 program 執行時占用的 memory 變少，所以同時能跑更多 programs**。

如果每個 process 都只載入目前需要的 page，而不是整個程式全部載入，main memory 裡就可以塞進更多 process。這會提升 multiprogramming degree，也就是同時存在於 memory 中、可被 CPU 排程的 process 數量。

當更多 process 可以同時存在時，CPU 比較不容易閒著。某個 process 等 I/O 時，CPU 可以切去跑另一個 process。因此講義寫到：CPU utilization 和 throughput 會增加，而且不一定會增加 response time 或 turnaround time。

第三個好處是：**需要的 I/O 變少**。

如果每次執行程式都要把整個 executable 或整個 process image 從 disk 搬進 RAM，I/O 開銷會很大。Virtual memory 讓 OS 只載入真正需要的部分，所以 load 或 swap process 時的 disk I/O 減少。Disk I/O 通常比 CPU 和 RAM 慢非常多，因此 I/O 減少後，使用者程式實際上會跑得更快。

這一頁可以整理成一句核心：

**Virtual memory 讓程式不必完整載入 RAM 才能執行，因此節省記憶體、降低 I/O、提高同時執行程式數量，進而提升 CPU utilization 和 throughput**。

## Virtual Memory 的正式定義
**正式定義 virtual memory：**
> Virtual memory = separation of user logical memory from physical memory

**意思是：** 使用者程式看到的 logical memory，和實際硬體 RAM 的 physical memory，被分離開來。

程式以為自己擁有一整塊連續、完整、從低位址到高位址的記憶體空間；但實際上，這些位址背後對應到哪些 physical frames，==是由 OS 和 MMU 決定的==。程式不需要知道自己的 page 實際放在 RAM 的哪裡，也不需要知道某些 page 目前是不是還在 disk 上。

這種分離帶來的第一個結果是：
* **Only part of the program needs to be in memory for execution.**
這和第 17 頁的動機完全對應。程式執行時，只需要目前正在使用的 code page、data page、stack page 等在 memory 裡。其他暫時用不到的 page 可以先留在 disk，等 page fault 發生時再載入。
## Virtual Memory 比 Physical Memory 大
![image](https://hackmd.io/_uploads/HJte1ddgGl.png)
這張圖在說明 Chapter 10 最核心的概念：**virtual memory 可以比 physical memory 大**。
左邊是一個 process 看到的 virtual memory，它被切成 `page 0`、`page 1`、`page 2` ……一直到最後的 page。這是 process 的邏輯視角，看起來像一整條完整、連續、很大的記憶體空間。

中間的 **memory map** 可以理解成 **page table** 或 **page map**。它負責記錄：「某個 virtual page 現在到底在哪裡？」有些 virtual pages 目前在 **physical memory** 裡，有些則還在右邊的 disk 裡。

中間偏右的長條是 **physical memory**，也就是實際 RAM。它比左邊的 virtual memory 小很多，代表 RAM 不需要容納整個 process 的所有 virtual pages。只要目前正在用到的 pages 在 RAM 裡，程式就可以繼續執行。

最右邊的圓柱是 **disk**，可以看成 backing store 或 swap space。當某些 virtual pages 暫時不用，或一開始還沒被載入時，它們可以留在 disk 裡。等 CPU 真的需要那些 pages 時，再透過 page fault 把它們載入 physical memory。

**所以這張圖的意思是：**
* **process 看到的是很大的 virtual memory，但實際 RAM 只放目前需要的部分 pages，其餘 pages 可以先放在 disk。**
* 這就是 virtual memory 為什麼能讓 logical address space 大於 physical address space 的原因。程式的「可見空間」可以很大，但不代表整個空間都要同時佔用 RAM。

**流程意義**
* 當 CPU 存取某個 virtual address 時，MMU 會根據 memory map 查詢該 virtual page 的狀態。
* 如果該 page 已經在 physical memory 中，MMU 就能直接把 virtual address 轉成 physical address，然後存取 RAM。
* 如果該 page 不在 physical memory，而是在 disk 上，MMU 無法直接完成轉換，就會觸發 **page fault**。接著 **OS 會介入**，==**把該 page 從 disk 讀入 RAM 的某個 page frame**==，更新 memory map，然後讓程式重新執行剛剛失敗的指令。

因此第 31 頁的重點不是「所有 pages 都在 RAM」，而是：
* **virtual memory 的 pages 可以分散存在 RAM 與 disk；memory map 負責記錄每個 virtual page 的目前位置。**


## Process 的 Virtual Address Space 如何映射到 Physical Memory
![image](https://hackmd.io/_uploads/rJYXzddxMg.png)
第 32 頁把 process 的 virtual address space 畫得更具體。左邊是一個 process 的邏輯記憶體布局，包含：

`Code Segment`：程式碼區，放機器指令。
`Global Data`：全域變數與已初始化資料。
`bss`：未初始化的全域變數或靜態變數。
`Stack`：函式呼叫、區域變數、return address 等。
`Heap`：動態配置記憶體，例如 `malloc()`、`new`。
`User Area`：使用者行程相關管理資訊。
`Page Table Data`：和頁表相關的資料。

左邊每一小格代表 process 的某一個 virtual page。雖然圖上寫 page frame，但依照這張圖的意思，更適合把左邊理解成 process 的 **virtual pages**，也就是 process 位址空間被切成一頁一頁後的結果。

右邊是 **Physical Memory**，被切成 page frames，旁邊標了 page number 1 到 15。這代表 RAM 裡面的固定大小空格。

中間的連線表示：**virtual pages 不需要連續放在 physical memory 裡**。

例如，process 的某個 code page 可能對應到 physical memory 的 page frame 1；另一個 data page 可能對應到 page frame 7；stack page 可能對應到 page frame 10。它們在 process 眼中是連續或有結構的，但在 RAM 裡可以散落在不同位置。

---
**第 32 頁的 Stack / Heap 重點**

圖中 Stack 和 Heap 中間有虛線與箭頭。這是在表示常見的 process address space 結構：
* **Heap 通常往高位址方向成長。**
* **Stack 通常往低位址方向成長。**

兩者中間可能有一大段尚未使用的 virtual address space。這段空間在 process 看起來存在，但不一定真的有 physical memory 對應。只有當 heap 擴張，或 stack 成長到需要新的 page 時，OS 才會配置或載入對應的 page frame。

這也是 virtual memory 的重要優點：address space 可以先保留很大範圍，但 physical memory 可以等到真的需要時才分配。

所以第 32 頁強調的是：
**process 的 virtual address space 有清楚的邏輯結構，但實體記憶體中的配置可以是分散、非連續、按需求存在的。**

## Page Table 同時記錄 RAM 與 Disk 的位置
![image](https://hackmd.io/_uploads/Sy8-7_Oxzx.png)
第 33 頁是三張裡面最關鍵的一張，因為它直接畫出 page table entry 的角色。

左邊是 **memory-resident page table (DRAM)**，意思是 page table 本身放在 RAM 裡。每一列是一個 page table entry，對應到某個 virtual page，例如 `PTE 0` 到 `PTE 7`。

Page table entry 裡有兩個重要欄位：

**第一個是 Valid bit。**
如果 valid bit 是 `1`，代表該 virtual page 目前在 physical memory 中，可以直接轉換成 physical address。

如果 valid bit 是 `0`，代表該 virtual page 目前不在 physical memory。這可能表示該位址非法，也可能表示該 page 在 disk 上，尚未載入 RAM。

第二個欄位是 **Physical page number or disk address**。
這表示 entry 裡可能記錄 physical page frame number，也可能記錄 disk address。

如果 valid = 1，entry 裡放的是 physical page number。
如果 valid = 0，但該 page 是合法的，只是目前不在 RAM，那 OS 可以透過 entry 或其他輔助資料找到它在 disk 上的位置。

## Physical Memory 裡目前有哪些 pages
右上方是 Physical memory (DRAM)。裡面目前放了：
`VP 1
VP 2
VP 7
VP 4`
這表示 virtual page 1、2、7、4 目前已經被載入 RAM。旁邊標的 `PP 0`、`PP 3` 則是 physical page number，也就是實體 page frame 編號。

注意這些 virtual pages 在 RAM 裡不一定照順序放。`VP 1`、`VP 2`、`VP 7`、`VP 4` 可以被放在任意 physical frames。Page table 會負責記住每個 virtual page 對應到哪個 physical frame。

所以第 33 頁再次強調：
* **virtual page number 和 physical page number 不需要一樣，也不需要連續。**

## Virtual Memory on Disk
右下方是 **Virtual memory (disk)**。它畫出 disk 上保存的 virtual pages，例如：

`VP 1、VP 2、VP 3、VP 4、VP 6、VP 7`

這代表某些 pages 的 backing store 在 disk 上。即使某些 pages 目前在 RAM，它們的原始內容或備份來源也可能在 disk 裡，例如 executable file、memory-mapped file 或 swap space。

圖中的虛線通常表示：某些 page table entries 指向 disk 上的 pages。這些 pages 目前不在 RAM，所以 valid bit 是 0；但它們不是非法位址，而是「合法，但尚未載入」。

當 CPU 存取這類 page 時，就會發生 page fault。OS 接著根據 disk address 找到該 page，把它讀入 RAM，更新 page table，把 valid bit 改成 1。
## 第 33 頁的完整流程
以某個目前不在 RAM 的 virtual page 為例，流程是：

1. CPU 產生 virtual address。
2. MMU 用 virtual page number 查 page table。
3. Page table entry 的 valid bit 是 0。
4. MMU 觸發 page fault。
5. OS 檢查該 page 是否屬於合法 address space。
6. 如果合法，OS 找到該 page 在 disk 上的位置。
7. OS 找一個空的 physical page frame。
8. OS 把 disk 上的 page 讀入 RAM。
9. OS 更新 page table entry，填入 physical page number，並把 valid bit 設成 1。
10. 程式重新執行剛剛失敗的 memory access。

所以這幾張圖最重要的意思是：
1. **page table 不只是做 virtual page 到 physical frame 的轉換，也用 valid bit 表示 page 是否目前在 RAM；若不在 RAM，OS 會透過 disk address 把它載入。**
2. **Virtual memory 讓 process 擁有比 physical memory 更大、看似連續的 address space；實際上只有部分 virtual pages 存在 RAM，其餘 pages 可以在 disk，並由 page table 的 valid bit 與位址欄位記錄目前位置。**
## Virtual-address Space
### 圖片解析
![image](https://hackmd.io/_uploads/HktOPvtlGx.png)
第 38 頁在說明 process 的 virtual address space 長什麼樣子，以及為什麼這種設計很適合 virtual memory。
可以先看成一個 process 眼中的完整記憶體空間。最下面是 address 0，最上面是最大 logical address。從低位址到高位址，通常會放：
* `code`：程式機器碼，也就是 text segment。
* `data`：全域變數、靜態變數等資料。
* `heap`：動態配置記憶體，例如 `malloc()`、`new`。
* 中間藍色區域：尚未使用的 address space hole。
* `stack`：函式呼叫、區域變數、return address、參數等。

這張圖最核心的地方是：**heap 往上長**，**stack 往下長**。
* Heap 從低位址附近開始，隨著程式動態配置記憶體而往高位址成長。Stack 則從高位址開始，隨著函式呼叫加深、區域變數增加而往低位址成長。兩者中間留下一大塊空洞，這塊空洞在 virtual address space 裡存在，但一開始不需要真的佔用 physical memory。
* 例如某個程式需要大量 `malloc()`，heap 就可以往上長很多。另一個程式有很深的 recursive function call，stack 就可以往下長比較多。這種配置讓 OS 不必一開始就猜 heap 和 stack 各自會需要多大。

更重要的是：**中間那塊 unused address space 是 hole，不需要 physical memory**。
* 也就是說，virtual address space 裡雖然看起來預留了一大片範圍，但只要 heap 或 stack 還沒長到那裡，OS 就不需要分配 physical page frames。等到 heap 或 stack 真的成長到某個新 page，才會透過 page fault 或記憶體配置機制建立對應的 physical frame。

這就是 virtual memory 的精髓：**位址空間可以先保留，實體記憶體可以晚點再給**。
### Sparse Address Space：稀疏位址空間
投影片寫到：
* **Enables sparse address spaces with holes left for growth, dynamically linked libraries, etc.**
這裡的 sparse address space 指的是：process 的 virtual address space 可以有很多空洞，不一定每個 virtual address 都真的對應到 physical memory。

**例如：**
* code 區有實際內容。
* data 區有實際內容。
* heap 目前只用到一小段。
* stack 目前只用到一小段。
* heap 和 stack 中間是一大片空洞。
* 中間某些位置可能映射 shared library。
* 某些區域可能預留給 memory-mapped file。
這種稀疏設計非常重要，因為現代 process 的 virtual address space 通常很大，但真正用到的只是一部分。OS 不需要替每一個 virtual page 都分配 RAM，只需要替真的被使用的 pages 建立 mapping。所以這頁的意思不是「process 真的吃掉整個 address space」。
而是：**process 擁有一個很大的 logical address space，但其中很多地方只是保留區，尚未真正配置 physical memory**。
### System Libraries Shared via Virtual Address Space
投影片下一點寫：

**System libraries shared via mapping into virtual address space**

這是在說 shared library，例如 Linux 裡常見的 `libc.so`。

很多 process 都會用到同一份系統函式庫。如果每個 process 都把整份 library 複製一份到 RAM，會非常浪費。因此 OS 會讓多個 process 的 virtual address space 中，都有一段區域映射到同一份 shared library 的 physical pages。

從每個 process 的角度來看，shared library 好像存在於自己的 virtual address space 裡。但實際上，背後可能是同一組 physical page frames。

這樣可以節省大量 RAM。尤其是 `libc`、GUI library、runtime library 這些幾乎每個 process 都會用到的東西，共享效果非常明顯。
### Shared Memory by Mapping Pages Read-Write
投影片也寫：

**Shared memory by mapping pages read-write into virtual address space**

這是在說 shared memory。兩個 process 平常有各自獨立的 virtual address space，理論上不能直接碰彼此的 memory。但 OS 可以刻意安排：讓兩個 process 的某些 virtual pages 映射到同一批 physical frames。

如果這些 shared pages 被設定成 read-write，那兩個 process 都能讀寫同一份資料。這就形成 shared memory IPC。

概念上像這樣：
```tex=
Process A virtual page X ─┐
                          ├── same physical frame
Process B virtual page Y ─┘
```
Process A 寫入這個 physical frame 後，Process B 讀同一個 frame，就能看到變化。這種 IPC 速度很快，因為資料不用透過 kernel 反覆複製。但缺點是需要同步機制，例如 mutex、semaphore，否則可能發生 race condition。
### Pages Can Be Shared During fork()
最後一點：

**Pages can be shared during `fork()`, speeding process creation**

這是在講 `fork()` 的最佳化，也就是後面會正式講的 **copy-on-write**, COW。

當一個 process 呼叫 `fork()` 建立 child process 時，直覺上好像要複製整份 address space。但如果 parent process 很大，完整複製會非常慢，也很浪費 RAM。

所以現代 OS 通常不會立刻複製所有 pages，而是先讓 parent 和 child 共享相同的 physical pages，並把這些 pages 設成 read-only。

只要 parent 和 child 都只是讀取，那就繼續共享。
如果其中一方嘗試寫入某一頁，才觸發 page fault。
OS 接著複製那一頁，讓寫入者擁有自己的 copy。

這樣 `fork()` 會快很多，因為剛建立 child process 時，不需要真的複製所有記憶體。

所以第 38 頁最後幾點都在強調同一個主題：

**virtual address space 不只是讓記憶體變大，也讓共享、延遲配置、快速建立 process 變得可行。**
## Shared Library Using Virtual Memory
![image](https://hackmd.io/_uploads/BJKmfYKxGx.png)

# 10.2 Demand Paging
## Demand Paging 的核心概念
![image](https://hackmd.io/_uploads/HyofAYtgMl.png)
這頁在正式定義 **Demand Paging（需求分頁）**。重點是：系統可以選擇在程式啟動時，把整個 process 全部載入 main memory；也可以選擇只在某個 page 真的被需要時，才把那一頁載入 memory。

傳統做法比較像「整個 process 先搬進 RAM 再執行」。Demand paging 則是「先讓 process 開始跑，缺哪一頁再補哪一頁」。這樣可以減少不必要的 I/O，因為很多程式碼或資料可能整個執行期間都不會用到。例如錯誤處理程式碼、冷門功能、大型但很少碰到的資料區，都不需要一開始就讀進 RAM。

這頁列出的好處可以整理成四個：**第一，Less I/O needed**，因為只讀需要的 page；**第二，Less memory needed**，因為 RAM 只放正在用的 page；**第三，Faster response**，因為 process 不用等整個程式載入完成才開始執行；**第四，More users**，因為每個 process 佔用的 RAM 變少，所以系統可以同時容納更多 process。

右邊圖表是在把 demand paging 類比成 paging system with swapping。Main memory 中只放部分 program A、program B 的 pages；disk 上則保留完整或部分的 backing store。需要某頁時，該 page 會 **swap in** 到 main memory；某些 page 不需要或 memory 壓力大時，可能會 **swap out** 回 disk。

這頁最後提到 Lazy swapper。Lazy swapper 的意思是：除非確定某個 page 將會被需要，否則不會把它換入 memory。不過嚴格來說，處理「page」的元件通常稱為 pager，因為 swapper 傳統上比較偏向處理整個 process，而 pager 是以 page 為單位處理。

## Demand Paging 的基本流程
這頁在說 Demand Paging 執行時會遇到兩種情況。

第一種情況是：需要的 page 已經在 memory 裡，也就是 **memory resident**。這時候行為和一般 paging 幾乎沒有差別。CPU 產生 virtual address，MMU 查 page table，找到對應 physical frame，直接完成存取。

第二種情況是：需要的 page 不在 memory 裡。這時候系統必須偵測到這件事，並且把該 page 從 storage 載入 memory。這個過程必須對程式透明，也就是程式本身不需要改寫，不需要手動呼叫「把 page 載入」之類的函式。程式仍然照常存取位址；page fault 由硬體與 OS 合作處理。

這頁的關鍵句是：

**Need new MMU functionality to implement demand paging**

意思是 Demand Paging 需要硬體支援。MMU 必須能夠分辨某個 page table entry 目前是否有效。如果該 page 不在 memory，MMU 要能觸發 page fault，讓 OS 接手處理。沒有這個硬體機制，OS 很難在每次 memory access 時有效率地判斷 page 是否在 RAM。


## Valid-Invalid Bit
![image](https://hackmd.io/_uploads/HyWgUEoefe.png)
這頁介紹 Demand Paging 裡最重要的 page table 欄位：**valid-invalid bit**。

每個 page table entry 都會搭配一個 valid-invalid bit。投影片用：
* v 代表 valid，表示該 page 目前在 memory 中，也就是 **memory resident**。
* i 代表 invalid，表示該 page 目前不在 memory 中。
在 Demand Paging 剛開始時，很多 page table entries 會被設成 i，因為 process 的大部分 pages 還沒有被載入 RAM。等到某個 page 第一次被存取，發生 page fault，OS 把它從 disk 載入 RAM 後，才會把該 entry 改成 `v`，並填入對應的 frame number。

圖中的 page table snapshot 表示某些 entries 是 `v`，某些是 `i`。`v` 的 entry 可以正常完成位址轉換；`i` 的 entry 會在被存取時造成 page fault。

這裡有一個細節很重要：**invalid 不一定代表非法位址**。
它可能代表兩種情況：
1. 一種是該 page 根本不屬於 process 的合法 address space，這是真正非法存取，OS 會終止 process。
2. 另一種是該 page 屬於 process，但目前還不在 memory，這是 demand paging 的正常情況，OS 會把 page 載入 RAM。
所以 valid-invalid bit 只告訴硬體「目前能不能直接轉換」。至於 invalid 到底是非法還是尚未載入，需要 OS 進一步判斷。

## 部分 Pages 不在 Main Memory 時的 Page Table
![image](https://hackmd.io/_uploads/SJMtUVolzl.png)
這頁用一個完整例子說明 valid-invalid bit 如何運作。

左邊是 logical memory，裡面有 8 個 pages：
> A、B、C、D、E、F、G、H

中間是 page table。每個 logical page 都有一個 page table entry。Entry 裡面有兩個欄位：
* **frame：** 如果 page 在 RAM，這裡記錄它對應到哪個 physical frame。
* **valid-invalid bit：** 標記該 page 目前是否在 main memory。
右邊中間是 physical memory，可以看到目前只有三個 pages 被載入 RAM：
```tex
A 在 frame 4
C 在 frame 6
F 在 frame 9
```
所以 page table 裡面會是：
```tex
Page A：frame 4，valid
Page B：invalid
Page C：frame 6，valid
Page D：invalid
Page E：invalid
Page F：frame 9，valid
Page G：invalid
Page H：invalid
```
最右邊的 disk 則表示 backing store，也就是尚未載入或可重新載入的 page 內容所在位置。B、D、E、G、H 目前不在 physical memory，但仍然存在 disk 上。當 CPU 存取 B、D、E、G、H 其中任何一頁時，就會發生 **page fault**。
**例如 CPU 存取 logical page B：**
1. MMU 查 page table，發現 B 的 valid-invalid bit 是 `i`。
2. 硬體觸發 page fault。
3. OS 檢查 B 是否是合法 page。
4. OS 找一個空的 physical frame。
5. 把 B 載入 RAM。
6. 更新 page table：填入 frame number，並把 bit 改成 `v`。
7. 重新執行剛剛造成 page fault 的指令。
**這張圖的重點是：** logical memory 看起來有 A 到 H，但 physical memory 只需要放目前用到的 A、C、F。其他 pages 可以先待在 disk。

## Steps in Handling a Page Fault
### 圖中角色
![image](https://hackmd.io/_uploads/H13FoNsgGe.png)
左邊的 load `M` 代表某條機器指令正在存取記憶體位址 `M`。例如 CPU 想讀取某個變數，或抓某條 instruction。

中間的 **page table** 是目前 process 的頁表。CPU 產生 virtual address 後，MMU 會用 virtual page number 查 page table，看該 page 是否在 memory。

右下角的 **physical memory** 是 RAM。裡面有很多 page frames，其中某一格是 free frame，準備拿來放缺少的 page。

右邊的大圓柱是 **backing store**，也就是 disk 上保存 page 內容的地方。這可能是 executable file、memory-mapped file，或 swap space。

上方的 **operating system** 代表 page fault handler，也就是 OS kernel 裡專門處理 page fault 的程式。

### Step 1：CPU reference 某個 address
圖上的第 1 步是 `reference`。
CPU 執行 `load M`，想存取 virtual address `M`。這個 virtual address 會被拆成：
> virtual page number + offset

MMU 會先用 virtual page number 去查快取，也就是 ==**TLB**==。

**TLB 在這裡的作用**
* TLB，全稱是 **Translation Lookaside Buffer**，可以理解成「page table 的高速快取」。
* 它不是存放真正資料，而是快取最近用過的位址轉換結果：
    * virtual page number → physical frame number
* 所以正常情況下，CPU 存取記憶體時會先走：
    1. CPU 產生 virtual address
    2. → MMU 查 TLB
    3. → 如果 TLB hit，直接得到 physical frame number
    4. → 加上 offset，形成 physical address
* 這樣就不用每次都去 main memory 查 page table，速度快很多。

**TLB miss 不等於 page fault**
這裡很容易搞混，要分開：
* **TLB miss：** TLB 裡沒有這個 virtual page 的快取紀錄。
* **Page fault：** page table 顯示該 page 目前不在 main memory，或該 reference 不合法。
* **所以流程是：**
    1. CPU 存取 virtual address
    2. → MMU 查 TLB
    3. → TLB miss
    4. → MMU 查 page table
    5. → page table entry 是 ==invalid==
    6. → page fault
也就是說，**TLB miss 只是找不到快取**；**page fault 是查到 page table 後發現不能直接存取**。
如果 TLB miss 但 page table entry 是 valid，那只要把轉換結果補進 TLB，然後繼續執行即可，不需要進 OS 做 page fault 處理。

### Step 2：Trap 到 Operating System
圖上的第 2 步是 `trap`。

當 MMU 查 page table 後發現該 entry 是 invalid，硬體會觸發 **page fault trap**。CPU 會從 user mode 切到 kernel mode，控制權交給作業系統的 page fault handler。

這時候 OS 會保存目前 process 的狀態，例如 program counter、registers、造成 fault 的 address 等。這也對應紅字寫的「有 register 不執行完的值」：因為那條 `load M` 還沒成功執行完，必須保存現場，之後才能重新執行。
### Step 3：OS 判斷是非法存取，還是 page 不在 memory
第 44 頁第 1 點寫：
> Operating system looks at another table to decide:
> - Invalid reference → abort
> - Just not in memory

這裡意思是：page table entry 是 invalid，只能告訴硬體「現在不能直接轉換」。但 OS 還要進一步判斷原因。
**可能有兩種情況。**
1. 第一種是 **invalid reference**。例如 process 存取完全不屬於自己的 address space 的位址，或違反權限。這是真正的非法記憶體存取，OS 會終止 process，常見結果就是 segmentation fault。
2. 第二種是 **just not in memory**。該 page 屬於合法 address space，只是目前還在 disk，尚未載入 RAM。這就是 demand paging 的正常情況。
**所以 page fault 不一定是錯誤。它可能只是 OS 延遲載入 page 的正常機制。**
### Step 4：Find free frame
如果 OS 判斷這個 page 是合法的，只是目前不在 memory，接下來就要在 physical memory 裡找一個空的 page frame。

圖中 physical memory 裡標了 free frame，意思是 OS 找到一格可以放缺失 page 的 RAM 空間。

如果有 free frame，直接使用。

如果沒有 free frame，就必須進入後面會講的 **page replacement**：==挑一個目前在 RAM 裡的 victim page，把它換出去，空出 frame 給新的 page==。

### Step 5：從 backing store 把 missing page 載入 RAM
OS 會根據 page table 或其他輔助資料結構，找到該 virtual page 在 backing store 上的位置。接著發出 disk I/O，把缺少的 page 從 disk 讀進剛剛找到的 free frame。

這一步通常很慢，因為 disk I/O 比 CPU 和 RAM 慢非常多。因此處理 page fault 時，process 可能會被 block，CPU 先切去執行其他 ready process。等 disk I/O 完成後，disk interrupt 通知 OS，OS 再回來完成後續處理。
### Step 6：Reset page table，設 valid bit
第 45 頁第 5 步是：

reset page table

這對應第 44 頁第 4 點：

Reset tables to indicate page now in memory
Set validation bit = v

Page 被載入 physical memory 後，OS 必須更新 page table entry。

原本該 entry 可能是：

invalid，沒有可用 frame number

更新後變成：

valid，frame number = 剛剛載入的 physical frame

也就是把 valid-invalid bit 設成 v，並填入該 page 現在所在的 physical frame number。
### TLB 在 Step 6 之後怎麼辦？
Page table 更新後，TLB 也要注意。

因為 TLB 是 page table 的快取，如果 TLB 裡有舊的、錯誤的 entry，必須避免它繼續被使用。

常見做法有幾種：

一種是 OS 在 page fault handler 結束時，把這個 virtual page 對應的舊 TLB entry 清掉，稱為 **TLB invalidation**。
```tex
另一種是硬體或 OS 在重新執行指令時，讓 MMU 再查一次 page table。這時 page table 已經是 valid，所以新的 mapping 會被載入 TLB。

所以 page fault 處理完後，下一次存取同一個 page 時，理想流程會變成：
```
TLB 的重點是加速後續轉換，不是負責從 disk 載入 page。真正處理 page fault 的是 OS。
### Step 7：Restart faulting instruction
剛剛失敗的是 load M。第一次執行時，因為 M 所在的 page 不在 memory，所以中途 page fault，指令沒有真正完成。

等 OS 把 page 載入、page table 更新、TLB 狀態處理好後，CPU 會重新執行這條 load M。

這一次 MMU 再查位址時，就可以找到 valid mapping，於是成功把 virtual address 轉成 physical address，完成 memory load。

**整體流程可以整理成：**
```tex
1. CPU 執行 load M，產生 virtual address
2. MMU 先查 TLB
3. TLB miss，於是查 page table
4. Page table entry 是 invalid
5. 硬體觸發 page fault trap，進入 OS
6. OS 判斷該 reference 是非法，還是只是 page 不在 memory
7. 若非法，abort process
8. 若合法但不在 memory，OS 找 free frame
9. OS 從 backing store 把 missing page 讀入 free frame
10. OS 更新 page table，把 valid bit 設成 v，填入 frame number
11. OS 清除或更新相關 TLB entry
12. 重新執行剛剛造成 page fault 的 load M
13. 這次位址轉換成功，程式繼續執行
```
## Aspects of Demand Paging
### 1. 極端情況：process 一開始完全沒有 page 在 memory

投影片第一點寫：

**Extreme case — start process with no pages in memory**

意思是最極端的 demand paging 做法，是 process 剛開始執行時，完全不先載入任何 page 到 main memory。OS 只建立 process 的位址空間、page table、PCB 等管理資料，然後把 instruction pointer 設到 process 的第一條指令。

可是第一條指令所在的 page 其實還不在 RAM。於是 CPU 一開始 fetch instruction，就會發生 page fault。OS 再把第一個需要的 page 載入 memory。

這種做法稱為：

**Pure demand paging（純需求分頁）**

它的精神是：**完全不預載 page，真的碰到才載入。**

也就是：

程式開始執行
→ 第一條 instruction 不在 memory
→ page fault
→ OS 載入該 page
→ 重新執行
→ 之後每個尚未存取過的 page 第一次被碰到時，也會 page fault

這樣做可以最小化一開始的載入量，但缺點是剛開始可能會連續發生不少 page faults。
### 2. 一條指令可能造成多個 page faults
投影片第二點寫：

**Actually, a given instruction could access multiple pages → multiple page faults**

這是很重要但容易忽略的細節。一般直覺會以為「一條指令最多只造成一次 page fault」，但其實不一定。
例如一條指令可能做這件事：

從 memory 取第一個 operand
從 memory 取第二個 operand
把結果寫回 memory

如果這三個 memory locations 分別位於不同 pages，而且那些 pages 都還不在 memory，那同一條指令在完成之前，就可能陸續造成多次 page fault。

**舉例：**
> ADD [A], [B] -> [C]

假設 A 所在 page 不在 RAM，第一次 page fault。
A 載入後重新執行，接著發現 B 所在 page 也不在 RAM，第二次 page fault。
B 載入後再重新執行，最後要寫 C，結果 C 所在 page 也不在 RAM，第三次 page fault。

所以同一條高階意義上的「加法指令」，可能牽涉多個 page。

### 3. Locality of reference 讓問題沒那麼糟
投影片寫：

**Pain decreased because of locality of reference**

意思是雖然 page fault 很昂貴，而且一條指令理論上可能造成多個 page faults，但實際情況通常沒有那麼慘，原因是 **locality of reference（參考局部性）**。

程式執行時通常具有兩種局部性：
* **Temporal locality（時間局部性）**：剛剛用過的資料，短時間內很可能再用到。
* **Spatial locality（空間局部性）**：剛剛用到某個位址，附近位址也很可能很快被用到。

因此，當某個 page 被載入後，接下來通常會連續使用同一頁或附近幾頁，不會每條指令都瘋狂 page fault。
所以 demand paging 能實用，背後很大程度是靠 locality 撐起來的。沒有 locality 的程式會造成大量 page fault，效能會非常差，後面會引出 **thrashing** 的概念。

### 4. Demand paging 需要的硬體支援
投影片最後列出三個需求：

1. **Page table with valid / invalid bit**
    需要 page table entry 能標記某頁目前是否在 memory。valid 表示在 RAM，invalid 表示目前不在 RAM 或非法。
2. **Secondary memory with swap space**
    需要 disk 上有地方可以保存不在 RAM 的 pages。這個地方可能是 executable file、memory-mapped file，或 swap space。
3. **Instruction restart**
    發生 page fault 時，造成 fault 的 instruction 還沒完成。OS 把 page 載入後，CPU 必須能重新執行那條 instruction，而且結果要像中間沒有失敗過一樣。

這個 instruction restart 是第 47 頁要進一步說明的重點。
## Instruction Restart
### 1. 為什麼需要 restart instruction？
當 CPU 執行某條指令時，如果中途發現需要的 page 不在 RAM，就會 page fault。這時那條指令沒有正常完成，OS 會把缺的 page 載入 memory，然後讓 CPU 回去重新執行剛剛那條指令。

理想情況下很簡單：

`load M`

第一次執行時 M 所在 page 不在 memory。
page fault 發生。
OS 載入 page。
重新執行 `load M`。
這次成功。

但有些指令比較複雜，一條指令可能會讀寫多個 memory locations，甚至會自動改變 register 或 memory 狀態，因此 restart 就會變麻煩。
### 2. Block move 指令的問題
投影片舉例：

**block move**

Block move 是把一段 memory 從 source 複製到 destination。這種指令可能一次搬很多 bytes，因此會跨越多個 pages。

問題是：假設搬到一半時發生 page fault，該怎麼 restart？

如果整條指令從頭重跑，可能會把前面已經搬過的資料再搬一次。一般情況也許還好，但如果 source 和 destination overlap，就可能出事。

投影片問：

**What if source and destination overlap?**

意思是如果來源區域和目的區域有重疊，搬移順序會影響結果。這就像 `memmove()` 和 `memcpy()` 的差別：重疊時必須小心方向，否則資料可能被覆蓋。

所以對硬體和 OS 來說，restart instruction 必須確保：

page fault 發生前已經做過的部分，不會造成錯誤結果。
重新執行後，整體效果必須等同於指令一次完整執行成功。
如果指令有副作用，CPU 必須能恢復到可安全重啟的狀態。
### 3. Auto increment / decrement location 的問題

投影片也提到：

**auto increment/decrement location**

有些 CPU 指令在存取 memory 的同時，會自動增加或減少某個 register。例如讀取某個位址後，address register 自動加 1 或加上一個資料大小。

如果 page fault 發生在指令中途，register 已經被改過，但 memory access 沒完成，那重新執行時就可能用錯位址。

因此硬體必須提供足夠機制，讓 OS 可以正確 restart faulting instruction。這通常需要 CPU 在 trap 發生時保存正確的 program counter 和相關狀態，或者讓指令設計成容易重啟。
## Performance of Demand Paging
### 1. Trap to operating system

第一步是硬體偵測到 page fault，CPU trap 進入 OS kernel。這代表從 user mode 切到 kernel mode。

這本身就有成本，因為 CPU 要保存部分狀態、切換控制權、跳到 page fault handler。

### 2. Save user registers and process state

進入 kernel 後，OS 要保存目前 process 的 registers 和 process state。因為該 process 之後會被暫停，等 page 載入完成後再繼續。

這一步確保未來能回到 page fault 發生前的狀態，並重新執行 faulting instruction。

### 3. Determine that the interrupt was a page fault

OS 要判斷這次 trap / interrupt 的原因是不是 page fault。因為進入 kernel 的事件很多，可能是 system call、timer interrupt、I/O interrupt、protection fault 等。

確認是 page fault 後，才進入 page fault handler 的邏輯。

### 4. Check page reference legality and find page on disk

OS 檢查 faulting address 是否合法。

如果是非法位址，例如存取不屬於該 process 的 address space，就終止 process。
如果是合法位址，只是 page 不在 RAM，就找到該 page 在 disk 上的位置。

這個位置可能在 executable file、memory-mapped file 或 swap space。

### 5. Issue a disk read to a free frame

OS 找到 free frame 後，發出 disk read，把缺的 page 讀入該 frame。

這裡又分成三個等待：

第一，等待 I/O request 在 device queue 裡輪到。
第二，等待 disk seek / latency。傳統硬碟要等磁頭移動、磁碟轉到正確位置。
第三，真正開始把 page 傳輸到 free frame。

這通常是 page fault 成本最大的一段，因為 disk I/O 比 memory access 慢非常多。

### 6. While waiting, allocate CPU to another user

因為 disk I/O 很慢，OS 不會讓 CPU 空等。造成 page fault 的 process 會被 block，CPU 會被分配給其他 ready process。

這裡老師手寫標了 context switch，因為 CPU 會切去執行另一個 process。

這也是為什麼 page fault 會牽涉排程器：page fault 不只是記憶體事件，也可能造成 process 狀態從 running 變成 waiting。

### 7. Receive interrupt from disk I/O subsystem

當 disk I/O 完成後，disk controller 會發出 interrupt 通知 OS：

缺的 page 已經讀入 physical memory。

這時 CPU 可能正在跑另一個 process，所以 OS 會再次進入 kernel 處理這個 I/O completion interrupt。

### 8. Save registers and process state for the other user

因為 CPU 當時可能在跑另一個 process，所以 OS 要先保存「目前正在跑的那個 process」的狀態。

這一步再次產生 context switch 相關成本。

### 9. Determine interrupt was from disk

OS 判斷這次 interrupt 是 disk I/O 完成，不是其他事件。

確認後，OS 才知道某個 page fault 等待的 disk read 已完成。

### 10. Correct page table and other tables

OS 更新 page table：

把該 virtual page 對應到剛剛讀入的 physical frame。
把 valid-invalid bit 改成 valid。
必要時更新其他 kernel tables。
相關 TLB entry 可能需要 invalidation 或讓之後重新填入。

這一步完成後，該 page 已經正式 memory resident。

### 11. Wait for CPU to be allocated to this process again

此時 page fault 的 process 已經可以繼續執行，但不代表它馬上拿回 CPU。

它通常會被放回 ready queue，等待 scheduler 再次 dispatch。

老師手寫的「回 ready queue 等著被 dispatch」就是這個意思。

### 12. Restore state and resume interrupted instruction

當 scheduler 之後選到這個 process，OS 恢復它的 registers、process state、page table context 等資訊，然後重新執行造成 page fault 的那條 instruction。

這次 page 已經在 memory，所以指令可以成功完成。
## demand paging effective access time
### Effective Access Time 公式
這頁正式給出 **EAT**，**Effective Access Time（有效存取時間）** 的公式。
`p = page fault rate`
也就是每次 memory reference 發生 page fault 的機率。範圍是：
`0 ≤ p ≤ 1`
投影片公式是：
```tex
EAT = (1 - p) × memory access + p × (page fault overhead + swap page out + swap page in)
```
### 為什麼有 swap page out？
這裡要特別注意：不是每次 page fault 都一定要 swap page out。

如果 physical memory 還有 free frame，只需要把 missing page swap in 進來即可。

如果 physical memory 沒有 free frame，就要找 victim page。若 victim page 是 dirty，也就是被修改過，OS 必須先把 victim page 寫回 disk，這就是 swap page out。接著才能把 missing page 讀進來，也就是 **swap page in**。

所以完整 worst case 是：
```tex
page fault overhead
+ swap out dirty victim page
+ swap in missing page
```
### Demand Paging 的三個主要成本
投影片列出三個主要活動：

第一，**service the interrupt**。
Page fault 發生後，CPU trap 進 OS，OS 執行 page fault handler。這部分通常只需要幾百條指令，相對 disk I/O 來說很小。

第二，**read the page**。
這是最花時間的部分。因為缺的 page 要從 disk / backing store 讀進 RAM。Disk I/O 通常是毫秒等級，而 RAM 是奈秒等級，差距非常大。

第三，**restart the process**。
Page 載入後，OS 更新 page table，恢復 process 狀態，重新執行剛剛造成 page fault 的 instruction。這部分也比 disk I/O 小很多。

所以考試上看到 page fault service time，通常最大來源就是「**讀 disk**」。

### Demand Paging 計算例題
![image](https://hackmd.io/_uploads/Hyl3Cw4ZGg.png)

## Demand Paging Optimizations
### 目的
這頁在講 Demand Paging 的效能最佳化。前面已經知道 page fault 很貴，尤其是從 disk 讀 page 進 RAM 很慢，所以這頁的主題是：OS 如何減少 page fault 的 I/O 成本，讓 demand paging 不要慢到爆炸。
### 1. Swap Space I/O 通常比 File System I/O 快
第一點寫：

**Swap space I/O faster than file system I/O even if on the same device**

意思是：就算 swap space 和一般檔案系統都在同一顆硬碟或 SSD 上，swap space 的 I/O 通常還是比較快。

原因是一般 file system 要處理很多額外管理工作，例如目錄結構、檔案 metadata、權限、block allocation、檔案碎片、journal 等等。讀一個檔案 page 時，OS 可能要經過比較多層 file system 邏輯。

但 swap space 是 OS 專門拿來放換出 pages 的區域。它通常以比較單純、連續、大塊的方式管理，不需要像一般檔案系統那樣處理一堆檔案語意。因此 swap I/O 可以更直接、更快。

投影片下面補充：

**Swap allocated in larger chunks, less management needed than file system**

也就是 swap space 常常以較大的 chunk 管理，減少細碎 block 管理成本。
### 2. Copy Entire Process Image to Swap Space at Process Load Time
第二個最佳化方法是：

**Copy entire process image to swap space at process load time**

意思是 process 剛被載入時，OS 可以先把整個 process image 複製到 swap space。之後 demand paging 發生時，就不要從原本的 executable file 或 file system 慢慢找 page，而是直接從 swap space page in / page out。

流程大概是：
```tex
程式從 hello.exe 載入。
OS 建立 process image。
OS 把整個 process image 先複製到 swap space。
之後缺 page 時，直接從 swap space 讀入。
之後要換出 page 時，也寫回 swap space。
```
好處是後續 page in / page out 都走 swap space，比走一般 file system 快。

投影片寫：

**Then page in and out of swap space**

就是這個意思。

但缺點也很明顯：process load time 會變長。因為程式一開始啟動時，就要先把整個 process image 複製到 swap space。這比較像是先多花一筆啟動成本，換取後面 paging 操作比較快。

投影片也註明：

**Used in older BSD Unix**

表示這是比較早期 BSD Unix 使用過的策略。
### 3. Demand Page from Program Binary，但 Free Frame 時直接丟掉
第三種策略比較聰明，也是現代 OS 常見邏輯：

**Demand page in from program binary on disk, but discard rather than paging out when freeing frame**

意思是：如果某個 page 的內容來自原本的 program binary，例如 executable file 裡的 code page，那第一次需要它時，可以直接從原本的 executable file 讀進 RAM。

但當 RAM 不夠，需要釋放這個 page frame 時，如果這個 page 沒有被修改過，就不需要把它寫回 swap space。因為原本 disk 上的 executable file 還有一份乾淨版本。下次需要時，再從原本 program binary 讀進來即可。
這對 read-only code pages 特別有用。

例如：

`libc.so` 的 code page 被載入 RAM。
它是 read-only，process 不會修改它。
RAM 不夠時，OS 可以直接把這個 page frame 回收。
不用寫回 disk。
下次需要時，再從 `libc.so` 檔案讀進來。

所以這種策略的重點是：

**乾淨的 file-backed page 可以直接丟掉，不用 swap out。**

這可以省掉一次昂貴的 disk write。

### 4. 但仍然需要寫到 Swap Space 的情況
#### 4.1 Anonymous Memory：沒有對應檔案的記憶體
投影片寫：

**Pages not associated with a file like stack and heap — anonymous memory**

`anonymous memory` 指的是沒有對應到任何檔案的記憶體頁面。最典型就是：

stack
heap

例如 heap 裡面由 `malloc()` 配置出的資料，本來不是從 executable file 讀出來的。Stack 裡的 local variables、function call state，也不是某個檔案裡原本就存在的內容。

所以這些 page 如果要被換出 RAM，不能說「反正 disk 上原本有一份，下次再讀回來」。因為 disk 上根本沒有原始版本。

因此 anonymous pages 被 eviction(驅逐) 時，如果內容還需要保留，就必須寫到 swap space。

簡單講：

code page：通常 file-backed，可以丟掉後從檔案重讀。
heap / stack page：通常 anonymous，沒有檔案來源，必須 swap out 保存內容。
#### 4.2 Modified File-backed Pages：檔案頁被改過但還沒寫回檔案系統
投影片寫：

**Pages modified in memory but not yet written back to the file system**

這是在說 dirty page。

有些 page 雖然本來來自檔案，例如 memory-mapped file，但 process 在 RAM 裡修改過它。此時 RAM 裡的版本比 disk 上的新，這個 page 就是 dirty。

如果直接丟掉，修改內容會消失，所以必須先寫回某個地方。可能是寫回原本檔案，也可能先寫到 swap space，視 OS 和 mapping 類型而定。

所以這裡的概念是：

乾淨 page 可以丟。
dirty page 不可以直接丟。
dirty page 必須被寫回 disk 或 swap，否則資料遺失。

# 10.3 Copy on Write
## Copy-on-Write 的核心概念
Copy-on-Write，**COW**，中文可翻成「寫入時複製」。這個機制主要用在 `fork()` 建立 child process 的時候，目的就是：**不要一 fork 就把整個 parent process 的記憶體全部複製一份**。

正常直覺會覺得，`fork()` 之後 parent 和 child 是兩個不同 process，所以 child 應該要有一份自己的 code、data、heap、stack。但如果 parent process 很大，立刻完整複製會非常慢，也很浪費 RAM。

COW 的做法比較聰明：一開始 parent 和 child 先**共享同一批 physical pages**。也就是 parent 的 page table 和 child 的 page table 會指向相同的 physical frames。只要雙方都只是讀取資料，就完全不用複製。

真正發生複製的時機是：
* **只要 parent 或 child 其中一方嘗試修改某個 shared page，OS 才真的複製那一頁。**
所以 COW 的名稱很直覺：
* Copy on Write = 寫入時才複製。
## 為什麼 COW 可以加速 process creation？
COW 可以讓 `fork()` 變快，因為建立 child process 時，只需要複製 page table / PCB 等管理資料，不需要立刻複製所有 physical pages。

例如 parent 有 1000 個 pages。如果沒有 COW，`fork()` 可能要複製 1000 個 pages。
有 COW 後，一開始只需要讓 child 的 page table 指向 parent 的 physical pages，並把這些 pages 標成「共享、暫時不可寫」。

如果 child 後來只呼叫 `exec()` 載入新程式，那 parent 的原本 pages 根本不需要複製。這在 Unix/Linux 很常見：

`fork()` 建立 child
→ child 馬上 `exec()` 執行新程式
→ 原本 address space 被新程式取代
→ 先前若完整複製 parent 記憶體，全部都是浪費

所以 COW 讓 process creation 更有效率，因為**只有真的被修改的 pages 才會被 copied**。

## COW 實際怎麼觸發？
![image](https://hackmd.io/_uploads/HJ9dp5rWMx.png)
![image](https://hackmd.io/_uploads/SJ1FpcSWfg.png)
COW 的關鍵是 page table 權限控制。

在 `fork()` 後，parent 和 child 的 page table 都指向同一批 physical pages，但 OS 會把這些 pages 標成 read-only。注意，這不是因為資料本來真的不能寫，而是 OS 故意這樣設計，用來偵測誰想修改 shared page。

**流程如下：**
1. Parent 和 child 共享 page C。
2. 兩邊的 page table 都指向同一個 physical page C。
3. OS 把 page C 標成 read-only / COW。
4. 某個 process 嘗試寫 page C。
5. CPU 發現寫入 read-only page，觸發 page fault。
6. OS 檢查後發現這不是非法錯誤，而是 COW fault。
7. OS 配置一個新的 free frame。
8. OS 把 page C 的內容複製到新 frame。
9. 修改寫入者的 page table，讓它指向新的 copy。
10. 把新的 page 設成 writable。
11. 重新執行剛剛的寫入指令。
## Zero-fill-on-demand pages 是什麼？
第 53 頁中間提到：

**free pages are allocated from a pool of zero-fill-on-demand pages**

意思是 OS 通常會維護一批已經清成 0 的 free frames。當 process 需要新的 page，例如 heap 長大、stack 長大，或 COW 需要新 page frame 時，可以快速拿一個乾淨的 frame 來用。

這裡有兩個目的。

第一是**安全性**。如果 frame 以前被別的 process 用過，裡面可能殘留別人的資料。如果直接給新 process 使用，新 process 可能讀到舊資料，這是嚴重安全問題。所以 OS 會先把 page 清成 0。

第二是**效能**。如果 page fault 發生時才臨時找 frame、清成 0、再處理其他事情，page fault latency 會變長。OS 維持一個 zero-filled page pool，可以讓 demand paging 和 COW fault 處理更快。

所以投影片寫：

**Pool should always have free frames for fast demand page execution.**

意思是 OS 最好保持一些可立即使用的乾淨 page frames，避免 page fault 發生時還要額外花時間整理空間。
# 10.4 Page Replacement
## 10.4 Page Replacement 前言重點
### What Happens if There is no Free Frame?
前面 page fault 的流程是假設 OS 可以找到一個 **free frame**，然後把 missing page 從 disk 載入 RAM。但實際系統中，physical memory 很有限，frame 可能已經被用光。

Frame 可能被各種東西佔用：

process 的 pages
kernel 本身
I/O buffers
file cache
其他 OS 資料結構

所以當 page fault 發生時，OS 可能會遇到：

**需要載入新的 page，但 RAM 裡沒有空 frame**。

這時候就不能只是單純「find free frame」，而是要改成：

**找一個目前在 memory 裡，但短期內比較不需要用的 page，把它 page out，空出 frame 給新的 page**。

這個被犧牲的 page 通常叫做 **victim page**（犧牲頁）。
### Page Replacement 的基本動作
Page replacement 的流程可以想成：
1. 發生 page fault。
2. OS 發現 missing page 合法，但不在 memory。
3. OS 嘗試找 free frame。
4. 如果沒有 free frame，就挑一個 victim page。
5. 如果 victim page 被修改過，要先寫回 disk。
6. 把 victim page 從 memory 移除。
7. 把 missing page 載入剛空出的 frame。
8. 更新 page table。
9. 重新執行造成 page fault 的指令。

所以 page replacement 是 page fault handler 的延伸版本。
### 為什麼 Page Replacement 需要演算法？
投影片寫：

**Algorithm — terminate? swap out? replace the page?**

意思是 OS 必須決定怎麼處理 memory 不足。理論上可以有幾種選擇：

直接終止 process。
把整個 process swap out。
或只置換掉某個 page。

現代 demand paging 通常採用第三種：**replace one page**。

但問題是：要換掉哪一頁？

如果換掉一頁很快又會被用到的 page，那它之後又會 page fault，被換回來。這會造成大量 I/O，效能很差。

所以 page replacement algorithm 的目標是：

**盡量選一個未來最不可能被用到的 victim page，讓 page faults 數量最少。**

投影片的 performance 重點就是：

**want an algorithm which will result in minimum number of page faults**

這就是後面 FIFO、Optimal、LRU 等演算法要處理的核心問題。
### Same Page May Be Brought Into Memory Several Times
意思是同一個 page 可能不是載入一次就永遠待在 RAM。

例如 page A 被載入 RAM。
後來 memory 滿了，OS 把 page A 選成 victim，換出到 disk。
之後 process 又需要 page A。
page A 再次 page fault，又被載入 RAM。

所以一個 page 可能反覆：
`page in → page out → page in → page out`
如果演算法選得很爛，就會一直把快要用到的 page 換出去，造成大量 page fault。這也是後面 **thrashing** 的前置概念。
### Dirty Bit / Modify Bit 的作用
第二個重點：

**Use modify (dirty) bit to reduce overhead of page transfers — only modified pages are written to disk**

這裡的 modify bit 又叫 dirty bit。

它用來記錄某個 page 在載入 memory 後，有沒有被修改過。

如果 dirty bit = 0，代表這個 page 沒被修改過，是乾淨的 clean page。
如果 dirty bit = 1，代表這個 page 被寫過，是 dirty page。

這很重要，因為 victim page 被換出時，不一定都要寫回 disk。

1. **情況一：Victim page 是 clean**
    * 如果 victim page 沒被修改過，它的內容和 disk 上原本的版本一樣。這時候 OS 可以直接丟掉它，不需要寫回 disk。
    * 例如 code page 通常是 read-only。若要換出 code page，可以直接丟掉。下次需要時，再從 executable file 讀回來即可。
    * 這是最快的情況。
2. **情況二：Victim page 是 dirty**
    * 如果 victim page 被修改過，RAM 裡的版本比 disk 上新。這時候不能直接丟掉，否則資料會遺失。
    * 所以必須先把 dirty page 寫回 disk，然後才能釋放 frame。
    * 因此 dirty bit 的目的就是：
    * **避免不必要的 disk write。只有被修改過的 page 才需要寫回 disk**。
### Page Replacement 完成 Logical Memory 和 Physical Memory 的分離
最後一句：

**Page replacement completes separation between logical memory and physical memory — large virtual memory can be provided on a smaller physical memory**

這句是整個 Chapter 10 的核心。

Demand paging 已經讓 process 不需要整個載入 RAM。
Page replacement 進一步讓 OS 在 RAM 滿的情況下，仍然可以繼續執行更大的 virtual memory。

也就是說，即使 virtual address space 很大，physical memory 很小，OS 還是可以透過：

page fault
page in
page out
page replacement

讓 process 以為自己擁有很大的 memory。

從 process 的角度看，它有完整的 logical memory。
從 OS 的角度看，只有目前需要的 pages 真的在 physical memory。
不常用的 pages 可以被換到 disk。

這就是 virtual memory 真正完整成立的原因。
## 10.4.1 Basic Page Replacement
### Basic Page Replacement 基本流程
這頁是在把 page fault 處理流程加上「沒有 free frame」時的處理方式。

前面 demand paging 的簡化流程是：page fault 發生後，OS 找到 desired page 在 disk 的位置，找一個 free frame，把 page 載入 memory，更新 page table，重新執行指令。

但現在問題是：**physical memory 可能已經滿了**。 所以第 2 步變得很重要。

如果有 free frame，就直接使用。
如果沒有 free frame，就必須用 **page replacement algorithm** 選一個 **victim frame**。

Victim frame 指的是：目前 RAM 裡某個被選中要犧牲的 frame。這個 frame 裡原本放著某個 page，OS 會把那個 page 趕出去，讓 frame 空出來給新的 desired page 使用。

**流程可以整理成：**
```tex
1. 找到 desired page 在 disk / backing store 的位置
2. 找 free frame
   - 有 free frame：直接用
   - 沒有 free frame：選 victim frame
3. 如果 victim page 是 dirty，就先寫回 disk
4. 把 desired page 載入這個 free frame / newly freed frame
5. 更新 page table 和 frame table
6. 重新執行造成 page fault 的指令
```
這頁最重要的是 dirty page 的處理。若 victim page 沒被修改過，代表 disk 上原本的版本仍然正確，可以直接丟掉；若 victim page 被修改過，代表 RAM 裡的版本比較新，必須先寫回 disk，否則資料會遺失。

所以 page replacement 的基本代價可能是：
```tex
沒有 dirty victim：
只需要 page in

有 dirty victim：
先 page out victim，再 page in desired page
```
**後者會多一次 disk write，成本更高。**
### Page Replacement 圖解流程
![image](https://hackmd.io/_uploads/ryci7sHZMe.png)
#### Step 1：Swap out victim page
第一步是把 victim page 換出去。圖中從 physical memory 的 victim frame 指向 disk，代表 victim page 被寫回 backing store。

但這一步只有在 victim page 是 dirty 時才真的需要。若 victim page 是 clean，就不用寫回 disk，可以直接丟掉。

所以更精準地說：
```tex
victim page dirty  → write back to disk
victim page clean  → discard directly
```
#### Step 2：Change old page table entry to invalid
Victim page 被移出 RAM 後，原本指向這個 frame 的 page table entry 必須改成 invalid。

因為那個 virtual page 現在已經不在 memory 了。如果 page table 還標成 valid，MMU 之後會以為它還在 RAM，導致錯誤的位址轉換。

所以 step 2 是在更新「被犧牲者」的 page table：
```tex
victim page 原本：valid，指向 frame f
victim page 換出後：invalid
```
這裡要注意，左邊 page table 上方那個 `0` `i` 可以理解成：某個 page 現在不在 RAM，所以 valid-invalid bit 是 invalid。
#### Step 3：Swap desired page in
第三步是把真正需要的 desired page 從 disk 載入剛剛空出來的 frame。

圖中從 disk 指向 physical memory 的 victim frame，代表原本 victim 佔用的 frame 現在被新 page 使用。

所以這個 frame 的身份改變了：
```tex
原本：frame f 裡放 victim page
現在：frame f 裡放 desired page
```
#### Step 4：Reset page table for new page
第四步是更新 desired page 的 page table entry。

Desired page 原本不在 memory，所以它的 page table entry 是 invalid。現在它已經被載入 frame f，所以要改成：

`frame number = f
valid-invalid bit = valid`

也就是圖左邊下面那格 `f v` 的意思。

最後 OS 重新執行造成 page fault 的指令。這次 MMU 查 page table 時，desired page 已經 valid，所以可以成功完成位址轉換。
### Page and Frame Replacement Algorithms
這頁開始進入後面演算法題的核心。它把兩種演算法分開：

**Frame-allocation algorithm：** 決定每個 process 可以拿到多少 frames，以及從哪些 frames 裡面做 replacement。
**Page-replacement algorithm：** 當需要換頁時，決定要踢掉哪一個 page。

這兩個概念不一樣。

Frame allocation 比較像是在問：
```tex
Process A 給幾個 frames？
Process B 給幾個 frames？
每個 process 的 frame 數固定還是可變？
```
Page replacement 則是在問：
```tex
現在 page fault 了，而且沒有 free frame。
RAM 裡有好幾個 page，哪一個要被選成 victim？
```
後面常見的 FIFO、Optimal、LRU，主要是在講 **page-replacement algorithm**。
### Reference String 是什麼？
這頁也介紹計算 page fault 的方式：用一串 reference string 來測試演算法。

Reference string 是一串「被存取的 page numbers」，不是完整 memory address。

投影片給的例子是：

`7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1`

這串代表 CPU 依序存取這些 pages。

例如第一個 `7` 表示存取 page 7。
第二個 `0` 表示存取 page 0。
第三個 `1` 表示存取 page 1。

演算法的任務就是：給定 frame 數量，例如 3 個 frames，照著這串 reference string 一步一步模擬，計算總共發生幾次 page fault。
## 10.4.2 FIFO Page Replacement
### 第 63 頁：FIFO Page Replacement Algorithm
FIFO 全稱是 First-In, First-Out，中文可以翻成「先進先出」。它的想法很直覺：**最早被放進 memory 的 page，最早被換出去**。

FIFO 不管某個 page 最近有沒有被使用，也不管它之後會不會再被用到，只看一件事：

**誰最早進來，誰就是下一個 victim。**

所以實作上只需要維護一個 FIFO queue。每次 page fault 且沒有 free frame 時，就把 queue 最前面的 page 換出去，再把新 page 放到 queue 最後面。
### FIFO 的判斷規則
給定 reference string：
`7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1`
題目說有 3 frames，代表同一時間 memory 裡最多只能放 3 個 pages。

FIFO 的規則如下：

如果 reference 的 page 已經在 frames 裡，這次是 **hit**，不產生 page fault。

如果 reference 的 page 不在 frames 裡，這次是 **page fault**。

若 frames 還沒滿，直接把 page 放進空 frame。

若 frames 已滿，根據 FIFO，把「最早進入 frames 的 page」換出去。

重點：**hit 不會改變 FIFO 順序**。
這點跟 LRU 不一樣。FIFO 不會因為某頁剛剛被用過，就把它變年輕。
### 3 Frames 計算
下面用 FIFO 追一次這串 reference string。括號中的順序可理解為 FIFO queue，左邊是最老、下一個會被換出去的 page。
```tex
Reference   Frames / FIFO queue      結果
7           [7]                       fault
0           [7, 0]                    fault
1           [7, 0, 1]                 fault
2           [0, 1, 2]                 fault，換出 7
0           [0, 1, 2]                 hit
3           [1, 2, 3]                 fault，換出 0
0           [2, 3, 0]                 fault，換出 1
4           [3, 0, 4]                 fault，換出 2
2           [0, 4, 2]                 fault，換出 3
3           [4, 2, 3]                 fault，換出 0
0           [2, 3, 0]                 fault，換出 4
3           [2, 3, 0]                 hit
2           [2, 3, 0]                 hit
1           [3, 0, 1]                 fault，換出 2
2           [0, 1, 2]                 fault，換出 3
0           [0, 1, 2]                 hit
1           [0, 1, 2]                 hit
7           [1, 2, 7]                 fault，換出 0
0           [2, 7, 0]                 fault，換出 1
1           [7, 0, 1]                 fault，換出 2
```
所以總共是：
`15 page faults`
這就是投影片上寫的 15 page faults。
### FIFO 的缺點
FIFO 最大問題是：**它不管 page 最近有沒有被用過**。

例如某個 page 很早就進入 memory，但其實它一直被頻繁使用。FIFO 還是會因為它「年紀最老」而把它換出去。

所以 FIFO 很簡單，但不聰明。它只記「進來時間」，不記「使用情況」。

這也是為什麼 FIFO 有時候會把很重要、很常用的 page 換掉，造成後續又馬上 page fault。
### Belady’s Anomaly：Frame 變多，Page Fault 反而變多
正常直覺會覺得：frames 越多，page faults 應該越少。

但 FIFO 有一個反直覺現象：增加 frames 數量，有時反而造成更多 page faults。 這稱為 Belady’s Anomaly。

經典 reference string 是：

`1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5`

使用 FIFO 時：

`3 frames → 9 page faults
4 frames → 10 page faults`

也就是 frame 多了，page fault 反而增加。

**為什麼 FIFO 會有 Belady’s Anomaly？**

原因是 FIFO 只看「進入 memory 的時間」，不看「最近是否使用」。

當 frame 數量改變時，FIFO queue 的淘汰順序也會改變。多一個 frame 不一定代表保留了更有用的 page，反而可能讓某些老但不重要的 page 留更久，導致真正快要用到的 page 被換掉。

也就是說，FIFO 沒有保證：

`frames = 4 時保留的 pages
一定包含 frames = 3 時保留的 pages`

這種沒有「包含性」的演算法，就可能出現 Belady’s anomaly。
## 10.4.3 Optimal Page Replacement
### Optimal Algorithm 是什麼？
Optimal page replacement algorithm（最佳頁面置換演算法）的規則是：

**當 page fault 發生且 frames 已滿時，換掉「未來最久之後才會再被用到」的 page。**

也可以白話記成：

**離下一次使用最遠的 page 先踢掉。
如果某個 page 之後完全不會再用到，那它最適合被換掉。**
### 範例
![image](https://hackmd.io/_uploads/HkXSajrWGe.png)
用 Optimal 演算法跑這串 reference string，3 frames 會得到：
```tex
Reference   Frames        結果
7           [7]           fault
0           [7,0]         fault
1           [7,0,1]       fault
2           [2,0,1]       fault，換出 7
0           [2,0,1]       hit
3           [2,0,3]       fault，換出 1
0           [2,0,3]       hit
4           [2,4,3]       fault，換出 0
2           [2,4,3]       hit
3           [2,4,3]       hit
0           [2,0,3]       fault，換出 4
3           [2,0,3]       hit
2           [2,0,3]       hit
1           [2,0,1]       fault，換出 3
2           [2,0,1]       hit
0           [2,0,1]       hit
1           [2,0,1]       hit
7           [7,0,1]       fault，換出 2
0           [7,0,1]       hit
1           [7,0,1]       hit
```
總共：
9 page faults
意思是：在這個 reference string、3 frames 的條件下，**任何 page replacement algorithm 都不可能比 9 page faults 更少**。

### 為什麼它叫 Optimal？
因為它每次都做「理論上最好的選擇」。

當要換 page 時，如果已經知道未來 reference string，那最合理的選擇一定是：

把最晚才會再用到的 page 換掉。

因為保留近期會用到的 page，可以避免短期內再次 page fault。
### 但為什麼實際 OS 不能直接用？
這就是重點。

Optimal algorithm 需要知道未來 memory references。可是實際作業系統在執行時，不可能知道 process 之後會依序存取哪些 pages。

所以 Optimal algorithm 不是實務上能真正實作的演算法。

它主要用途是：

拿來當 benchmark，也就是評估其他演算法表現好不好。

例如同一串 reference string：

Optimal：9 page faults
FIFO：15 page faults

代表 FIFO 距離理論最佳值還有一段差距。
## 10.4.4 LRU Page Replacement
### Least Recently Used
意思是「最久沒有被使用的 page」。
它的核心想法是：

**過去最久沒被用到的 page，未來也比較可能暫時不會用到，所以 page fault 時就換掉它。**

這和 Optimal Algorithm 剛好形成對比。Optimal 是看未來，選「未來最久不會用到」的 page；但 OS 不可能預知未來，所以 LRU 改用過去的資訊，選「過去最久沒用到」的 page。
### LRU 的基本規則
LRU 的 replacement rule 是：

**Replace page that has not been used in the most amount of time.**

也就是 page fault 且 frames 已滿時，檢查目前 memory 裡的 pages，找出「距離上次被 reference 最久」的 page，把它當成 victim。

以三個 frames 來說，假設目前 memory 裡有：
`[2, 0, 3]`
如果過去最近使用順序是：
```tex
最近：3
再來：0
最久以前：2
```
那 page fault 時，LRU 會換掉 2，因為 2 是目前 frames 裡最久沒被用到的 page。
### LRU 和 FIFO 的差別
FIFO 看的是：
* **誰最早進入 memory。**
LRU 看的是：
* **誰最久沒有被 reference。**

這兩個完全不同。

例如某個 page 很早就進入 memory，但一直被使用。FIFO 仍然可能因為它「最老」而把它換掉；LRU 則會保留它，因為它最近剛被用過。

所以 LRU 通常比 FIFO 聰明。

投影片上的結果是：

FIFO：15 page faults
LRU：12 page faults
Optimal：9 page faults

所以 LRU 的表現是：

比 FIFO 好，但比理論最佳的 Optimal 差。

這很合理，因為 LRU 只能根據過去推測未來，不可能真的知道未來。
### 範例
`7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1`
| 步驟 | Reference | Frame 1 | Frame 2 | Frame 3 | Hit / Fault | 換掉誰 | 累積 faults|
| -: | --------: | ------: | ------: | ------: | ----------- | --: | --------: |
|  1 |         7 |       7 |         |         | Fault       |     |         1 |
|  2 |         0 |       7 |       0 |         | Fault       |     |         2 |
|  3 |         1 |       7 |       0 |       1 | Fault       |     |         3 |
|  4 |         2 |       2 |       0 |       1 | Fault       |   7 |         4 |
|  5 |         0 |       2 |       0 |       1 | Hit         |     |         4 |
|  6 |         3 |       2 |       0 |       3 | Fault       |   1 |         5 |
|  7 |         0 |       2 |       0 |       3 | Hit         |     |         5 |
|  8 |         4 |       4 |       0 |       3 | Fault       |   2 |         6 |
|  9 |         2 |       4 |       0 |       2 | Fault       |   3 |         7 |
| 10 |         3 |       4 |       3 |       2 | Fault       |   0 |         8 |
| 11 |         0 |       0 |       3 |       2 | Fault       |   4 |         9 |
| 12 |         3 |       0 |       3 |       2 | Hit         |     |         9 |
| 13 |         2 |       0 |       3 |       2 | Hit         |     |         9 |
| 14 |         1 |       1 |       3 |       2 | Fault       |   0 |        10 |
| 15 |         2 |       1 |       3 |       2 | Hit         |     |        10 |
| 16 |         0 |       1 |       0 |       2 | Fault       |   3 |        11 |
| 17 |         1 |       1 |       0 |       2 | Hit         |     |        11 |
| 18 |         7 |       1 |       0 |       7 | Fault       |   2 |        12 |
| 19 |         0 |       1 |       0 |       7 | Hit         |     |        12 |
| 20 |         1 |       1 |       0 |       7 | Hit         |     |        12 |

### LRU 的實作問題
LRU 概念很漂亮，但真的實作不簡單。因為理想 LRU 需要知道每個 page「上次被使用的時間」。投影片列了兩種實作方式：

第一種是 **Counter implementation**。
第二種是 **Stack implementation**。

### Counter Implementation：用 counter 記錄上次使用時間

Counter implementation 的概念是：

系統有一個遞增的 clock / counter。
每個 page table entry 裡面都有一個 counter 欄位。
每次某個 page 被 reference，就把目前 clock 值複製到該 page 的 counter。
當需要 replacement 時，找 counter 值最小的 page。

Counter 值越小，代表它越久以前被使用。

例如：

page 0 counter = 100
page 1 counter = 250
page 2 counter = 80

那 LRU 會換掉 page 2，因為 80 最小，代表 page 2 最久沒用。

這種方法的缺點是：

每次 replacement 時可能要掃描 page table，找最小 counter。

如果 page table 很大，掃描成本就很高。
### Stack Implementation：用 stack 記錄最近使用順序
Stack implementation 的概念是用一個雙向 linked list 維護 page 使用順序。

Stack 最上面是：

**最近剛被使用的 page。**

Stack 最下面是：

**最久沒有被使用的 page，也就是 LRU victim。**

每次某個 page 被 reference，就把它移到 stack 最上面。

如果 page fault 且需要 replacement，就直接換掉 stack 最下面的 page。

所以 Stack implementation 的好處是：

**replacement 時不用搜尋，最下面那個就是 LRU page。**

但缺點是：

**每次 reference 都要更新 linked list。**

投影片寫 `requires 6 pointers to be changed`，意思是如果某個 page 在 linked list 中間，被 reference 後要搬到最上面，會牽涉多個指標更新。例如要把它從原位置拔出來，再接到 stack top。

所以 stack implementation 的取捨是：

replacement 快，但每次 reference 更新成本高。
### ==用 Stack 記錄最近使用的 pages==
![image](https://hackmd.io/_uploads/S1jmQhSbGx.png)
這頁是在圖解 stack implementation。

Reference string 是：

`4, 7, 0, 7, 1, 0, 1, 2, 1, 2, 7, 1, 2`

圖中有兩個時間點：

`a`：reference 到某個 page 前的 stack 狀態。
`b`：reference 到 page `7` 之後的 stack 狀態。

在 a 時，stack 是：
```
top
2
1
0
7
4
bottom
```
這表示最近使用的是 `2`，最久沒使用的是 `4`。

接著 reference 到 `7`。因為 `7` 已經在 stack 裡，所以不是新增 page，而是把 `7` 從原本位置移到最上面。

因此 b 之後 stack 變成：
```
top
7
2
1
0
4
bottom
```
意思是：`7` 剛剛被 reference，所以變成 most recently used；`4` 仍然在最下面，是 least recently used。

這張圖的重點是：

LRU stack 不是記錄 page 進入 memory 的時間，而是記錄 page 最近被使用的順序。
## 10.4.5 LRU Approximation Algorithms
### 前言
這頁的主題是：真正的 LRU 很好，但實作成本太高，所以 OS 常用近似版。

真正 LRU 要精確知道每個 page「上一次被使用的時間」，這需要硬體支援或頻繁更新資料結構。每次 memory reference 都要更新時間戳或 stack，成本太高。因此實務上常用 **LRU Approximation Algorithm（LRU 近似演算法）**，用比較便宜的方法猜測哪個 page 最近比較少用。
### Reference Bit：參考位元
**Reference bit** 是每個 page table entry 裡的一個硬體輔助位元。它的意思是：

`reference bit = 0`：最近還沒有被使用過。
`reference bit = 1`：最近有被使用過。

一開始，每個 page 的 reference bit 通常設為 `0`。只要某個 page 被 CPU reference，也就是被讀取、寫入或取指令，硬體就會把該 page 的 reference bit 設成 `1`。

所以 reference bit 不會記錄「精確使用時間」，只記錄一個粗略資訊：
**這個 page 最近有沒有被碰過。**
因此它只能近似 LRU，不能做到真正 LRU。
### Reference Bit 的限制
如果要做 replacement，看到某個 page 的 reference bit 是 0，代表它最近沒有被使用過，可以考慮換掉。

但問題是：如果很多 pages 的 reference bit 都是 1，就不知道誰比較早、誰比較晚被使用。因為 reference bit 只有 0 或 1，沒有順序資訊。

所以投影片寫：

We do not know the order, however

意思是：reference bit 可以知道「有沒有用過」，但不知道「誰比較久沒用」。
### ==Second-Chance Algorithm：第二次機會演算法==
![image](https://hackmd.io/_uploads/BJUhN2SbMg.png)
Second-Chance Algorithm 可以理解成：

FIFO + reference bit

原本 FIFO 是最早進來的 page 最早被換出去。但 Second-Chance 會在換掉它之前，先看它的 reference bit。

規則是：

如果候選 victim 的 reference bit = 0，代表最近沒被用過，直接換掉。

如果候選 victim 的 reference bit = 1，代表最近有被用過，所以給它第二次機會：
把 reference bit 清成 0，但暫時不換掉它，然後繼續檢查下一個 page。

所以 Second-Chance 的精神是：

**老 page 不一定馬上被踢掉；如果最近有被用過，就先放過一次。**
### ==Clock Replacement：時鐘演算法==
**可以整理成這樣：**
```
page fault 發生，且沒有 free frame
↓
clock hand 指向某個 page
↓
看 reference bit

如果 reference bit = 0：
    選這個 page 當 victim
    把新 page 放進這個 frame
    新 page 的 reference bit 設為 1
    clock hand 移到下一格

如果 reference bit = 1：
    把 reference bit 改成 0
    不換掉它
    clock hand 移到下一格
    繼續檢查
```
若所有 pages 的 reference bit 一開始都是 1，clock hand 可能會繞一圈，把全部都清成 0。接著第二圈遇到第一個 0 時，就會選它當 victim。

這也是為什麼它叫 second chance：**reference bit = 1 的 page 至少可以逃過第一次掃描**。
# 10.5 Page Allocation
## 開頭
### Allocation of Frames 的前言
第 75 頁：Allocation of Frames 的前言

這一節開始討論 Frame Allocation（頁框分配）。前面 Page Replacement 關心的是：「沒有空 frame 時，要換掉哪一頁？」而 Frame Allocation 關心的是更前一層的問題：

**系統總共有一堆 physical frames，應該分配幾個給每個 process？**

因為 physical memory 是有限資源，不可能每個 process 想要多少 frame 就給多少。給太少，process 很容易 page fault；給太多，其他 process 就沒 frame 可用，系統同時能跑的 process 變少。

### 每個 process 都需要最少數量的 frames
投影片第一點寫：

**Each process needs minimum number of frames**

意思是每個 process 至少要有某個最低數量的 frames，否則連基本指令都可能無法正常執行。

原因是：一條 instruction 可能同時需要多個 pages。例如 instruction 本身可能跨 page，operand 也可能跨 page。如果 frame 太少，CPU 可能載入 instruction page 後，又要載入 data page，結果 instruction page 被換掉；接著要重新執行 instruction，又發現 instruction page 不在 memory，形成無限 page fault，程式根本跑不起來。

所以 OS 必須保證 process 至少有足夠 frames，能完成最基本的 instruction execution。
### Maximum frames 是系統總 frame 數
投影片接著寫：

**Maximum of course is total frames in the system**

意思很直覺：一個 process 最多可以拿到多少 frames？理論上最多就是整個系統可用的 frame 數量。

但實務上不會真的把所有 frames 都給一個 process，因為 kernel、I/O buffer、其他 process 都需要 memory。

所以 frame allocation 真正的問題是在 minimum 和 maximum 之間找平衡：

給太少 → page fault 多。
給太多 → 其他 process 沒 memory。
要公平，也要有效率。
## Fixed Allocation
### 前言
Fixed Allocation 的意思是：**先決定每個 process 可以拿多少 frames，然後這個數量在一段時間內固定**。

這頁主要講兩種 fixed allocation：
* **Equal allocation（平均分配）**
* **Proportional allocation（比例分配）**

### Equal Allocation：平均分配
投影片例子：

如果 OS 保留自己需要的 frames 之後，還剩下：

100 frames

目前有：

5 processes

那平均分配就是：

100 / 5 = 20 frames per process

所以每個 process 都拿 20 個 frames。

這種方式優點是簡單、公平，看起來每個 process 都一樣。

但缺點也很明顯：process 大小不同時，平均分配不一定合理。

例如一個小 process 只需要 10 pages，卻拿到 20 frames，可能浪費。另一個大 process 需要 500 pages，也只拿到 20 frames，可能一直 page fault。

所以 equal allocation 很簡單，但不夠聰明。
### Free Frame Buffer Pool
投影片在 equal allocation 下補一句：

**Keep some as free frame buffer pool**

意思是系統通常不會把所有 frames 都分光，會留下一些 frames 當作 **free frame buffer pool**。

這樣 page fault 發生時，OS 可以快速拿 free frame 使用，不用每次都馬上做 page replacement。

如果完全沒有保留 free frames，一旦 page fault 發生，就常常要先選 victim、寫回 dirty page、再載入新 page，成本很高。

所以 free frame buffer pool 是為了讓 demand paging 反應更快。
### Proportional Allocation：依 process size 比例分配
Proportional allocation 比 equal allocation 更合理一點。它根據 process 的大小分配 frames。
定義如下：
```
s_i = process p_i 的大小
S = 所有 process 大小總和 = Σs_i
m = 系統可分配的總 frames 數量
a_i = process p_i 分到的 frames 數量
```
公式是：
```
a_i = (s_i / S) × m
```
意思是：process 越大，分到越多 frames；process 越小，分到越少 frames。

第 76 頁右下角例子

投影片給：
```
m = 64
s1 = 10
s2 = 127
```
所以總 process size：
```
S = 10 + 127 = 137
```
但投影片計算時用的是 62，不是 64：
```
a1 = 10 / 137 × 62 ≈ 4
a2 = 127 / 137 × 62 ≈ 57
```
這通常表示 64 frames 裡面可能保留了 2 個 frames 給 OS 或 free frame buffer pool，所以真正拿來分配給 process 的是：
```
m = 62
```
因此：
```
a1 = (10 / 137) × 62 ≈ 4.52 → 約 4
a2 = (127 / 137) × 62 ≈ 57.48 → 約 57
```
加起來約：
```
4 + 57 = 61
```
剩下可能因為四捨五入或 buffer pool 保留而沒有完全分掉。
這個例子重點不是小數怎麼處理，而是比例分配精神：
**p2 比 p1 大很多，所以 p2 應該拿到遠多於 p1 的 frames。**
### Proportional Allocation 的動態性
投影片寫：

**Dynamic as degree of multiprogramming, process sizes change**

意思是 proportional allocation 會受到系統狀態影響。

如果 process 數量改變，例如新 process 進來、舊 process 結束，總大小 S 會改變，每個 process 分到的 frame 數也可能重新調整。

如果某個 process 的 size 變大，例如 heap 成長，或 mmap 新區域，它可能需要更多 frames。

所以 proportional allocation 比 equal allocation 更符合 process 需求，但管理上也比較複雜。
## Priority Allocation
直接用一個小例子看 **Priority Allocation** 怎麼分配 frames。

假設 OS 保留 kernel 需要的 frames 後，剩下 **60 個 frames** 可以分給 3 個 processes：

| Process | Priority 權重 | 說明   |
| ------- | ----------: | ---- |
| P1      |           3 | 高優先權 |
| P2      |           2 | 中優先權 |
| P3      |           1 | 低優先權 |

總 priority 權重：

```text
3 + 2 + 1 = 6
```

所以用比例分配：

```text
P1 = 3 / 6 × 60 = 30 frames
P2 = 2 / 6 × 60 = 20 frames
P3 = 1 / 6 × 60 = 10 frames
```

結果就是：

| Process | 分到的 frames |
| ------- | ---------: |
| P1      |         30 |
| P2      |         20 |
| P3      |         10 |

這就是 Priority Allocation 的意思：**不是每個 process 平均分，也不是看 process size，而是根據 priority 權重分配。**

---

再看 page fault 時怎麼辦。

假設目前沒有 free frame，而 **P1 發生 page fault**。因為 P1 是高優先權 process，OS 可以有兩種選擇：

第一種是 **local replacement**：只從 P1 自己的 30 個 frames 裡挑一個 victim page 換掉。

第二種是 **priority-based replacement**：因為 P1 優先權比較高，所以 OS 可以從較低優先權的 process，例如 P2 或 P3，挑一個 frame 讓出來給 P1。

例如從 P3 拿一個 frame：

```text
原本：
P1 = 30 frames
P2 = 20 frames
P3 = 10 frames

P1 page fault 後，搶 P3 一個 frame：
P1 = 31 frames
P2 = 20 frames
P3 = 9 frames
```

所以 Priority Allocation 的精神就是：

**高優先權 process 不只一開始可以分到比較多 frames，在 page fault 時也可能優先取得低優先權 process 的 frame。**

## Global Replacement 和 Local Replacement
**Global replacement 的意思是**：當某個 process 發生 page fault 且沒有 free frame 時，OS 可以從「整個系統所有 frames」裡挑 victim。也就是說，process A 缺 frame 時，可以把 process B 的某個 frame 搶過來用。這種方式彈性高，因為 memory 可以流向目前最需要的 process，所以整體 **throughput** 通常比較好，也比較常見。但缺點是每個 process 的效能會變得比較不穩定，因為某個 process 原本分到的 frames 可能突然被別人拿走，導致之後 page fault 變多。

**Local replacement 的意思是**：每個 process 只能從「自己已經被分配到的 frames」裡挑 victim。也就是 process A 發生 page fault 時，只能換掉 process A 自己的 page，不能搶 process B 的 frame。這樣每個 process 的 frame 數比較固定，所以單一 process 的效能比較穩定、比較可預測。但缺點是 memory 利用率可能比較差，因為某些 process 可能 frames 很多但用不滿，另一些 process frames 不夠卻不能借用。
## NUMA：Non-Uniform Memory Access，非均勻記憶體存取。
### 解釋
![image](https://hackmd.io/_uploads/SycAu3rZzg.png)
前面討論 frame allocation、page replacement 時，通常先假設「所有 CPU 存取所有 memory 的速度都一樣」。但在現代多 CPU / 多 socket / 多 NUMA node 系統中，這個假設不一定成立。某顆 CPU 存取「離自己近」的 memory 會比較快；存取「掛在其他 CPU 或其他 system board 旁邊」的 memory 會比較慢。這種架構就叫 **NUMA**。
### 為什麼這跟 Frame Allocation 有關？
Frame allocation 原本是在問：
**每個 process 要分到幾個 frames？**
但在 NUMA 系統中，還要多問一件事：
這些 frames 應該分配在哪個 NUMA node 上？
最佳情況是：thread 被排程在哪顆 CPU 上執行，就盡量把它使用的 memory 分配在靠近那顆 CPU 的 memory node 裡。
這就是投影片寫的：
**Optimal performance comes from allocating memory “close to” the CPU on which the thread is scheduled.**
意思是：thread 跑在哪裡，memory 就盡量放在哪裡附近。
這樣可以降低 memory access latency，提高 cache / memory 效能。
### Scheduler 也要配合
只把 memory 分配在靠近 CPU 的地方還不夠，scheduler 也要配合。
假設某個 thread 的 memory 大多分配在 CPU 0 附近，但 scheduler 下一次卻把這個 thread 排到 CPU 1 上執行，那 thread 之後就會一直跨 node 存取遠端 memory，效能就變差。

所以 NUMA 最佳化需要兩件事一起做：
```
1. Memory allocation：
   盡量把 page frame 分配在靠近 thread 執行 CPU 的 memory node。

2. CPU scheduling：
   之後也盡量把該 thread 排回同一個 CPU / 同一個 NUMA node 上執行。
```
投影片這句：
modifying the scheduler to schedule the thread on the same system board when possible
就是這個意思。Scheduler 不只看 CPU 是否空閒，也要考慮 thread 的 memory locality。
# 10.6 Thrashing 整體核心

**Thrashing** 的意思是：系統花太多時間在 **paging / swapping pages in and out**，反而沒有時間真正執行程式。也就是 process 一直缺 page、一直 page fault、一直把 page 從 disk 搬進 RAM，又馬上把另一個等等會用到的 page 換出去，最後 CPU 幾乎都在等 paging I/O。課本定義很直白：如果 process 花在 paging 的時間比 executing 還多，就是 thrashing。講義也整理成「系統大部分時間都在 swapping process pieces，而不是 executing instructions」。

最短記憶法：

```txt id="ujcjyw"
Thrashing =
RAM 不夠放 active pages
→ page fault 爆增
→ 一直換頁
→ CPU 等 I/O
→ 效能崩潰
```

---

## 1. 為什麼會 Thrashing？

Thrashing 的根本原因是：**process 沒有拿到足夠 frames，無法容納目前正在活躍使用的 pages。**

一個 process 在某段時間內會集中使用一批 pages，例如目前正在跑某個 function、迴圈、陣列處理。這批正在活躍使用的 pages 可以先粗略叫做它的 **working set**。如果 OS 給這個 process 的 frames 太少，working set 放不進 RAM，那 process 就會一直發生 page fault。

流程如下：

```txt id="itz2ww"
Process 需要 page A
→ page A 不在 RAM
→ page fault
→ OS 找 frame 載入 page A
→ 但 frame 不夠，所以替換掉 page B
→ Process 很快又需要 page B
→ 又 page fault
→ 再把其他 page 換掉
→ 不斷重複
```

這就是「剛換出去的 page 很快又被需要」，也就是 thrashing 的典型狀態。

---

## 2. 10.6.1 Cause of Thrashing：早期 paging system 的災難循環

課本用一個早期 paging system 的行為來說明 thrashing 怎麼被 OS 自己越搞越嚴重。

OS 通常會監控 **CPU utilization**。如果 CPU utilization 太低，OS 可能會以為：「是不是 RAM 裡面的 processes 太少？CPU 沒東西可以跑？」於是 OS 會提高 **degree of multiprogramming**，也就是把更多 processes 加進系統。

正常情況下，增加 multiprogramming 的確可以提高 CPU utilization，因為當某些 process 等 I/O 時，CPU 可以跑其他 process。但如果系統已經 memory pressure 很高，再加入新的 process，就會讓 frames 更不夠分。

災難流程如下：

```txt id="t0818d"
CPU utilization 下降
        ↓
OS 誤判：系統中 process 太少
        ↓
提高 degree of multiprogramming
        ↓
加入更多 process
        ↓
每個 process 分到的 frames 更少
        ↓
page fault 更多
        ↓
更多 process 等 paging device
        ↓
ready queue 變空
        ↓
CPU utilization 更低
        ↓
OS 又繼續加入 process
        ↓
Thrashing
```

這個循環最麻煩的點在於：**CPU utilization 下降的真正原因不是 process 太少，而是太多 process 都在等 page fault I/O。** OS 如果只看 CPU utilization，就會做出錯誤反應：繼續增加 multiprogramming，反而讓 thrashing 更嚴重。

---

## 3. Global Replacement 為什麼會加劇 Thrashing？

課本中特別提到 **global page-replacement algorithm**。Global replacement 的意思是：當某個 process 發生 page fault 需要 frame 時，它可以從所有 frames 中挑 victim，不一定只能換掉自己的 page。講義也整理：global replacement 可以讓一個 process 從其他 process 那裡拿 frame，throughput 可能較高，但 per-process execution time 會變得很不穩定。

在 thrashing 情境下，global replacement 會變得危險：

```txt id="d5aqsz"
Process A page fault
→ A 從 Process B 那裡搶走 frame
→ B 原本需要的 page 被換出去
→ B 很快 page fault
→ B 又從 C 或 A 搶 frame
→ C 又 page fault
→ 全部 process 互相害對方
```

所以 global replacement 可能讓 thrashing 在 processes 之間傳染。某個 process frames 不夠時，可能把其他 process 的 working set 也破壞掉，造成整個系統 page fault rate 爆炸。

---

## 4. Figure 10.20：CPU utilization vs. degree of multiprogramming

Figure 10.20 的 x 軸是 **degree of multiprogramming**，也就是系統中同時存在多少 processes。y 軸是 **CPU utilization**。

圖的意思是：

```txt id="vwsgdp"
一開始 process 數量增加
→ CPU utilization 上升

到某個最佳點附近
→ CPU utilization 達到高峰

再繼續增加 process
→ memory 不夠分
→ page fault 大量增加
→ thrashing 出現
→ CPU utilization 急速下降
```

這張圖非常重要，因為它反直覺。一般會以為「process 越多，CPU 越忙」。但超過臨界點後，process 越多，大家越搶 frames，最後 CPU 不是更忙，而是更閒。因為大家都卡在 paging device I/O queue 裡，ready queue 反而空掉。

所以 Figure 10.20 的結論是：

```txt id="7kwglm"
要停止 thrashing，
不是增加 multiprogramming，
而是降低 multiprogramming。
```

也就是要把一些 process suspend / swap out，釋放 frames，讓留下來的 processes 至少能放下自己的 working set。

---

## 5. Local Replacement 可以限制 Thrashing，但不能完全解決

課本接著說，可以用 **local replacement algorithm** 或 **priority replacement algorithm** 限制 thrashing 的影響。

Local replacement 的意思是：某個 process 發生 page fault 時，只能從自己的 allocated frames 裡選 victim，不能偷別人的 frames。這樣做的好處是：如果 Process A thrashing，它只能害自己，不能把 Process B 的 working set 也換爆。

```txt id="zch4lf"
Global replacement：
Process A 可以搶 Process B 的 frame
→ thrashing 會擴散

Local replacement：
Process A 只能換自己的 frame
→ thrashing 被限制在 A 自己身上
```

但是 local replacement 不是萬靈丹。因為如果很多 processes 都 frames 不夠，它們還是會各自 page fault，大家還是會排隊等 paging device。也就是說，local replacement 可以防止互相搶 frames，但無法改變「總 memory 不夠」這個根本問題。

---

## 6. Locality Model：為什麼 Demand Paging 平常可以正常運作？

要理解 working set，必須先理解 **locality model（區域性模型）**。

Locality 的意思是：程式在一小段時間內通常只會集中使用少數幾個 pages。講義把 Principle of Locality 整理成：process 裡的 program/data references 會聚集在一起，短時間內只需要少數 pieces，因此 OS 可以根據近期歷史猜測未來會需要哪些 pieces。

例如：

```txt id="3vu0cp"
執行某個 function 時：
會使用該 function 的 instructions
會使用 local variables
會使用部分 global variables
會使用 stack 上的呼叫資訊
```

這些東西形成一個 locality。當 function 結束，process 可能離開這個 locality，進入下一個 locality。

所以一個 program 的執行不是平均散布在整個 address space，而是像這樣：

```txt id="v8aksv"
locality A → locality B → locality C → locality A ...
```

而且 localities 可能重疊。例如兩個 function 都會用到同一批 global data pages。

---

## 7. Figure 10.21：Locality in a memory-reference pattern

Figure 10.21 是一張 page reference pattern。x 軸是 **execution time**，y 軸是 **page numbers**。圖中的密集直線 / 點狀痕跡代表程式在某段時間內反覆 reference 某些 pages。

這張圖的重點不是看單一數值，而是看形狀：記憶體參考不是完全隨機亂跳，而是一段時間集中在某些 page numbers 區域。圖中標出的 (a)、(b) 就是不同時間點的 locality。

可以這樣理解：

```txt id="fkxmic"
在時間區段 (a)：
process 主要活躍在某一批 pages

到時間區段 (b)：
process 移動到另一批 pages

中間有些 pages 可能重疊
```

這張圖證明 locality 的存在。若 memory reference 完全隨機，那 demand paging 幾乎沒救，因為 OS 無法保留「近期會再用到」的 pages。但實際程式通常有 locality，所以 demand paging 才有機會成功。

---

## 8. Working-Set Model：用 Δ 定義「目前需要哪些 pages」

**Working-set model** 是用來預防 thrashing 的重要方法。它的核心想法是：不要亂猜 process 需要多少 frames，而是根據最近一段 reference history 估計 process 目前正在使用哪些 pages。

Working-set model 使用一個參數：

```txt id="uubsiw"
Δ = working-set window
```

意思是觀察最近 Δ 次 memory references。某個 page 如果在最近 Δ 次 references 中出現過，就屬於目前的 working set。

定義可以寫成：

```txt id="a06eq1"
WS(t) =
在時間 t 前最近 Δ 次 page references 中，
有被 reference 到的 page 集合。
```

也就是：

```txt id="ofm90e"
Working set =
最近一段時間內真的被用到的 pages。
```

---

## 9. Figure 10.22：Working-set model 範例

Figure 10.22 給了一串 page reference：

```txt id="grfmeo"
... 2 6 1 5 7 7 7 5 1 6 2 3 4 1 2 3 4 4 3 4 4 4 1 3 2 3 4 4 3 4 4 4 ...
```

圖中用 Δ 當 working-set window。不同時間點 t，往回看最近 Δ 次 references，得到不同 working set。

圖上標示：

```txt id="ywbu3j"
WS(t1) = {1, 2, 5, 6, 7}
```

意思是，在時間 t1 往回看 Δ 次 reference，出現過的 page 有 1、2、5、6、7。

另一個時間點：

```txt id="2el4mk"
WS(t2) = {3, 4}
```

意思是，在時間 t2 往回看 Δ 次 reference，最近只活躍使用 page 3 和 page 4。

這正好展示 locality 轉換。前面可能在一個 locality，常用 1、2、5、6、7；後面進入另一個 locality，只常用 3、4。

---

## 10. Δ 太小或太大會怎樣？

Working-set model 的準確度取決於 Δ 的選擇。

如果 Δ 太小：

```txt id="4yt1s0"
只看到很短的 reference history
→ 可能漏掉同一個 locality 裡稍微晚一點才會用到的 pages
→ working set 被估太小
→ process 分到 frames 太少
→ page fault rate 上升
```

如果 Δ 太大：

```txt id="hx5tr9"
看到太長的 reference history
→ 可能把好幾個 localities 混在一起
→ working set 被估太大
→ process 佔太多 frames
→ 系統可同時執行的 processes 變少
```

所以 Δ 不是越大越好，也不是越小越好。它要大到足以包含目前 locality，又不能大到把很多不同 localities 混在一起。

---

## 11. WSSᵢ 與 D：判斷系統會不會 Thrashing 的公式

每個 process 都有自己的 working set。課本用：

```txt id="q7jfqr"
WSS_i = working-set size of process P_i
```

也就是 process `P_i` 目前 working set 的大小，換句話說，它至少需要多少 frames 才能順利執行目前 locality。

整個系統的 total demand for frames 是：

```txt id="yltztr"
D = Σ WSS_i
```

其中：

```txt id="tp6wz9"
D = 所有 processes 目前總共需要的 frames 數量
m = 系統可用的 physical frames 數量
```

判斷邏輯：

```txt id="6nt10k"
如果 D <= m：
系統 frames 足夠
→ processes 的 working sets 可以放進 RAM
→ 不容易 thrashing

如果 D > m：
總需求超過可用 frames
→ 至少有些 process working set 放不下
→ thrashing 會發生
```

這是 10.6 的超級核心公式。它把 thrashing 的原因講得很精準：不是單純「process 很多」就 thrashing，而是 **所有 active processes 的 working sets 總和超過 physical memory 能容納的 frames**。

---

## 12. Working-Set Strategy：如何用 Working Set 防止 Thrashing？

當 OS 估計出每個 process 的 working set 後，就可以用它來做 frame allocation。

策略如下：

```txt id="ly9642"
1. OS 監控每個 process 的 working set。

2. 分配給每個 process 至少 WSS_i 個 frames。

3. 計算 D = ΣWSS_i。

4. 如果 D <= m：
   代表 frames 夠，process 可以繼續執行。

5. 如果 D > m：
   代表記憶體壓力太大。
   OS 應該 suspend / swap out 某些 process。
   被 suspend 的 process 釋放 frames 給其他 processes。
```

這樣做的精神是：與其讓全部 processes 都餓死，不如暫停一些 processes，讓剩下的 processes 吃飽 frames，至少能正常前進。

---

## 13. Figure：Working Sets and Page-Fault Rates 藍色方塊

藍色方塊說明 working set 和 page-fault rate 的關係。

當 process 從一個 locality 轉移到新的 locality 時，一開始新的 locality 的 pages 還不在 RAM，所以會產生一波 page faults。隨著新 locality 的 pages 逐漸載入 RAM，page-fault rate 會下降。

所以圖形會像：

```txt id="u7szq2"
進入新 locality
→ page fault rate 上升形成 peak
→ working set 被載入 RAM
→ page fault rate 下降
→ 再進入下一個 locality
→ 又出現下一個 peak
```

這個圖想說明：page fault rate 的尖峰不一定代表永久 thrashing。有時候只是 process 正在切換 locality。但如果 memory 足夠，尖峰過後會下降；如果 memory 不足，page fault rate 會長期維持高檔，才是真正危險。

---

## 14. Working Set 的實作近似：Timer + Reference Bit

理論上的 working set 要在每次 memory reference 時都維護最近 Δ 次 references，成本太高。所以課本提出近似做法：用 fixed-interval timer interrupt 加上 reference bit。

例如：

```txt id="dvrmdu"
Δ = 10,000 references
timer interrupt 每 5,000 references 發生一次
```

每次 timer interrupt 時，OS 會複製並清除每個 page 的 reference bit。之後如果 page fault 或需要判斷 working set，就可以看：

```txt id="j3kc69"
目前 reference bit
+ 前兩次 interval 記錄下來的 reference history bits
```

如果這些 bits 中至少有一個是 1，就推測該 page 在最近約 10,000 到 15,000 references 中被使用過，因此視為 working set 的一部分。

但是這只是近似，因為 timer interval 中間的精確 reference 時間不知道。若要更準，可以增加 history bits、提高 interrupt frequency，例如更多 bits、每 1,000 references interrupt 一次。但代價是 interrupt overhead 更高。課本也明確提到，更多 history bits 與更頻繁 interrupts 可降低不確定性，但會提高服務 interrupt 的成本。

---

## 15. 10.6.3 Page-Fault Frequency：更直接的控制方法

Working-set model 很成功，但實作上有點笨重。因此課本又介紹 **Page-Fault Frequency，PFF**，用 page fault rate 直接控制 thrashing。

PFF 的核心想法：

```txt id="mnvu1e"
page-fault rate 太高：
代表 process frames 不夠
→ 給它更多 frames

page-fault rate 太低：
代表 process 可能 frames 太多
→ 拿走一些 frames
```

所以 PFF 不直接估 working set，而是看症狀：page fault rate。

---

## 16. Figure 10.23：Page-Fault Frequency 圖解

Figure 10.23 的 x 軸是 **number of frames**，y 軸是 **page-fault rate**。曲線往下代表：process 拿到 frames 越多，page fault rate 通常越低。

圖上有兩條水平線：

```txt id="g0wgz6"
upper bound
lower bound
```

控制邏輯如下：

```txt id="gcokpo"
如果 page-fault rate > upper bound：
代表 page fault 太頻繁
→ process frames 不夠
→ increase number of frames

如果 page-fault rate < lower bound：
代表 page fault 很少
→ process 可能 frames 太多
→ decrease number of frames

如果 page-fault rate 介於上下界之間：
代表狀態可接受
→ frame allocation 不變
```

這比 working set 更直接，因為 thrashing 本身的症狀就是 high page-fault rate。課本說 PFF 採取更直接方法：控制 page-fault rate；當 page-fault rate 太高，process 需要更多 frames；太低，process 可能有太多 frames。

---

## 17. PFF 如果沒有 free frames 怎麼辦？

如果某個 process page-fault rate 超過 upper bound，OS 想給它更多 frames，但 free-frame list 沒有 frame，怎麼辦？

這時候就不能硬撐。課本說可能需要選一個 process swap out 到 backing store。被 swap out 的 process 釋放 frames，這些 frames 再分給 page-fault rate 太高的 processes。

也就是：

```txt id="r2o052"
PFF 偵測到某 process frames 不夠
        ↓
想增加 frames
        ↓
但沒有 free frames
        ↓
swap out / suspend 某些 process
        ↓
釋放 frames
        ↓
分配給仍在執行的 process
```

這跟 Figure 10.20 的結論一致：發生 thrashing 時，要降低 degree of multiprogramming，而不是提高。

---

## 18. Working Set vs. PFF 比較

兩者都想避免 thrashing，但思路不同。

```txt id="o4zfgs"
Working-set model：
直接估計 process 最近真的用到哪些 pages。
根據 WSS_i 分配 frames。
重點是「工作集合大小」。

Page-Fault Frequency：
不直接估 working set。
直接觀察 page fault rate。
高於 upper bound 就加 frames；
低於 lower bound 就減 frames。
重點是「缺頁率是否合理」。
```

Working set 比較理論化，可以較精準掌握 locality；PFF 比較直覺、實務控制感比較強。

---

## 19. 10.6.4 Current Practice：現代實務做法

課本最後說，實務上 thrashing 和 swapping 對效能影響非常差。現在最佳實務通常是：**盡量提供足夠 physical memory，讓所有 working sets 都能放進 RAM，除非極端情況，否則避免 swapping。**

也就是說，理論上可以用 working set、PFF、replacement policy 來處理 thrashing；但真正的現代系統設計觀點是：

```txt id="zut2v6"
能加 RAM 就加 RAM。
不要讓系統進入頻繁 swapping。
```

從 smartphones 到 servers，足夠的 physical memory 通常能提供最好的使用者體驗。這也呼應 10.2 的 EAT 例子：page fault / storage I/O 比 memory access 慢太多，一旦頻繁發生，效能會直接崩。

---

## 20. 整個 10.6 的完整流程圖

```txt id="ylm0tk"
Demand paging 正常運作
        ↓
Process 只需要目前 locality 的 pages
        ↓
如果 frames 足夠容納 working set
        ↓
page fault rate 低，CPU utilization 高

但如果：
frames 不足
        ↓
working set 放不下
        ↓
page fault rate 高
        ↓
process 等 paging I/O
        ↓
CPU utilization 下降
        ↓
OS 誤以為 process 太少
        ↓
增加 degree of multiprogramming
        ↓
frames 更不夠
        ↓
更多 page faults
        ↓
Thrashing
```

解法方向：

```txt id="s370nw"
1. 用 local / priority replacement 限制互相搶 frames。
2. 用 working-set model 確保每個 process 至少有 WSS_i frames。
3. 若 D = ΣWSS_i > m，暫停一些 process，降低 multiprogramming。
4. 用 page-fault frequency 直接監控 page fault rate。
5. 實務上提供足夠 physical memory，盡量避免 swapping。
```

---

## 21. 考試重點整理

```txt id="fvbwf2"
Thrashing 定義：
Process 花在 paging 的時間比 executing 還多。

Thrashing 根本原因：
Process 沒有足夠 frames 容納 working set。

CPU utilization 陷阱：
CPU utilization 下降時，盲目增加 multiprogramming 會讓 thrashing 更嚴重。

Figure 10.20：
degree of multiprogramming 增加，一開始 CPU utilization 上升；
超過臨界點後 thrashing 發生，CPU utilization 急速下降。

Locality：
Process 在短時間內集中使用一批 pages。
Process 會從一個 locality 移動到另一個 locality，localities 可以重疊。

Figure 10.21：
Memory-reference pattern 顯示 references 會集中在某些 page numbers 區域，證明 locality。

Working set：
最近 Δ 次 references 中被用到的 pages 集合。

Figure 10.22：
WS(t1) = {1,2,5,6,7}
WS(t2) = {3,4}
表示不同時間點 process 位於不同 locality。

WSS_i：
Process P_i 的 working-set size。

D = ΣWSS_i：
整個系統對 frames 的總需求。

判斷 thrashing：
若 D > m，總 working-set demand 超過 available frames，會 thrashing。

PFF：
Page-fault frequency 用上下界控制 frame allocation。
太高加 frames，太低減 frames。

Current practice：
最佳做法是提供足夠 physical memory，避免 swapping / thrashing。
```

最終一句話：

**Thrashing 的本質不是 page replacement algorithm 算錯，而是 memory allocation 失衡。當系統同時活躍的 processes 需要的 working sets 總量超過 RAM 可提供的 frames，所有 process 就會開始互相換頁、等待 disk I/O，CPU utilization 反而暴跌。解法不是再塞更多 process，而是降低 multiprogramming、保證 working set、控制 page-fault rate，或實務上直接提供足夠 physical memory。**
