---
title: chapter 11 Mass-Storage Structure

---

# 11.1 ~ 11.5 11.7
![Ch11-10](https://hackmd.io/_uploads/HJ65DGvbzx.jpg)
![Ch11-11](https://hackmd.io/_uploads/rJAawMPWfg.jpg)
![Ch11-13](https://hackmd.io/_uploads/BJ80wMwWGe.jpg)
![Ch11-14](https://hackmd.io/_uploads/ByO4_GD-fl.jpg)
![Ch11-15](https://hackmd.io/_uploads/SJAVuzvZfl.jpg)
![Ch11-16](https://hackmd.io/_uploads/SJ7SdGDZMg.jpg)
![Ch11-17](https://hackmd.io/_uploads/Sk6H_zvWzx.jpg)
![Ch11-19](https://hackmd.io/_uploads/r1Q8OzDWfg.jpg)
![Ch11-22](https://hackmd.io/_uploads/Sy98uGPbGl.jpg)
![Ch11-23](https://hackmd.io/_uploads/rkMwOzD-fx.jpg)
![Ch11-24](https://hackmd.io/_uploads/BJPP_zD-Ml.jpg)
![Ch11-25](https://hackmd.io/_uploads/S19PuMDWGe.jpg)
![Ch11-26](https://hackmd.io/_uploads/S1yduzwZfl.jpg)
![Ch11-27](https://hackmd.io/_uploads/BJQ_OMvWfe.jpg)
![Ch11-28](https://hackmd.io/_uploads/SyddOMP-Ge.jpg)
![Ch11-29](https://hackmd.io/_uploads/B12_uGPZfl.jpg)
![Ch11-30](https://hackmd.io/_uploads/rkxFuzw-ze.jpg)
![Ch11-31](https://hackmd.io/_uploads/r1OKuGP-Gx.jpg)
![Ch11-32](https://hackmd.io/_uploads/S1htOzwZfx.jpg)
![Ch11-33](https://hackmd.io/_uploads/rJWc_zPbGl.jpg)
# 11.6 Swap-Space Management
## 11.37
這頁在講 **Swap-Space Management（交換空間管理）**，也就是作業系統怎麼使用硬碟上的一塊空間，當作主記憶體不足時的延伸區域。

**Swap space（交換空間）** 是 virtual memory 的一部分。當 main memory / physical memory 不夠時，OS 可以把某些暫時不用的 pages 從 RAM 移到 disk 上的 swap space；之後如果又需要那些 pages，再從 swap space 讀回 RAM。簡單說：

```text
RAM 不夠
→ 把暫時不用的 pages 丟到 swap space
→ 需要時再讀回 RAM
```

不過現在實體記憶體容量變大，所以傳統大量 swapping 比以前少見。

---

Swap space 可以有兩種存在方式。第一種是放在一般 **file system（檔案系統）** 裡，也就是用一個 swap file 當交換空間。第二種是放在獨立的 raw disk partition，也就是專門切一塊磁碟分割區給 swap 用。通常獨立 partition 比較直接，少了 file system 的管理負擔，效率可能比較好。

**Raw partition** 指的是沒有一般檔案系統格式的磁碟分割區，OS 直接拿它做 swap 管理。

---

投影片接著比較不同系統的 swap-space management。

**4.3BSD** 的做法是：process 一開始建立時，就先替它配置 swap space。這些 swap space 會保留給 process 的 **text segment** 和 **data segment**。

**Text segment** 指的是程式碼區，也就是 program instructions。
**Data segment** 指的是資料區，例如全域變數、靜態資料等。

這種做法比較保守，因為 process 一開始就先準備好 swap 空間，但缺點是可能會預留太多沒用到的空間。

---

**Swap map** 是 kernel 用來追蹤 swap space 使用情況的資料結構。它記錄哪些 swap 區塊已經被使用、哪些還是空的，以及某個 page 被換出時放在 swap space 的哪個位置。

可以把 swap map 想成 swap space 的配置表：

```text
哪一段 swap 已使用
哪一段 swap 還空著
某個 page 對應到 swap 裡哪個位置
```

---

**Solaris 2** 的策略比較晚分配 swap space。它不是 virtual memory page 一建立就立刻配置 swap，而是等到某個 **dirty page** 被迫離開 physical memory 時，才分配 swap space。

**Dirty page** 指的是已經在 RAM 中被修改過的 page。因為內容已經和 disk 上原本的版本不同，所以不能直接丟掉，必須找地方保存。

Solaris 2 的邏輯是：

如果是 file data，而且還沒要求寫回 file system，就先寫到 swap space。
如果是其他 dirty pages，而且沒有其他原始位置可以回去，也寫到 swap space。
如果是 text segment pages，通常可以直接丟掉；因為程式碼本來就在 executable file 裡，需要時再從 file system 讀回來即可。

---

最後兩點是在問 swap space 不夠怎麼辦。

如果系統的 swap space 用完，新的 dirty page 可能無法被換出，process 配置記憶體也可能失敗，嚴重時 OS 可能需要終止某些 process。

有些系統允許 **multiple swap spaces（多個交換空間）**，也就是可以同時使用多個 swap partitions 或 swap files，增加可用的交換空間容量。

---

重點整理：

**Swap space 是 disk 上用來延伸 main memory 的區域。OS 會把暫時不用或被迫換出的 pages 放到 swap space。Kernel 用 swap maps 追蹤 swap 使用情況。早期 BSD 會在 process 開始時先配置 swap；Solaris 2 則偏向等 dirty page 真的要被換出時才配置 swap。Text segment 通常可直接丟棄並從原檔重讀，而 dirty anonymous pages 通常需要寫入 swap。**

## 11.38
![image](https://hackmd.io/_uploads/SyPPtzP-Gx.png)
這頁在講 **Linux 系統中用來管理 swap 的資料結構**，重點是 **swap area、page slot、swap partition / swap file、swap map** 之間的關係。

**Swap area（交換區域）** 指的是整塊可以拿來做 swapping 的磁碟空間。它可能來自一個 **swap partition（交換分割區）**，也可能來自一個 **swap file（交換檔案）**。當 RAM 裡某些 page 暫時不用、或需要被換出時，OS 可以把它們放到這個 swap area 裡。

**Page slot（頁槽）** 是 swap area 裡的基本單位。每個 page slot 大小通常對應一個 memory page。也就是說，RAM 裡的一個 page 被 swap out 到 disk 時，會放進 swap area 裡的一個 page slot。

圖上方那排長條就是 swap area，被切成一格一格的 page slots。每一格都可以放一個被換出的 page。

下面的 **swap map** 是 Linux kernel 用來追蹤 swap area 使用狀態的表。它會記錄每個 page slot 目前有沒有被使用，以及被多少地方 reference。

圖中的 swap map 數字：

```text
1   0   3   0   1
```

意思大概是：

`0`：這個 page slot 是空的，沒有被使用。
`1`：這個 page slot 正在被一個 page 使用。
`3`：這個 page slot 被多個地方 reference，可能代表有共享或 copy-on-write 相關情況。

所以圖中灰色 page slot 對應 swap map 的 `0`，代表空閒；藍色 page slot 對應 `1` 或 `3`，代表正在被使用。

這張圖的核心概念是：

**Linux 把 swap partition 或 swap file 切成很多 page slots，並用 swap map 記錄每個 slot 的使用情況。當 page 被 swap out 時，kernel 會找一個空的 page slot 放進去；當 page 被 swap in 回 RAM 或不再需要時，swap map 會更新，釋放對應的 slot。**

考試版可寫：

**Linux manages swap space by dividing a swap partition or swap file into page slots. The kernel uses a swap map to record whether each page slot is free or in use, and how many references it has. A value of 0 means the slot is free; a positive value means the slot is occupied or referenced.**

# 11.8 RAID Structure
## 11.39
這頁是 **RAID Structure** 的前言，重點是在說：**用多顆磁碟合作，透過 redundancy 提高可靠性，也可能提升效能。**

**RAID = Redundant Array of Inexpensive Disks**，中文常翻成「廉價磁碟冗餘陣列」；現在也常寫成 **Redundant Array of Independent Disks**，「獨立磁碟冗餘陣列」。核心概念是把多顆 disks 組成一個邏輯上的儲存系統，利用 **redundancy（冗餘）** 來避免單顆磁碟壞掉就造成資料遺失。

**Redundancy** 是「多放一些額外資訊」的意思。可能是完整複製一份資料，例如 mirroring；也可能是放 parity 校驗資訊，例如 RAID 5 / RAID 6。目的都是：某顆 disk 故障時，系統還能從其他 disks 恢復資料。

---

**MTTF = Mean Time To Failure**，中文是「平均故障時間」。它代表一個元件平均多久會壞一次。RAID 透過多顆磁碟的冗餘設計，可以讓整個儲存系統的資料可靠性提高，也就是增加資料真正遺失前的平均時間。

**MTTR = Mean Time To Repair**，中文是「平均修復時間」。它代表磁碟壞掉後，從發現故障、更換磁碟，到資料重建完成所需的平均時間。這段時間很危險，因為如果 RAID 正在 degraded mode，這時候又壞另一顆相關磁碟，就可能造成資料遺失。

**Mean Time To Data Loss** 可以理解成 **MTTDL = Mean Time To Data Loss**，中文是「平均資料遺失時間」。它不是單顆磁碟壽命，而是整個 RAID 系統平均多久可能真的發生不可恢復的資料遺失。它會受到 **MTTF** 和 **MTTR** 影響：磁碟越不容易壞，MTTDL 越長；壞掉後修越快，暴露在危險狀態的時間越短，MTTDL 也越長。

---

投影片中間的 mirrored disks 例子是在說 **mirroring（鏡像）** 的可靠性。

Mirrored disks 可以想成兩顆磁碟存一樣的資料。只壞一顆時，資料還在另一顆上，所以不會立刻 data loss。真正危險的是：第一顆壞掉後，在修好前，第二顆也壞掉。

投影片使用的估算式是：

```text
Mean time to data loss = MTTF² / (2 × MTTR)
```

若假設：

```text
MTTF = 100,000 hours
MTTR = 10 hours
```

則：

```text
MTTDL = 100,000² / (2 × 10)
      = 10,000,000,000 / 20
      = 500,000,000 hours
```

約等於：

```text
57,000 years
```

這個例子想表達的是：**mirroring 可以大幅提高資料可靠性**。因為資料遺失需要兩顆鏡像磁碟在修復期間內同時故障，機率比單顆磁碟故障低很多。

---

**NVRAM = Non-Volatile Random Access Memory**，中文是「非揮發性隨機存取記憶體」。它的特點是斷電後資料不會消失。RAID 系統常搭配 NVRAM 來改善寫入效能。

原因是寫入 RAID 時，有時需要同時更新多顆 disk 或 parity。如果每次都等所有 disk 寫完才回覆 host，速度會變慢。使用 NVRAM 後，controller 可以先把寫入資料安全地記在 NVRAM 裡，再回覆 host「寫入完成」，之後再慢慢寫到 disks。因為 NVRAM 斷電不會丟資料，所以比普通 RAM 安全。

---

最後一句說：

**Several improvements in disk-use techniques involve the use of multiple disks working cooperatively**

意思是後面要介紹的 RAID 等技術，核心都是讓多顆 disks 合作。合作方式可能是：

把資料分散到多顆 disks，提高平行讀寫效能。
把資料複製到多顆 disks，提高可靠性。
用 parity 儲存額外資訊，在磁碟故障時重建資料。

總結一句：

**RAID 是利用多顆 disks 組成的儲存架構，透過 redundancy 提高可靠性，增加 MTTF / MTTDL，並透過 NVRAM、平行存取、mirroring、parity 等技術改善效能與資料安全性。**

## 11.42
這頁在延續 **RAID = Redundant Array of Inexpensive Disks**，也常寫成 **Redundant Array of Independent Disks**。重點是：RAID 把多顆 disks 組成一個邏輯儲存單位，透過 **striping、mirroring、parity、hot spare、replication** 等方式提高效能與可靠性。

**Disk striping（磁碟條帶化）** 是把資料切成小區塊，分散存到多顆 disks 上。對外看起來像一個 storage unit，但內部其實是多顆 disks 一起工作。好處是可以平行讀寫，提高效能。例如一份大檔案被切成 block 0、block 1、block 2、block 3，分別放到不同磁碟，讀取時多顆磁碟可以同時工作。

**Mirroring / Shadowing（鏡像，RAID 1）** 是把同一份資料完整複製到另一顆 disk。也就是兩顆 disks 存一樣的內容。好處是可靠性高，一顆 disk 壞掉時，另一顆還有完整資料；缺點是容量成本高，因為兩顆磁碟只能提供一份有效容量。

**RAID 1+0** 又叫 **RAID 10，striped mirrors**。意思是先做 mirroring，再做 striping。簡單講，就是先把 disks 兩兩鏡像成多組 mirror pairs，再把資料 striping 到這些 mirror pairs 上。它同時有高效能與高可靠性，是很常用的企業級配置。

**RAID 0+1** 是 **mirrored stripes**。意思是先做 striping，再把整組 stripe 做鏡像。它看起來也結合 RAID 0 和 RAID 1，但容錯性通常不如 RAID 1+0。RAID 1+0 比較常被偏好，因為壞一顆 disk 後，只影響其中一組 mirror pair；RAID 0+1 則可能因為一顆 disk 壞掉，使整組 stripe 失效。

**Block-interleaved parity（區塊交錯同位元 / parity）** 是 RAID 4、RAID 5、RAID 6 的核心概念。它不是像 RAID 1 那樣完整複製一份資料，而是額外儲存 **parity information（同位元資訊）**。Parity 可以用來在某顆 disk 壞掉時，根據其他 disks 的資料重建遺失內容。這種方法比 mirroring 省容量，因為不用完整複製所有資料。

RAID 4 使用專門的一顆 parity disk。RAID 5 把 parity 分散到不同 disks，避免單一 parity disk 成為瓶頸。RAID 6 則有兩份 parity，可以容忍兩顆 disks 同時故障。

投影片後面提到，**RAID within a storage array can still fail if the array fails**。意思是 RAID 只能處理陣列內部某些 disks 故障，但如果整台 storage array 的 controller、電源、背板或整個機櫃出問題，RAID 本身還是不夠。所以大型系統常做 **replication（複寫）**，把資料自動複製到另一台 storage array，避免單一陣列故障造成服務中斷或資料遺失。

**Hot-spare disk（熱備援磁碟）** 是預先放在 storage array 裡、但平常不分配給正常資料使用的備用磁碟。一旦某顆 disk 壞掉，系統可以自動啟用 hot spare，並把遺失資料 rebuild 到這顆備用磁碟上。這可以縮短系統處於危險狀態的時間，也就是降低資料遺失風險。

總結：**RAID 透過多顆 disks 合作提升效能與可靠性。Striping 提升平行讀寫效能；mirroring 提供完整複本；parity 用較少 redundancy 達成容錯；hot spare 可在磁碟故障後自動接手；replication 則用來防止整個 storage array 故障。**


## 11.43
### 架構
![image](https://hackmd.io/_uploads/S1-lsMDZfl.png)
這張圖在列 **RAID Levels（RAID 層級）**。
**RAID** 常見全稱是 **Redundant Array of Independent Disks**，也可見舊稱 **Redundant Array of Inexpensive Disks**。核心意思是：把多顆磁碟組合成一個儲存系統，透過不同配置方式提升效能或可靠性。

---


### (a) RAID 0：Non-redundant Striping

**RAID 0 = non-redundant striping**，中文可以叫 **無冗餘條帶化**。

它的做法是把資料切成一塊一塊的 **stripe / strip（條帶）**，然後分散寫到多顆 disks 上。例如有 4 顆 disks，要寫入資料 A、B、C、D、E、F、G、H，可能會變成：

```text
Disk 0：A, E
Disk 1：B, F
Disk 2：C, G
Disk 3：D, H
```

這樣讀寫大檔案時，多顆 disks 可以同時工作，所以效能很好，尤其是連續讀寫時很快。

但是 RAID 0 的大問題是：**沒有 redundancy（冗餘）**。也就是沒有備份、沒有 parity、沒有容錯能力。只要其中一顆 disk 壞掉，分散在那顆 disk 上的資料就消失，整個 RAID 0 的資料通常都會壞掉。

所以 RAID 0 的特色是：

```text
優點：速度快、容量利用率高
缺點：沒有容錯，一顆 disk 壞掉就可能整組資料毀掉
用途：追求速度、不重視資料安全的場合
```

一句話記法：**RAID 0 只有 striping，沒有 redundancy，所以快但不安全。**

---

### (b) RAID 1：Mirrored Disks

**RAID 1 = mirrored disks**，中文是 **鏡像磁碟**。

它的做法是把同一份資料完整寫到兩顆 disks 上。也就是每一顆主要 disk 都有一顆 mirror disk 保存相同內容。

例如資料 A、B、C、D 寫入時：

```text
Disk 0：A, B, C, D
Disk 1：A, B, C, D   ← Disk 0 的鏡像
```

如果 Disk 0 壞掉，Disk 1 還有完整資料，所以系統可以繼續運作。這就是 RAID 1 的可靠性來源。

RAID 1 的讀取效能通常可以比較好，因為兩顆 disks 都有同樣資料，讀取時可以從其中一顆讀，也可以分散讀取請求。但寫入時，資料必須寫到兩顆 disks，因為兩邊要保持一致，所以寫入成本比較高。

RAID 1 的最大缺點是容量利用率低。因為資料完整複製一份，所以如果有兩顆 1 TB disks，實際可用容量只有 1 TB，不是 2 TB。

所以 RAID 1 的特色是：

```text
優點：可靠性高，一顆 disk 壞掉資料仍在
缺點：容量成本高，因為要完整複製資料
用途：重視資料安全，例如系統碟、重要資料儲存
```

一句話記法：**RAID 1 是完整複製一份資料，安全但容量浪費一半。**

---

### (c) RAID 2：Memory-style Error-Correcting Codes

**RAID 2 = memory-style error-correcting codes**，中文可理解成 **類似記憶體 ECC 的錯誤更正碼 RAID**。

這個比較少見，現代系統幾乎不常用，但課本會介紹它的概念。

RAID 2 的想法是：資料不是單純分散到 disks，而是用類似記憶體中的 **ECC = Error-Correcting Code（錯誤更正碼）** 來保護資料。

圖中可以看到前面幾顆 disks 是資料 disks，後面幾顆標成 `P` 的 disks 是用來存放錯誤更正碼或 parity-like information 的 disks。

它的操作概念是：

```text
資料被切成 bit-level 或 very small unit
分散放到多顆 data disks
額外計算 error-correcting code
把 ECC 資訊存到額外的 check disks
```

當某顆 disk 發生錯誤時，系統可以利用 ECC 資訊偵測並更正錯誤，有點像記憶體 ECC 可以偵測和修正 bit error。

不過 RAID 2 有幾個問題。第一，它需要多顆額外 disks 來存 ECC，成本不低。第二，現代磁碟本身已經有內建錯誤偵測與更正能力，所以 RAID 2 的價值變小。第三，它的設計比較複雜，因此實務上很少使用。

所以 RAID 2 的特色是：

```text
優點：可以用 error-correcting code 偵測與修正錯誤
缺點：需要額外 check disks，架構複雜，現代很少使用
用途：主要是理論介紹，實務不常見
```

一句話記法：**RAID 2 是把 ECC 記憶體那套錯誤更正概念搬到磁碟陣列上，但實務上很少用。**

---

### (a)～(c) 快速比較

```text
RAID 0：
striping only
速度快，沒有備援，一顆壞掉就危險

RAID 1：
mirroring
資料完整複製，可靠性高，但容量成本高

RAID 2：
memory-style ECC
用錯誤更正碼保護資料，概念上可修錯，但實務少用
```

最重要的是：**RAID 0 提升效能但不提供可靠性；RAID 1 提升可靠性但犧牲容量；RAID 2 使用 ECC 思想做錯誤修正，但不是現代常用 RAID。**

### (d) RAID 3：Bit-interleaved Parity

**RAID 3 = bit-interleaved parity**，中文可理解成 **位元交錯同位元 RAID**。

它的做法是把資料切得非常細，通常到 **bit-level** 或 byte-level，然後分散到多顆 data disks 上，再用一顆專門的 parity disk 儲存 parity。

例如有 4 顆 data disks 加 1 顆 parity disk：

```text
Disk 0：資料 bit 0
Disk 1：資料 bit 1
Disk 2：資料 bit 2
Disk 3：資料 bit 3
Disk 4：Parity
```

每次讀寫資料時，通常多顆 disks 會一起動作。因為資料被切得很細，一筆資料會橫跨很多顆 disks，所以 RAID 3 適合大量連續資料傳輸，例如大檔案串流。

如果其中一顆 data disk 壞掉，系統可以用其他 data disks 加上 parity disk 重建遺失的 bit。Parity 通常可以用 XOR 概念理解：

```text
P = D0 ⊕ D1 ⊕ D2 ⊕ D3
```

如果 Disk 2 壞掉：

```text
D2 = D0 ⊕ D1 ⊕ D3 ⊕ P
```

RAID 3 的缺點是那顆專門的 parity disk 會變成瓶頸。因為每次寫入資料時，都需要更新 parity disk，所以 parity disk 壓力很大。

一句話記法：**RAID 3 把資料切到很細的 bit / byte level，分散到多顆 disks，並用一顆專門 parity disk 做容錯。**

---

### (e) RAID 4：Block-interleaved Parity

**RAID 4 = block-interleaved parity**，中文可理解成 **區塊交錯同位元 RAID**。

RAID 4 和 RAID 3 很像，都有一顆專門的 parity disk。差別在於 RAID 4 的資料切割單位比較大，是以 **block** 為單位，而不是 bit-level。

也就是資料會像這樣分散：

```text
Disk 0：Block A
Disk 1：Block B
Disk 2：Block C
Disk 3：Block D
Disk 4：Parity block
```

RAID 4 的好處是：因為資料是 block-level 分散，不同小型讀取可以只讀某一顆 data disk，不一定每次都要全部 disks 一起動。因此隨機讀取比 RAID 3 更有彈性。

但是 RAID 4 還是有一個明顯問題：**所有 parity 都集中在同一顆 parity disk**。所以只要有寫入，尤其是很多小寫入，就會一直更新 parity disk，造成 parity disk 成為 bottleneck。

一句話記法：**RAID 4 用 block-level striping 加上一顆專門 parity disk；比 RAID 3 彈性高，但 parity disk 容易成為寫入瓶頸。**

---

### (f) RAID 5：Block-interleaved Distributed Parity

**RAID 5 = block-interleaved distributed parity**，中文可理解成 **區塊交錯分散式同位元 RAID**。

RAID 5 是 RAID 4 的改良版。它一樣使用 block-level striping，也一樣使用 parity，但最大差別是：

**RAID 5 不把 parity 固定放在同一顆 disk，而是把 parity 分散到所有 disks 上。**

所以圖中每顆 disk 裡都可以看到 `P`，代表 parity block 會輪流分布在不同 disks。

例如：

```text
Stripe 0：D0  D1  D2  P
Stripe 1：D3  D4  P   D5
Stripe 2：D6  P   D7  D8
Stripe 3：P   D9  D10 D11
```

這樣做的好處是避免 RAID 4 的 parity disk bottleneck。因為 parity 寫入壓力被分散到多顆 disks，整體寫入效能比較平均。

RAID 5 可以容忍 **一顆 disk 故障**。如果某顆 disk 壞掉，可以用其他 disks 的 data blocks 和 parity blocks 把資料重建回來。

不過 RAID 5 不能同時容忍兩顆 disks 壞掉。如果在重建期間又壞第二顆，就可能 data loss。

一句話記法：**RAID 5 是 block-level striping 加 distributed parity，可容忍一顆 disk failure，效能與容量利用率比 RAID 1 更平衡。**

---

### (g) RAID 6：P + Q Redundancy

**RAID 6 = P + Q redundancy**，中文可理解成 **雙重同位元冗餘 RAID**。

RAID 6 可以看成 RAID 5 的加強版。RAID 5 每個 stripe 只有一份 parity；RAID 6 每個 stripe 有兩份獨立的 redundancy information，通常稱為：

```text
P parity
Q parity
```

所以圖中每一列會有兩種 parity 資訊。這代表 RAID 6 可以容忍 **兩顆 disks 同時故障**。

RAID 6 的操作概念是：

```text
資料被切成 blocks 分散到多顆 disks
每一組資料額外產生 P 和 Q 兩份校驗資訊
P、Q 也分散存放在不同 disks
```

如果一顆 disk 壞掉，RAID 6 可以重建。
如果兩顆 disks 同時壞掉，RAID 6 仍然可以靠 P 和 Q 重建。

RAID 6 的優點是可靠性比 RAID 5 高，非常適合大容量磁碟陣列。因為磁碟容量越大，重建時間越久；在重建期間第二顆 disk 壞掉的風險也越高，所以 RAID 6 比 RAID 5 更安全。

缺點是寫入成本比較高。因為每次寫入時要更新兩份 parity，計算和 I/O 成本都比 RAID 5 高。而且容量也要犧牲兩顆 disks 的等效空間給 P 和 Q。

一句話記法：**RAID 6 是 RAID 5 的加強版，使用 P + Q 兩份 parity，可容忍兩顆 disks 同時故障，但寫入成本較高。**

---

### (d)～(g) 快速比較

```text
RAID 3：
bit-interleaved parity
資料切很細，使用專門 parity disk
適合大筆連續傳輸，但 parity disk 可能成為瓶頸

RAID 4：
block-interleaved parity
資料以 block 分散，使用專門 parity disk
隨機讀較好，但寫入時 parity disk 還是瓶頸

RAID 5：
block-interleaved distributed parity
parity 分散到所有 disks
可容忍一顆 disk 故障，是常見平衡方案

RAID 6：
P + Q redundancy
有兩份 parity
可容忍兩顆 disks 故障，但寫入成本更高
```

最重要的演進方向是：

```text
RAID 3：資料切很細 + 固定 parity disk
RAID 4：資料改成 block + 固定 parity disk
RAID 5：block + parity 分散
RAID 6：block + 兩份分散 parity
```

所以從 RAID 3 到 RAID 6，可以看成逐步改善 **寫入瓶頸** 和 **容錯能力** 的過程。

## 11.44
### 操作
![image](https://hackmd.io/_uploads/S1eE2GwWMe.png)
### RAID 0+1：先 Stripe，再 Mirror

**RAID 0+1** 可以理解成：

**先做 RAID 0，再做 RAID 1。**
也就是先把多顆 disks 做成一組 **striped set**，然後再把整組 striped set 複製一份。

RAID 0 的意思是 **striping（條帶化）**，資料會被切開，分散寫到多顆 disks 上，提高讀寫效能。
RAID 1 的意思是 **mirroring（鏡像）**，資料會完整複製一份，提高可靠性。

所以 **RAID 0+1** 的組合順序是：

```text
先 RAID 0：把資料分散到多顆 disks
再 RAID 1：把整組 RAID 0 複製一份
```

假設有 8 顆 disks，可以分成上下兩組：

```text
上面 4 顆：RAID 0 striped set
下面 4 顆：上面那組的 mirror
```

概念圖：

```text
RAID 0 組 A：Disk 1  Disk 2  Disk 3  Disk 4
RAID 0 組 B：Disk 5  Disk 6  Disk 7  Disk 8

組 B 是組 A 的 mirror
```

所以資料會先被 stripe 到上面那組 disks，同時下面那組保存一模一樣的 stripe 結構。

---

### RAID 0+1 壞一顆 disk 時會怎樣？

圖上方的 `(a) RAID 0 + 1 with a single disk failure` 是在畫 RAID 0+1 壞一顆 disk 的情況。

假設上面那組 RAID 0 裡面有一顆 disk 壞掉。因為 RAID 0 本身沒有容錯能力，所以**只要 stripe set 裡壞一顆，整組 RAID 0 就等於失效**。

但是 RAID 0+1 有另一組 mirror，所以資料還能從下面那組讀出來。

問題是：壞掉一顆 disk 後，上面整組 RAID 0 都不能再被視為完整可用，系統會依賴下面整組 mirror。

所以 RAID 0+1 的弱點是：**一顆 disk 壞掉時，會使整個 striped set 失效，容錯彈性比較差。**

---

### RAID 1+0：先 Mirror，再 Stripe

**RAID 1+0** 又常叫 **RAID 10**。

它的組合順序跟 RAID 0+1 相反：

**先做 RAID 1，再做 RAID 0。**
也就是先把 disks 兩兩做成 mirror pairs，然後再把資料 stripe 到這些 mirror pairs 上。

組合順序是：

```text
先 RAID 1：每兩顆 disks 組成一組 mirror
再 RAID 0：把資料 stripe 到多組 mirror pairs
```

假設有 8 顆 disks，可以看成 4 組 mirror：

```text
Mirror pair 1：Disk 1 ↔ Disk 5
Mirror pair 2：Disk 2 ↔ Disk 6
Mirror pair 3：Disk 3 ↔ Disk 7
Mirror pair 4：Disk 4 ↔ Disk 8
```

然後資料再 stripe 到這四組 mirror pairs 上。

概念圖：

```text
Stripe across：Pair 1   Pair 2   Pair 3   Pair 4

Pair 1：Disk 1 mirror Disk 5
Pair 2：Disk 2 mirror Disk 6
Pair 3：Disk 3 mirror Disk 7
Pair 4：Disk 4 mirror Disk 8
```

所以 RAID 1+0 是「一堆鏡像組，再把資料分散到這些鏡像組」。

---

### RAID 1+0 壞一顆 disk 時會怎樣？

圖下方的 `(b) RAID 1 + 0 with a single disk failure` 是在畫 RAID 1+0 壞一顆 disk 的情況。

假設第一組 mirror pair 裡，上面的 disk 壞掉。因為下面那顆 disk 是它的 mirror，所以這一組 mirror pair 仍然可以提供資料。

其他 mirror pairs 完全不受影響。

所以 RAID 1+0 的優點是：**壞一顆 disk 時，只影響那一組 mirror pair，不會讓整個 stripe set 失效。**

甚至如果壞掉的 disks 分散在不同 mirror pairs，而且每組 mirror pair 至少還剩一顆好 disk，整個 RAID 1+0 仍然可以活著。

---

### RAID 0+1 vs RAID 1+0 差在哪？

兩者都結合了：

```text
RAID 0：striping，高效能
RAID 1：mirroring，高可靠性
```

但組合順序不同。

```text
RAID 0+1：
先 stripe，再 mirror
= mirror of stripes

RAID 1+0：
先 mirror，再 stripe
= stripe of mirrors
```

差別最大的地方是容錯能力。

**RAID 0+1** 中，一顆 disk 壞掉會讓整組 RAID 0 striped set 失效，接下來主要依靠另一整組 mirror。
**RAID 1+0** 中，一顆 disk 壞掉只會影響它所在的 mirror pair，其他 mirror pairs 還是正常。

所以實務上通常比較偏好 **RAID 1+0 / RAID 10**，因為它容錯能力比較好，重建風險也比較低。

---

### 最短記法

```text
RAID 0+1 = 先 RAID 0 後 RAID 1
= 先切條帶，再整組鏡像
= mirrored stripes

RAID 1+0 = 先 RAID 1 後 RAID 0
= 先兩兩鏡像，再跨鏡像組切條帶
= striped mirrors
```

一句話總結：

**RAID 0+1 是把兩組 striped disks 做鏡像；RAID 1+0 是把多組 mirrored disks 做 striping。兩者都有高效能與高可靠性，但 RAID 1+0 通常容錯能力更好。**

## orher
![Ch11-45](https://hackmd.io/_uploads/r1LlpGDWMl.jpg)
![Ch11-46](https://hackmd.io/_uploads/Hy6yRGDZMl.jpg)
![Ch11-47](https://hackmd.io/_uploads/HJfxAMvZfl.jpg)
![Ch11-48](https://hackmd.io/_uploads/rJYgRfDbfx.jpg)
![Ch11-49](https://hackmd.io/_uploads/rJ1-0fPZfg.jpg)
![Ch11-50](https://hackmd.io/_uploads/B1qZAGP-Gg.jpg)
