---
title: chapter 14 File-System Implementation

---

# 14.1 File-System Structure
## Concept
這節在講 **File System（檔案系統）到底怎麼建立在 disk / storage device** 上。前面第 13 章比較偏「使用者看到的檔案與目錄」；第 14 章開始進入「OS 內部怎麼實作 file system」。
## 一、為什麼 disk 適合拿來做 file system？
課本一開始說，傳統上大部分 file system 都建立在 **disk（二級儲存裝置）** 上，因為 disk 有兩個重要特性。

第一，disk 可以 **rewrite in place（原地重寫）**。也就是可以把某個 block 讀出來、修改，再寫回同一個位置。不像某些儲存媒體只能順序寫入或不容易覆寫。

第二，disk 可以 **directly access any block（直接存取任意 block）**。不用像磁帶那樣從頭一路找，可以直接去讀第 1000 個 block、第 50000 個 block。這讓 OS 可以很方便地隨機存取不同檔案的資料。
## 二、Block 是 file system 的基本傳輸單位
Block（區塊） 是 file system 和 storage device 之間進行 I/O 的基本單位。OS 通常不是一個 byte 一個 byte 跟 disk 溝通，而是一次讀寫一個 block。

常見大小可能是：
```
512 bytes
4 KB = 4096 bytes
```
課本提到 disk sector 通常是 512 bytes 或 4096 bytes，而 file system 常常會把一個或多個 sector 組成一個 block。

可以把它想成：
```
Application 看的是 file
File system 管的是 logical blocks
Disk 實際存的是 sectors / physical blocks
```
## 三、File system 要解決兩個核心問題
這段提到 file system 主要有兩個設計問題。

第一個是：**file system 要怎麼呈現給使用者看？**
也就是定義檔案長什麼樣子、有哪些 attributes、有哪些操作，例如 open、read、write、close，也包含 directory structure 的設計。

第二個是：**logical file system 如何對應到 physical storage device？**
也就是使用者看到的是檔案名稱、目錄、路徑，但底層 disk 只知道 block number。OS 必須把「檔案」對應到「disk blocks」。

例如：
```
/home/sj/report.txt
→ inode / file control block
→ logical block 0, 1, 2
→ disk block 8032, 8033, 9812
→ device driver 下命令讀取
```
## 四、Layered File System：分層式檔案系統
這種分層設計的好處是：上層不需要知道太多底層硬體細節，底層也不需要理解使用者的檔案名稱與目錄結構。每層只負責自己該做的事，架構比較清楚，也比較容易維護。
![image](https://hackmd.io/_uploads/ByG4OjpbGe.png)

## 五、I/O Control Layer
**I/O control（輸入/輸出控制層）** 是最靠近硬體的一層，包含 **device driver（裝置驅動程式）** 和 **interrupt handler（中斷處理程式）**。

這一層的工作是把上層的高階命令轉成硬體能懂的低階操作。

例如上層說：
```
retrieve block 123
```
I/O control 會轉成：
```
對 disk controller 寫入 command register
設定目標 block
設定 memory buffer
啟動 DMA / I/O transfer
等待 interrupt 完成
```
所以 device driver 可以看成「翻譯器」：把 OS 的 block I/O request 翻成特定硬體 controller 的指令。
## 六、Basic File System
**Basic file system（基本檔案系統）**，在 Linux 裡也常稱為 **block I/O subsystem（區塊 I/O 子系統）**。

這一層只需要知道比較基本的操作，例如：
```
read block
write block
```
它不太管檔名、不太管目錄，也不管這個 block 屬於哪個使用者。它主要根據 **logical block address（邏輯區塊位址）** 去向 device driver 發出讀寫要求。

另外，這層也會管理一些 **buffer / cache**。
例如資料剛從 disk 讀進來時，會先放在 memory buffer；常用的 file-system metadata 也可能被 cache 起來，避免每次都真的讀 disk。
## 七、File-Organization Module
File-organization module（檔案組織模組） 比 basic file system 更懂「檔案」的概念。
它知道：
```
每個 file 有哪些 logical blocks
哪些 disk blocks 是 free 的
哪些 disk blocks 已經被使用
如何分配新的 block 給 file
```
例如某個檔案有 logical block 0、1、2，但它們不一定連續放在 disk 上。File-organization module 會負責把 file 的 logical block number 轉換成實際 storage 上的 block number。

可以理解成：
```
file logical block 0 → disk block 500
file logical block 1 → disk block 801
file logical block 2 → disk block 1200
```
這層也會處理 **free-space management（空閒空間管理）**，也就是記錄 disk 上哪些 block 還沒被使用。
## 八、Logical File System
Logical file system（邏輯檔案系統） 是最接近使用者抽象的一層。它管理的是 metadata，而不是實際檔案內容。
**Metadata（中介資料 / 描述資料）** 是描述檔案的資料，例如：
```
檔案名稱
檔案大小
擁有者
權限
建立時間
修改時間
資料 block 位置
```
這一層會管理 directory structure，負責把 symbolic file name，例如 report.txt，轉成 file system 內部使用的 file structure。
```
Logical file system 負責管理目錄。
當看到 report.txt 這個檔名時，
它會查 directory entry，
找到 report.txt 對應的 inode / FCB，
接著 OS 才知道這個檔案的真正資訊與資料位置。
```

課本提到 F**CB = File Control Block（檔案控制區塊）**。
在 UNIX 系統中，類似的東西叫 **inode = index node（索引節點）**。==FCB / inode 裡面會存放檔案的各種資訊，例如權限、owner、檔案大小，以及檔案內容所在位置==。

## 九、整體流程範例：讀取一個檔案
假設 application 要讀：
```
read("/home/user/a.txt")
```
整體會經過：
```
application programs
→ 呼叫 read()

logical file system
→ 查 directory，找到 a.txt 對應的 inode / FCB

file-organization module
→ 根據 inode 找出檔案的 logical blocks 對應到哪些 disk blocks

basic file system
→ 發出 read block request

I/O control
→ device driver 控制 disk controller / DMA

devices
→ disk / SSD 實際讀資料

這就是 Figure 14.1 的分層意義。
```
# 14.2 File-System Operations
## Concept
這節開始講 file system operation（檔案系統操作） 背後需要哪些資料結構，以及 `open()`、`close()`、`read()`、`write()` 這些操作在 OS 裡面大概怎麼跑。

前面 14.1 是講 file system 的分層架構；14.2 則更具體地說：file system 要真的運作，必須同時維護 disk 上的資料結構，以及 main memory 裡的暫存資料結構。
## 14.2.1 Overview：檔案系統需要哪些資料結構？
### 觀念
File system 需要兩大類資料結構：
```
1. On-storage structures
   放在 disk / SSD 上，斷電後仍然存在。

2. In-memory structures
   放在 main memory 中，系統執行時用來加速與管理。
```
**On-storage structures** 是長期保存的結構，例如檔案 metadata、directory、free blocks 資訊。
**In-memory structures** 則是 OS 開機後載入 RAM 的結構，用來減少磁碟 I/O、加速查詢與追蹤目前開啟的檔案。
### 一、On-storage structures：存在儲存裝置上的結構
![image](https://hackmd.io/_uploads/By9f3j6bMg.png)
**Boot block** 是這個 partition 的開機區塊。如果這個 partition 可以拿來開機，就會有用來載入 OS 的 boot code。不是每個 partition 都一定真的拿來開機，但 file system 會保留這個位置。

**Super block** 存放整個 file system 的總體資訊，例如 file system 大小、block 數量、free block 數量、inode 數量、block size 等。可以把它想成「這個檔案系統的總管理資料」。

**Free space management** 是用來記錄哪些 disk blocks 還沒被使用。常見做法有 bit vector / bitmap，也就是每個 block 用一個 bit 表示 free 或 allocated。

**Index nodes** 就是 inode 區。UNIX 類系統會用 inode 代表檔案，inode 內部記錄檔案 metadata，以及指向實際 data blocks 的指標。

**Files & directories** 是真正存放檔案內容與目錄資料的地方。檔案的實際資料 block 會在這一區，而目錄本身也是一種特殊檔案，裡面記錄「檔名 → inode number」的對應。
#### 1. Boot control block
**Boot control block（開機控制區塊）** 是每個 volume 可能會有的開機資訊區。
**Volume** 可以理解成一個檔案系統所在的儲存單位，例如一個 partition。

它裡面可能存放「如何從這個 volume 開機」所需的資訊。如果該 volume 不含 OS，就可以是空的。
不同系統名稱不同：
```
UFS：boot block
NTFS：partition boot sector
```
UFS = UNIX File System
NTFS = New Technology File System
#### 2. Volume control block
Volume control block（卷宗控制區塊） 儲存整個 volume 的基本資訊，例如：
```
總共有多少 blocks
每個 block 多大
free blocks 數量
free blocks 的位置
free FCB 數量
free FCB 的位置
```
它有點像「整個檔案系統的總帳本」。

不同系統名稱不同：
```
UFS：superblock
NTFS：master file table
```
**Superblock（超級區塊）** 是 UNIX / Linux 檔案系統裡非常重要的 metadata 結構。
如果 superblock 壞掉，整個檔案系統可能無法正常掛載。
#### 3. Directory structure(Index nodes)
**Directory structure（目錄結構）** 是用來組織檔案的結構。
它會記錄檔名以及對應的內部檔案資訊。

在 UNIX / UFS 中，directory 通常包含：
```
file name
inode number
```
也就是：
```
檔名 → inode number → inode / FCB → 檔案 metadata 與資料 block 位置
```
在 NTFS 中，directory 資訊則存在 master file table（主檔案表） 裡。

#### 4. Per-file FCB
FCB = File Control Block（檔案控制區塊）。
每個檔案都有自己的 FCB，用來記錄該檔案的詳細資訊。
![image](https://hackmd.io/_uploads/Bk9las6WGg.png)
Figure 14.2 畫的就是典型 FCB，裡面包含：
```
file permissions
file dates
file owner, group, ACL
file size
file data blocks or pointers to file data blocks
```
也就是說，FCB 不一定直接存檔案內容，而是存「檔案內容在哪裡」的資訊。

在 UNIX / Linux 中，FCB 對應的概念通常是：
```
inode = index node
```
inode 記錄檔案大小、權限、擁有者、時間、link count、資料 block 指標等。
### 二、In-memory structures：存在 RAM 裡的結構
#### 1. In-memory mount table
**Mount table（掛載表）** 記錄目前系統掛載了哪些 volume / file system。
例如：
```
/        → disk A 的 root file system
/home    → disk B 的 file system
/mnt/usb → USB drive 的 file system
```
當 OS 看到某個路徑時，會根據 mount table 判斷這個檔案到底位於哪個 file system / volume。
#### 2. In-memory directory-structure cache
Directory-structure cache（目錄結構快取） 儲存最近查過的 directory 資訊。

例如剛剛查過：
```
/home/sj/main.c
```
OS 可能會把 /home、/home/sj 的 directory entries cache 起來。下次再查同一個目錄，就不用重新讀 disk。

這可以大幅加快 pathname lookup。
#### 3. System-wide open-file table
System-wide open-file table（系統共用開檔表） 記錄整個系統目前開啟的檔案。

它會保存每個 open file 的資訊，例如：
```
檔案對應的 FCB / inode copy
目前 file offset
開啟模式：read / write / append
reference count
狀態資訊
```
File offset 是目前讀寫位置。
例如已經讀到第 200 bytes，下次 `read()` 會從第 200 bytes 後繼續讀。
#### 4. Per-process open-file table
Per-process open-file table（**每個 process 自己的開檔表**） 是每個 process 私有的表。
當 process 呼叫 `open()` 時，OS 會在這張表中新增一個 entry。這個 entry 會指向 system-wide open-file table 裡的某個 entry。

Application 拿到的 **file descriptor（檔案描述符）**，本質上就是這張 per-process table 的 index。
```
fd = 3
```
意思大概是：
```
process 的 open-file table 第 3 號 entry
→ 指向 system-wide open-file table
→ 指向 FCB / inode
→ 找到真正檔案
```
Windows 裡類似的東西稱為：
```
file handle
```
#### 5. Buffers
**Buffer（緩衝區） 用來暫存正在讀寫的 file-system blocks**。

例如 disk 一次讀出一個 block，資料會先進入 memory buffer，再交給 process。
寫檔時，資料也可能先放進 buffer，之後再統一寫回 disk。

這樣可以減少 I/O 次數，提高效能。
### 建立檔案時發生什麼事？
當 process 要建立一個新檔案時，會呼叫 logical file system。接著大概做這些事：
```
1. 檢查目錄格式與權限
2. 配置一個新的 FCB / inode
3. 找到目標 directory
4. 在 directory 裡新增一個 entry
5. 讓檔名指向新的 FCB / inode
6. 把更新後的 directory / FCB 寫回 storage
```
例如建立：
```
/home/sj/a.txt
```
OS 會先找到 /home/sj 這個 directory，然後新增一筆：
```
a.txt → inode number
```
接著分配一個新的 inode，記錄 a.txt 的 metadata。剛建立時，檔案大小通常是 0，資料 block 可能還沒分配。
### Directory 可以被當成特殊檔案
課本提到，有些 OS，包括 UNIX，會把 directory 當成一種特殊 file。

差別在於 directory 的內容不是普通資料，而是：

file name → inode number

也就是一堆 directory entries。

所以 directory 也可以有自己的 FCB / inode，只是它的 type field 會標示它是 directory。
## 14.2.2 Usage：open、read、write、close 怎麼運作？
### 一、為什麼要先 open？
檔案被使用前，通常要先 `open()`。

原因是：每次 `read()` 或 `write()` 都重新從路徑開始查 directory、找 FCB / inode 會很慢。所以 OS 會用 `open()` 先完成查找，之後用 file descriptor 快速操作。

例如：
```cpp
fd = open("a.txt", O_RDONLY);
read(fd, buffer, 100);
close(fd);
```
`open()` 做完後，後續 `read()` 不用再一直查 `a.txt` 這個名字，而是直接用 `fd` 找到已開啟檔案。
### 二、open() 的流程
`open()` 大概會做：
```
1. 傳入 file name
2. kernel 查 directory structure
3. 找到對應 FCB / inode
4. 檢查權限
5. 如果檔案還沒在 system-wide open-file table 中，就載入 FCB
6. 在 system-wide open-file table 建立 / 更新 entry
7. 在 per-process open-file table 建立 entry
8. 回傳 file descriptor / file handle
```
所以 `open()` 回傳的不是檔案本體，而是一個之後操作檔案用的代號。
### 三、read() / write() 的流程
當 process 呼叫：
```
read(fd, buffer, size);
```
OS 不需要再從檔名開始找，而是：
```
fd
→ per-process open-file table
→ system-wide open-file table
→ FCB / inode
→ 找到資料 block
→ 執行讀取
```
如果資料在 cache / buffer 裡，就直接從 memory 拿。
如果不在，才需要透過 basic file system、I/O control、device driver 去讀 disk。

write() 類似，只是方向變成把資料寫入 buffer / disk，並可能更新 metadata，例如 file size、last modified time。
### 四、close() 的流程
當 process 呼叫 `close(fd)` 時：
```
1. 從 per-process open-file table 移除 entry
2. system-wide open-file table 的 open count 減 1
3. 如果 open count 變成 0，代表沒有 process 再開著這個檔案
4. 把更新過的 metadata 寫回 disk
5. 移除 system-wide open-file table entry
```
這裡的 **open count（開啟計數）** 很重要。因為同一個檔案可能被多個 process 同時 open。

例如：
```
process A 開啟 a.txt
process B 也開啟 a.txt
```
這時 system-wide open-file table 裡可能只有一份主要紀錄，但 open count 是 2。
當 process A close，open count 變 1，檔案還不能完全關掉。等 process B 也 close，open count 變 0，才真正清理相關資料。
### Cache 很重要
課本最後提醒：file-system structures 的 cache 很重要，不能忽略。

通常 OS 會把除了真正 file data blocks 以外的許多 metadata 放在 memory 裡，例如：
```
directory entries
inode / FCB
superblock
mount table
open-file table
recently accessed blocks
```
這樣做的原因很簡單：disk I/O 很慢，memory 快很多。

所以現代 file system 的效能很大一部分來自 cache。
如果 cache hit，就不用真的碰 disk；如果 cache miss，才需要做實際 I/O。
## on-storage structures
![image](https://hackmd.io/_uploads/rkiVgn6bze.png)
上半部是整顆 disk 的配置。最前面是 **MBR = Master Boot Record（主開機記錄）**，它通常位在磁碟最前面，裡面包含開機用的小程式，以及旁邊標示的 **partition table（分割表）**。Partition table 會記錄這顆 disk 被切成哪些 partition，例如 Partition 1、Partition 2、Partition 3、Partition 4，每個 partition 從哪裡開始、大小是多少。

```text
整顆 disk
┌───────────────┬─────────────┬─────────────┬─────────────┬─────────────┐
│ Master boot   │ Partition 1 │ Partition 2 │ Partition 3 │ Partition 4 │
│ record + table│             │             │             │             │
└───────────────┴─────────────┴─────────────┴─────────────┴─────────────┘
```

所以第一層重點是：**partition table 負責告訴 OS：磁碟有哪些 partition，以及每個 partition 的範圍。**

---

下半部則是在放大其中一個 partition，展示這個 partition 裡面的 file system layout。每個 partition 可以被視為一個 **volume（卷宗）**，而一個 volume 裡面會有自己的 file system metadata 和真正的檔案資料。

下半部的結構大概是：

```text
某個 partition / volume
┌────────────┬─────────────┬──────────────────────┬─────────────┬───────────────────┐
│ Boot block │ Super block │ Free space management│ Index nodes │ Files & directories│
└────────────┴─────────────┴──────────────────────┴─────────────┴───────────────────┘
```

**Boot block（開機區塊）** 是這個 partition 自己的開機資訊。如果這個 partition 可以拿來開機，boot block 會包含啟動 OS 需要的資訊；如果不是開機 partition，也可能存在但內容不重要。

**Super block（超級區塊）** 對應課本的 **volume control block（卷宗控制區塊）**。它記錄整個 file system 的基本資訊，例如 block size、總共有多少 blocks、free blocks 有多少、inode 有多少、file system 狀態等。可以把它想成這個 file system 的總設定表。

**Free space management（空閒空間管理）** 用來記錄哪些 disk blocks 還沒被使用。常見方式可能是 bitmap 或 free list。當要建立新檔案、讓檔案變大時，file system 會從這裡找可用空間。

**Index nodes** 指的是 **inodes = index nodes（索引節點）**。inode 是 UNIX / Linux 類檔案系統中用來描述檔案的核心結構，裡面記錄檔案權限、擁有者、大小、時間，以及檔案資料位於哪些 data blocks。注意：inode 通常不存檔名，檔名主要在 directory 裡。

**Files & directories（檔案與目錄）** 是真正存放資料內容的區域。一般檔案的內容放在這裡，directory 本身也會被當成一種特殊檔案，裡面存放「檔名 → inode number」的對應關係。

---

整張圖的核心可以整理成一句話：

**MBR 和 partition table 管的是整顆 disk 如何分割；每個 partition 裡面則有自己的 file system 結構，包括 boot block、superblock、free-space management、inode 區，以及真正存放 files 和 directories 的 data blocks。**

所以這張圖正好對應 14.2 的觀念：file system 不只是檔案內容本身，還需要很多 metadata 才能管理檔名、空間分配、權限、檔案位置與開機資訊。

# 14.3 Directory Implementation
這節在講 **directory 本身要怎麼實作**。前面已經知道 directory 的功能是把 **file name** 對應到 **FCB / inode**，也就是：
```
file name
→ directory entry
→ FCB / inode
→ file data blocks
```
但真正實作時，directory entry 要怎麼存、怎麼搜尋、怎麼新增、怎麼刪除，都會影響 file system 的效率、效能與可靠性。因此 14.3 主要討論兩種常見方法：linear list 和 hash table。
## Figure 14.3：In-memory file-system structures
這張圖分成兩部分。上半部 `(a)` 是 **file open**，下半部 `(b)` 是 **file read**。

在 `(a) File open` 中，application 在 user space 呼叫：
```
open(file name)
```
這時 kernel 會根據 file name 去查 **directory structure**。Directory structure 可能部分已經在 kernel memory 裡，也可能需要從 secondary storage 讀進來。找到檔案後，directory entry 會指到對應的 **FCB = File Control Block（檔案控制區塊）**，也就是儲存檔案 metadata 的結構，例如權限、大小、資料 block 位置等。
所以 open 的重點是：
```
file name
→ 查 directory
→ 找到 FCB / inode
→ 建立 open-file table entry
```
在 `(b) File read` 中，application 不再用 file name，而是用 `open()` 回傳的 **index / file descriptor** 來讀檔。
流程是：
```
read(index)
→ per-process open-file table
→ system-wide open-file table
→ FCB
→ data blocks
```
**Per-process open-file table** 是每個 process 自己的開檔表。
**System-wide open-file table** 是整個系統共用的開檔表。
這樣設計的好處是：`open()` 之後，後續 `read()` / `write()` 不用每次重新搜尋檔名，只要透過 file descriptor 找到已開啟檔案即可。
![image](https://hackmd.io/_uploads/rylM_1kfMg.png)
圖 (a) 同時畫出 **disk / secondary storage** 和 **kernel memory** 裡的 **directory structure**，是因為它們代表不同層次：

**Disk 裡的 directory structure 是正式、長期保存的版本**。
也就是檔案系統真正存在 storage 上的目錄資料。斷電後還會存在。

**Kernel memory 裡的 directory structure 是被載入或快取的版本**。
OS 在執行 `open(file name)` 時，不可能每次都從 disk 慢慢找，所以會把常用或正在查詢的 directory entries 載入 main memory，方便快速查找。

所以圖 (a) 想表達的是：
```
secondary storage:
保存真正的 directory structure 和 FCB

kernel memory:
暫時載入 / cache 部分 directory structure
用來加速 open(file name)
```
## 14.3.1 Linear List
**Linear list（線性串列 / 線性列表）** 是最簡單的 directory implementation。做法是：directory 裡面就是一串 directory entries，每筆 entry 包含：
```
file name
pointer to data blocks / inode number / FCB reference
```
概念像這樣：
```
directory:
[cat → inode 3]
[bo → inode 8]
[a → inode 10]
[test → inode 14]
[data → inode 20]
```
搜尋檔案時，就從頭開始一筆一筆比對 file name。
例如要找 test：
```
cat 不是
bo 不是
a 不是
test 找到
```
這種方法的優點是：**簡單，容易實作。建立、刪除、列出目錄都很直觀**。

但缺點是：**搜尋很慢**。如果 directory 裡有很多檔案，每次找檔案都可能要做 **linear search**。假設有 10,000 個檔案，最差可能要比對 10,000 次。Directory 又是很常被使用的結構，所以搜尋慢會直接拖累 file system performance。

建立新檔案時也有一個問題：必須先搜尋整個 directory，確認沒有同名檔案，才能新增。因為同一個 directory 裡不能有重複檔名。

刪除檔案時，可以有幾種做法：
```
1. 把該 entry 標記成 unused
2. 把 entry 加入 free directory entry list
3. 把最後一個 entry 搬到被刪除的位置，縮短 directory 長度
```
如果 directory 是 sorted list（排序過的列表），搜尋可以用 binary search，比單純 linear search 快。不過新增與刪除會變麻煩，因為要維持排序，可能需要搬動很多 directory entries。

所以 linear list 的取捨是：
```
優點：簡單、實作容易、列出目錄方便。
缺點：搜尋慢，檔案多時效能差。
```
## 14.3.2 Hash Table
**Hash table（雜湊表）** 是為了解決 linear list 搜尋太慢的問題。

它的做法是：用一個 **hash function（雜湊函數）** 把 file name 轉成某個 index，然後直接跳到對應位置找 directory entry。

概念像這樣：
```
file name
→ hash function(file name)
→ 得到 hash table index
→ 到 hash table[index]
→ 找到對應的 directory entry
→ directory entry 再指向 FCB / inode
```
這樣不用從頭掃整個 directory，搜尋速度通常比 linear list 快很多。

不過 hash table 通常還是會搭配 linear list。課本說的是：directory entries 仍然存在 linear list 裡，但另外建立一個 hash table。Hash table 的 entry 會指向 linear list 中對應的 directory entry。

要注意一點：**`hash table[index]` 不一定直接就是 directory entry 本體**。課本這裡比較像是：
```
hash table[index] 裡面放的是 pointer
指向真正存在 linear list 裡的 directory entry
```
也就是 directory entries 可能仍然存在一個 linear list 裡，只是另外加一張 hash table 來加速搜尋。

Hash table 的主要問題是 **collision（碰撞）**。
**Collision 是指兩個不同 file names 算出同一個 hash value**。
例如：
```
hash("data") = 7
hash("mail") = 7
```
這時兩個檔案都想放在同一格，就要處理 collision。
常見做法有兩種：
```
1. Linear probing
2. Chained overflow
```
**Linear probing（線性探測）** 是如果原本位置被占用，就往下一格找空位。
例如 hash 到第 7 格，但第 7 格滿了，就試第 8 格、第 9 格。

**Chained overflow（鏈結溢位）** 是每個 hash table entry 後面接一條 linked list。發生 collision 時，就把同一個 hash value 的 entries 串在一起。

課本比較推薦 chained overflow，因為比較有彈性；即使多個檔名 hash 到同一格，也只是串成一條 list，不用大規模搬動整張 table。

Hash table 還有一個麻煩是：大小通常固定。如果原本 hash table 只有 64 格，後來檔案越來越多，可能需要擴充成 128 格。但 hash table 一旦變大，hash function 可能也要改，原本 entries 的位置也要重新計算，這叫 **rehashing（重新雜湊）**。

所以 hash table 的取捨是：
```
優點：搜尋快，通常比 linear list 有效率。
缺點：需要處理 collision，table 大小與 rehashing 會增加複雜度。
```
# 14.4.1 Contiguous Allocation
**Allocation Methods（配置方法）** 是在講：file system 要怎麼把一個檔案分配到 disk blocks 裡。因為很多檔案共用同一個 secondary storage，所以 OS 必須決定每個檔案要放在哪些 blocks，才能讓空間利用有效、讀取速度也夠快。

主要有三種方法：
```
1. Contiguous allocation：連續配置
2. Linked allocation：鏈結配置
3. Indexed allocation：索引配置
```
這裡先看第一種：Contiguous Allocation（連續配置）。
## 1. Contiguous Allocation 是什麼？
![image](https://hackmd.io/_uploads/S1p2IlyMGl.png)
Contiguous allocation 要求每個 file 必須佔用一段「連續的 disk blocks」。

假設一個檔案從 block `b` 開始，長度是 n 個 blocks，那它會佔用：
```
b, b+1, b+2, ..., b+n-1
```
例如檔案 `mail` 從 block 19 開始，長度 6，那它佔用：
```
19, 20, 21, 22, 23, 24
```
所以 directory entry 只需要記兩個東西：
```
start block
length
```
也就是：
```
file name → start block + length
```
## 3. 優點：存取速度快
Contiguous allocation 最大優點是 **access 很快**。
因為檔案 blocks 是連續的，所以 sequential access 很有效率。例如讀完 block 19，下一個就是 block 20，再下一個就是 block 21。對傳統 HDD = Hard Disk Drive（硬碟） 來說，這樣可以減少 disk head 移動，seek time 低，讀取速度好。

**Sequential access（循序存取）** 很簡單：
```
目前讀 block b
下一個就讀 block b+1
```
**Direct access（直接存取 / random access） 也很容易**。
假設檔案從 block b 開始，要讀檔案的第 i 個 block，就直接讀：
```
b + i
```
例如 `mail` 從 block 19 開始，要讀它的第 3 個 logical block：
```
19 + 3 = block 22
```
所以 contiguous allocation 同時支援 sequential access 和 direct access，而且計算非常簡單。

## 4. 缺點一：External Fragmentation
第一個大問題是 **external fragmentation（外部碎片）**。

隨著檔案不斷建立、刪除，disk 上的 free space 會被切成很多小洞。雖然總 free space 可能夠大，但如果沒有一段夠長的連續空間，就沒辦法放入新檔案。

例如需要 5 個連續 blocks，但 free space 長這樣：
```
free: 2 blocks
used
free: 3 blocks
used
free: 4 blocks
```
總共有 9 個 free blocks，但沒有任何一段連續 5 blocks，所以配置失敗。這就是 external fragmentation。
## 5. 找空間的方法：First Fit / Best Fit
Contiguous allocation 的問題很像前面記憶體管理講過的 **dynamic storage-allocation problem（動態儲存配置問題）**。系統要從 free holes 裡找一塊大小夠的空間。

常見方法有：

**First fit**：找到第一個夠大的洞就放進去。
優點是快；缺點是可能留下很多碎片。

**Best fit**：找最剛好的洞。
優點是看起來比較省空間；缺點是搜尋比較慢，而且可能留下很多超小碎片。

課本提到，first fit 和 best fit 在空間利用上都不算明顯最佳，但 **first fit 通常比較快**。
## 6. 解法之一：Compaction
為了處理 external fragmentation，可以做 **compaction（壓縮 / 緊縮）**。

做法是把已使用的檔案 blocks 搬到一起，讓 free space 合併成一大塊連續空間。

概念像這樣：
```
Before:
used free used free used free

After compaction:
used used used free free free
```
這樣可以解決碎片問題，但代價很高。因為要搬大量資料，對大型 storage 會很耗時。

而且 compaction 常常需要在系統比較空閒時做，甚至需要 offline。
**Offline** 是指正常系統操作暫停，不讓一般使用者繼續使用該 file system。現代系統比較希望能 online 做整理，也就是系統仍然運作時進行，但實作更複雜。
## 7. 缺點二：檔案大小不好預測
Contiguous allocation 的第二個大問題是：**檔案建立時很難知道未來會長多大**。

如果一開始配置太少：
```
file 原本佔 4 blocks
後來需要變成 8 blocks
```
但後面如果已經有其他檔案，就無法直接往後延伸。此時可能要：
```
1. 找一段新的更大連續空間
2. 把整個檔案搬過去
3. 釋放舊空間
```
這很麻煩，也很花時間。

如果一開始配置太多，又會造成 **internal fragmentation（內部碎片）**。
也就是分配給檔案的空間比實際需要大，多出來的空間在檔案內部被浪費。

例如檔案只需要 6 blocks，但一開始分配 20 blocks：
```
實際使用 6 blocks
浪費 14 blocks
```
## 8. 改良方式：Extent
為了改善 contiguous allocation 的缺點，很多系統使用 **extent（延伸區段）**。

**Extent** 是一段連續 blocks。
也就是檔案不一定只能有一段連續空間，而是可以由多個 extents 組成。

例如一個檔案可以長這樣：
```
extent 1: start 10, length 4
extent 2: start 80, length 6
extent 3: start 200, length 3
```
這樣比傳統 contiguous allocation 彈性高。檔案一開始先給一段 contiguous space，如果不夠，再增加新的 extent。

可以想成：
```
傳統 contiguous allocation：
一個檔案只能有一整段連續 blocks。

Extent-based allocation：
一個檔案可以由多段連續 blocks 組成。
```
這樣同時保留部分連續配置的效能優勢，又比較能處理檔案成長問題。
# 14.4.2 Linked Allocation
Linked allocation（鏈結配置） 的核心想法是：**一個 file 不需要放在連續的 disk blocks 裡，而是把分散在 disk 各處的 blocks 用 pointer 串起來**。

也就是說，一個檔案可以長這樣：
```
file jeep:
block 9 → block 16 → block 1 → block 10 → block 25
```
這些 blocks 在 disk 上不連續，但每個 block 裡面都存著==「下一個 block 在哪裡」的 pointer==，所以 file system 可以沿著 pointer 一路把整個檔案讀完。

## Figure 14.5 在畫什麼？
![image](https://hackmd.io/_uploads/H1TVYgyffx.png)
右邊 directory entry 不需要記錄整段連續空間，只需要記：
```
file name: jeep
first block: 9
last block: 25
```
讀檔時，file system 從第一個 block 開始：
```
read block 9
→ block 9 裡面說下一個是 16
→ read block 16
→ block 16 裡面說下一個是 1
→ read block 1
→ ...
→ 最後到 block 25
```
最後一個 block 的 pointer 會是特殊值，例如 null，代表檔案結束。
## Linked Allocation 怎麼建立與寫入？
建立新檔案時，directory entry 會先被建立，first block pointer 設成 `null`，檔案大小設成 0，表示這是一個空檔案。

當檔案要寫入資料時，file system 會從 free-space management 找一個空的 block，把資料寫進去，然後把這個 block 接到檔案 linked list 的尾端。

例如原本：
```
file A:
5 → 12 → 20
```
新增一個 block 30 後：
```
file A:
5 → 12 → 20 → 30
```
所以 linked allocation 的檔案可以自然成長，只要 disk 上還有 free blocks，就可以繼續接新的 block。
## 優點一：沒有 External Fragmentation
Linked allocation 最大優點是：沒有 **external fragmentation（外部碎片）問題**。

因為檔案不需要一整段連續空間，只要 disk 上有任何空 block，就可以拿來用。

例如 free blocks 分散成這樣：
```
free blocks:
3, 8, 14, 22, 29
```
Contiguous allocation 可能會很痛苦，因為找不到連續空間。
但 linked allocation 可以直接把這些 blocks 串起來：
```
3 → 8 → 14 → 22 → 29
```
所以不需要 compaction，也不需要一開始就知道檔案未來會長多大。
## 優點二：檔案容易成長
Linked allocation 不需要在建立檔案時預先宣告大小。檔案變大時，只要再找新的 free block 接到尾端即可。

這解決 contiguous allocation 的問題：如果原本檔案後面已經被其他檔案佔走，contiguous allocation 就很難原地擴充；linked allocation 則完全沒這個壓力。
## 缺點一：Random Access 很差
Linked allocation 的主要缺點是：只適合 sequential access（循序存取），不適合 direct access / random access（直接存取 / 隨機存取）。

假設要讀檔案的第 i 個 block，沒辦法直接算出它在哪裡。必須從第一個 block 開始，一路 follow pointers。

例如要讀第 10 個 block：
```
block 9 → block 16 → block 1 → block 10
```
必須先讀前面 3 個 pointer，才能知道第 4 個 block 在哪裡。

這和 contiguous allocation 差很多。Contiguous allocation 可以直接算：
```
start block + i
```
但 linked allocation 不行。
## 缺點二：Pointer 會浪費空間
因為每個 block 都要存一個 pointer 指向下一個 block，所以每個 block 裡有一小部分空間不能拿來存 user data。
例如課本提到：
```
block size = 512 bytes
pointer size = 4 bytes
```
那真正能存資料的空間只有：
```
512 - 4 = 508 bytes
```
所以每個 block 都有 4 bytes overhead。比例是：
```
4 / 512 = 0.78%
```
看起來不大，但大量 blocks 累積起來仍然是浪費。
## 缺點三：可靠性問題
Linked allocation 還有一個問題是 reliability（可靠性）。因為檔案 blocks 是靠 pointer 串起來的，如果其中某個 pointer 壞掉，整個檔案後半段可能就找不到了。

例如：
```
9 → 16 → 1 → 10 → 25
```
如果 block 16 裡面的 pointer 壞掉，原本應該指向 block 1，結果變成錯誤位置，那 file system 可能會讀到錯的資料，甚至把其他檔案的 block 當成這個檔案的一部分。

可能的改善方法包含：
```
1. doubly linked list：每個 block 同時記前一個和下一個
2. 在每個 block 記 file name / relative block number
3. 使用 FAT 把 pointer 集中管理
```
但這些方法都會增加額外空間或管理成本。
## Cluster：把多個 blocks 合成一組
課本提到一種常見改良方式：使用 **cluster（叢集）**。

Cluster 是多個 blocks 的集合。File system 不再一個 block 一個 block 配置，而是一次配置一個 cluster。

例如：
```
1 cluster = 4 blocks
```
那麼原本每 512 bytes 就要一個 pointer，現在變成每 4 個 blocks 才需要一個 pointer。這樣可以減少 pointer overhead，也減少 free-space management 的負擔。

但缺點是：可能造成更多 **internal fragmentation（內部碎片）**。

例如一個檔案只需要 1 KB，但 cluster 大小是 4 KB，那剩下 3 KB 就浪費了。
## FAT：File-Allocation Table
![image](https://hackmd.io/_uploads/H1RcceJMGe.png)
課本接著介紹 linked allocation 的重要變形：**FAT = File-Allocation Table（檔案配置表）**。

FAT 的想法是：不要把 pointer 放在每個 data block 裡，而是把所有 blocks 的「下一個 block 是誰」集中放在一張表裡。

也就是說，傳統 linked allocation 是：
```
block 217 裡面存 pointer → 618
block 618 裡面存 pointer → 339
block 339 裡面存 pointer → EOF
```
FAT 則是：
```
FAT[217] = 618
FAT[618] = 339
FAT[339] = EOF
```
EOF = End of File（檔案結尾）。

圖 14.6 中，directory entry 裡面記錄檔案 test 的 start block 是 217。

然後從 FAT 查：
```
directory entry:
test → start block 217

FAT[217] = 618
FAT[618] = 339
FAT[339] = -1
```
所以這個檔案實際佔用的 blocks 是：
```
217 → 618 → 339 → EOF
```
圖中的 `-1` 就是 end-of-file marker，代表這是最後一個 block。
## FAT 的好處
FAT 的好處是 pointer 集中在 FAT 裡，不放在每個 data block 裡，所以 data block 可以完整拿來存資料。

另外，如果 FAT 被 cache 在 memory 裡，找下一個 block 就不需要每次都讀 disk block，只要查 memory 中的 FAT，速度會快很多。

例如：
```
查 FAT[217] 得到 618
查 FAT[618] 得到 339
查 FAT[339] 得到 EOF
```
如果 FAT 在 RAM 裡，這些查詢都很快。
## FAT 的缺點
FAT 的問題是整張 table 可能很大，而且如果沒有被 cache 起來，讀取檔案時可能會造成很多 disk head movement。

因為要先去 FAT 區查下一個 block，再去資料區讀 block；如果每次都這樣來回跑，效能會很差。

所以 FAT 系統通常會盡量把 FAT cache 在 memory 裡。
# 14.4.3 Indexed Allocation
**indexed allocation（索引配置）** 是用來改善 linked allocation 隨機存取很差的問題。Linked allocation 雖然可以讓 blocks 分散在磁碟各處，但如果要找檔案第 `i` 個 block，必須從第一個 block 一路追 pointer，速度很慢。Indexed allocation 的解法是：**把一個檔案所有 data block 的位址集中放到同一個 index block 裡面**。

概念是：
```
file
→ index block
→ data block addresses
→ actual data blocks
```
也就是說，每個 file 都有自己的 **index block（索引區塊）**。Index block 裡面不是存檔案資料，而是存「這個檔案的第 0、1、2、3... 個 block 分別放在哪裡」。
## Figure 14.7 在畫什麼？
![image](https://hackmd.io/_uploads/r1wf2xJzfe.png)
圖中檔案 jeep 的 directory entry 記錄：
```
file jeep
index block = 19
```
意思是：jeep 這個檔案的索引表放在 disk block 19。

然後 block 19 裡面放的是：
```
9
16
1
10
25
-1
-1
...
```
這代表 jeep 的實際 data blocks 是：
```
logical block 0 → physical block 9
logical block 1 → physical block 16
logical block 2 → physical block 1
logical block 3 → physical block 10
logical block 4 → physical block 25
```
`-1` 或 `null` 表示後面還沒有使用，沒有對應的 data block。

所以如果要讀 `jeep` 的第 3 個 block，不用像 linked allocation 那樣從頭追 pointer，只要看 index block 的第 3 個 entry：
```
index block[3] = 10
```
因此直接去讀 disk block 10。
## Indexed Allocation 的操作方式
建立新檔案時，OS 會先配置一個 index block，裡面的 entries 一開始全部設成 `null`。

當檔案第一次寫入第 `i` 個 block 時，OS 會從 free-space management 找一個 free block，然後把這個 block 位址填進 index block 的第 `i` 格。

例如：
```
寫入 file 的 logical block 2
↓
OS 找到 free physical block 88
↓
index block[2] = 88
```
之後要讀 logical block 2，就直接查：
```
index block[2] → physical block 88
```
## 優點一：支援 Direct Access
Indexed allocation 最大優點是：支援 **direct access / random access（直接存取 / 隨機存取）**。

因為第 `i` 個 logical block 的位置就在 index block 的第 `i` 個 entry 裡，所以可以直接查表。
```
要讀 file 的第 i 個 block
→ 查 index block[i]
→ 得到 physical block address
→ 直接讀取該 block
```
這點比 linked allocation 好很多。

Linked allocation：
```
要找第 i 個 block
→ 從第一個 block 開始一路追 pointer
```
Indexed allocation：
```
要找第 i 個 block
→ 直接查 index block 第 i 格
```
所以 indexed allocation 很像前面 memory management 的 page table：
**logical block number 透過 index table 對應到 physical block number**。
## 優點二：沒有 External Fragmentation
Indexed allocation 不要求檔案 blocks 連續，所以也沒有 contiguous allocation 的 external fragmentation（外部碎片） 問題。

檔案的 data blocks 可以散落在 disk 各處，只要 index block 有記錄它們的位置即可。
```
file A:
logical block 0 → physical block 9
logical block 1 → physical block 200
logical block 2 → physical block 31
logical block 3 → physical block 88
```
這些 physical blocks 不需要連續。
## 缺點一：Index Block 造成空間浪費
Indexed allocation 的主要缺點是：**每個 file 都要有一個 index block**。

如果檔案很小，例如只有 1 個 block，仍然需要配置一整個 index block 來存 pointer，這就很浪費。

例如：
```
small file:
data block 只有 1 個

但仍然需要：
1 個 index block + 1 個 data block
```
所以小檔案很多時，index block overhead 會變明顯。
## 缺點二：Index Block 大小不好決定
如果 index block 太小，就不能支援大檔案。
如果 index block 太大，小檔案又會浪費空間。

假設：
```
block size = 4096 bytes
pointer size = 4 bytes
```
那一個 index block 可以放：
```
4096 / 4 = 1024 個 pointers
```
如果每個 pointer 指向一個 4 KB data block，那一個 index block 最多可以管理：
```
1024 × 4 KB = 4096 KB = 4 MB
```
所以單一 index block 只能支援到約 4 MB 檔案。若檔案更大，就要用更進階的設計。
## 解決大檔案問題的三種方法
### 1. Linked Scheme：把多個 index blocks 串起來
Linked scheme（鏈結式索引） 是讓 index block 本身也可以串起來。

也就是第一個 index block 裡面放一些 data block addresses，最後一格指向下一個 index block。

概念像：
```
index block 1
→ data block addresses
→ next index block

index block 2
→ data block addresses
→ next index block
```
這樣檔案變大時，可以增加更多 index blocks。

優點是能支援大檔案。
缺點是如果要找很後面的 block，可能還是要追 index block pointer，因此效能不一定最好。
### 2. Multilevel Index：多層索引
Multilevel index（多層索引） 是把 index 做成多層結構。

第一層 index block 不直接指向 data block，而是指向第二層 index blocks。第二層 index blocks 才指向真正 data blocks。

概念像：
```
first-level index block
→ second-level index block
→ data blocks
```
如果還不夠，可以變成三層、四層：
```
level 1 index
→ level 2 index
→ level 3 index
→ data blocks
```
課本例子中，如果 block size 是 4096 bytes，一個 index block 可以放 1024 個 4-byte pointers。兩層 index 可以管理：
```
1024 × 1024 data blocks
```
如果每個 data block 是 4 KB：
```
1024 × 1024 × 4 KB = 4 GB
```
所以 multilevel index 可以支援非常大的檔案。
### 3. Combined Scheme：UNIX inode 的混合設計
![image](https://hackmd.io/_uploads/Byj-ZZkMzx.png)
**Combined scheme（混合式配置）** 是 UNIX 類系統常用的方法。它把不同方法混在一起，讓小檔案快、大檔案也能支援。

課本 Figure 14.8 畫的是 **UNIX inode**。
**inode = index node（索引節點）**，是 UNIX / Linux 中用來記錄檔案 metadata 和 block pointers 的資料結構。

它的設計大概是：
```
inode
├── file metadata
├── direct blocks
├── single indirect block
├── double indirect block
└── triple indirect block
```
---

**Direct blocks**
Direct blocks（直接區塊指標） 是 inode 裡直接指向 data blocks 的 pointers。

例如 inode 前 12 個 pointers 是 direct blocks：
```
direct pointer 0 → data block
direct pointer 1 → data block
...
direct pointer 11 → data block
```
如果 block size 是 4 KB，12 個 direct blocks 可以直接存取：
```
12 × 4 KB = 48 KB
```
所以小檔案可以不用額外 index block，直接靠 inode 裡的 direct pointers 找到資料。這很有效率。

---
**Single indirect block**

Single indirect block（單層間接區塊） 是 inode 裡的一個 pointer，指向一個 index block。這個 index block 裡面再放 data block addresses。
```
inode
→ single indirect block
→ data blocks
```
也就是多一層查表。

---
**Double indirect block**

Double indirect block（雙層間接區塊） 是 inode 指向第一層 index block，第一層 index block 再指向第二層 index blocks，第二層 index blocks 才指向 data blocks。
```
inode
→ double indirect block
→ indirect blocks
→ data blocks
```
這可以支援更大的檔案。

---
**Triple indirect block**

Triple indirect block（三層間接區塊） 再多一層：
```
inode
→ triple indirect block
→ double indirect blocks
→ indirect blocks
→ data blocks
```
這用來支援超大檔案。

---
inode 不是檔案內容本體，而是檔案的控制資訊與 block 指標。圖中左邊 inode 內含：
```
mode
owners
timestamps
size / block count
direct blocks
single indirect
double indirect
triple indirect
```
**mode** 通常包含檔案型態與權限，例如 regular file、directory、symbolic link，以及 read / write / execute 權限。

**owners** 通常記錄 owner 和 group，也就是檔案擁有者與群組。

**timestamps** 是時間資訊，例如 access time、modify time、change time。不同系統可能命名略有不同，但重點是記錄檔案被存取、修改或 metadata 改變的時間。

**size / block count** 記錄檔案大小與佔用多少 blocks。

**direct blocks** 是 inode 直接指向 data blocks。小檔案可以直接靠這些 pointers 找到資料，速度快。

**single indirect block** 是 inode 指向一個 index block，而那個 index block 再指向 data blocks。

**double indirect block** 是 inode 指向第一層 index block，第一層再指向第二層 index blocks，第二層才指向 data blocks。

**triple indirect block** 則是三層 index block，支援更大的檔案。
# 14.4.4 Performance
這節是在比較前面三種 allocation methods 的效能差異。重點不是哪一種永遠最好，而是要看 file system 主要使用情境是 **sequential access（循序存取）** 還是 **direct access / random access（直接存取 / 隨機存取）**。

三種方法分別是：

```text
Contiguous allocation：連續配置
Linked allocation：鏈結配置
Indexed allocation：索引配置
```

---

## 1. Contiguous allocation：最快，但彈性差

**Contiguous allocation** 的效能通常最好，因為一個 file 的 blocks 是連續的。

假設檔案從 block `b` 開始，要讀第 `i` 個 block，直接算：

```text
physical block = b + i
```

所以不管是 sequential access 還是 direct access 都很快。

例如：

```text
file start block = 100
要讀 logical block 5
→ physical block = 100 + 5 = 105
```

只要 directory entry 或 FCB 裡有 `start block` 和 `length`，OS 就能馬上算出位置。

因此 contiguous allocation 的優點是：

```text
讀取快
計算簡單
sequential access 很有效率
direct access 也很有效率
```

但缺點是：

```text
容易 external fragmentation
檔案大小不好預測
檔案變大時可能要搬移
```

所以效能好，但管理麻煩。

---

## 2. Linked allocation：適合 sequential，不適合 direct access

**Linked allocation** 的檔案 blocks 可以散落在 disk 各處，靠 pointer 串起來。

例如：

```text
file A:
9 → 16 → 1 → 10 → 25
```

如果是 sequential access，問題不大。因為讀完 block 9，就知道下一個是 16；讀完 16，就知道下一個是 1。

但如果要 direct access，例如直接讀第 4 個 block，就麻煩了。因為不能直接算位置，必須從第一個 block 開始一路追 pointer：

```text
第 0 個：9
第 1 個：16
第 2 個：1
第 3 個：10
第 4 個：25
```

所以要找第 `i` 個 block，最壞情況可能要讀 `i` 次 pointer。這就是 linked allocation 不適合 random access 的原因。

它的優點是：

```text
沒有 external fragmentation
檔案容易成長
不需要事先宣告檔案大小
```

缺點是：

```text
direct access 很慢
pointer 有額外空間成本
pointer 壞掉會影響可靠性
```

所以這節說：**如果 application 需要 direct access，就不應該使用 linked allocation。**

---

## 3. Indexed allocation：支援 direct access，但有 index overhead

**Indexed allocation** 把 file 所有 data blocks 的位址集中放在 **index block（索引區塊）** 裡。

要讀第 `i` 個 logical block 時：

```text
index_block[i] → physical block number
```

所以它支援 direct access，比 linked allocation 好很多。

例如：

```text
index_block[0] = 9
index_block[1] = 16
index_block[2] = 1
index_block[3] = 10
index_block[4] = 25
```

要讀 logical block 3，就查：

```text
index_block[3] = 10
```

然後直接讀 physical block 10。

不過 indexed allocation 的效能取決於 **index block 在不在 memory 裡**。

如果 index block 已經 cache 在 memory 中：

```text
查 index block 很快
→ 直接找到 data block
```

如果 index block 不在 memory：

```text
先讀 index block
再讀 data block
```

如果是 double indirect 或 triple indirect，就可能要讀更多層 index blocks，才找到真正的 data block。

所以它的優點是：

```text
支援 direct access
沒有 external fragmentation
檔案可以分散放
```

缺點是：

```text
index block 佔空間
小檔案可能浪費
多層 indirect 會增加額外 disk access
```

---

## 4. 有些系統會混合使用

課本提到，有些系統會根據檔案大小混合使用 contiguous allocation 和 indexed allocation。

例如小檔案只有 3、4 個 blocks，直接用 contiguous allocation，因為快又簡單。
如果檔案變大，再自動切換成 indexed allocation。

這樣的設計理由是：**大多數檔案其實都很小**。所以小檔案用 contiguous allocation 可以有很好的平均效能；大檔案再用 indexed allocation 取得彈性。

這也和 UNIX inode 的 combined scheme 很像：

```text
小檔案 → direct blocks，快
大檔案 → indirect blocks，彈性高
```

---

## 5. 傳統 HDD 和 NVM 的考量不同

傳統 **HDD = Hard Disk Drive（硬碟）** 有 disk head，因此 file system 很在意：

```text
seek time
head movement
block 是否連續
```

所以 contiguous allocation 對 HDD 很有優勢，因為可以減少磁頭移動。

但 **NVM = Nonvolatile Memory（非揮發性記憶體）**，例如 SSD 類儲存裝置，沒有傳統磁頭移動問題。因此很多傳統為了避免 disk head seek 的最佳化，在 NVM 上未必有效。

對 NVM 來說，重點可能變成：

```text
減少 CPU instruction count
減少 software path overhead
減少資料在 OS 內部繞太多層
降低 latency
提高 parallelism
```

也就是說，storage 變快後，瓶頸可能不再是磁碟機械動作，而是 OS / file system 本身的處理成本。

---

## 總結比較

```text
Contiguous allocation
優點：sequential / direct access 都快
缺點：external fragmentation、檔案不易成長

Linked allocation
優點：無 external fragmentation、檔案容易成長
缺點：direct access 很慢，只適合 sequential access

Indexed allocation
優點：支援 direct access、無 external fragmentation
缺點：index block overhead，多層 index 會增加存取成本
```

本節核心觀念是：**allocation method 的效能取決於 access pattern。Sequential access 可以接受 linked allocation；direct access 則較適合 contiguous 或 indexed allocation。現代系統常混合多種方法，讓小檔案快、大檔案也能彈性成長。**

# 14.5.1 Bit Vector
Free-space management（空閒空間管理） 是在處理一件事：disk 空間有限，所以檔案被刪除後釋放出來的 blocks 要能重新被使用。

File system 必須知道：
```
哪些 blocks 已經被檔案使用
哪些 blocks 還是 free，可以分配給新檔案
```
所以 OS 會維護一個 **free-space list（空閒空間清單）**。雖然名字叫 list，但它不一定真的用 linked list 實作。這一節先介紹最常見的一種做法：**bit vector / bitmap**。
## Bit Vector / Bitmap
Bit vector（位元向量） 或 bitmap（位元圖） 的意思是：用一串 bits 來表示 disk blocks 的使用狀態。

每個 block 對應 1 個 bit：
```
bit = 1 → 這個 block 是 free
bit = 0 → 這個 block 已經 allocated
```
所以如果 disk 有 32 個 blocks，就可以用 32 個 bits 表示整個 disk 的空間狀態。
## 課本例子
題目說 blocks：
```
2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 17, 18, 25, 26, 27
```
是 free blocks，其餘 blocks 都已經被 allocated。

所以從 block 0 開始看：
```
block number:  0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 ...
bit value:     0 0 1 1 1 1 0 0 1 1 1  1  1  1  0  0  0  1  1  0 ...
```
因此 bitmap 會長成：
```
00111100111111000110000011100000 ...
```
這串 bit 的意思就是：
```
第 0 個 bit = 0 → block 0 已使用
第 1 個 bit = 0 → block 1 已使用
第 2 個 bit = 1 → block 2 是 free
第 3 個 bit = 1 → block 3 是 free
...
```

## 建立新檔案時怎麼用？
假設 file system 要幫新檔案找 3 個 free blocks，它就可以去 bitmap 裡找 1。

如果 allocation method 不要求連續，例如 linked allocation 或 indexed allocation，那找到任意三個 1 就可以：
```
block 2, block 3, block 4
```
分配出去後，要把這些 bit 改成 0：
```
原本：
00111100111111000110000011100000

分配 block 2, 3, 4 後：
00000100111111000110000011100000
```
因為 block 2、3、4 已經不再 free。
## 刪除檔案時怎麼用？
如果某個檔案被刪除，它原本佔用的 blocks 要歸還給 free-space management。

例如刪除的檔案原本佔用 blocks 10、11、12，那 bitmap 中對應 bits 要改回 1：
```
block 10, 11, 12 變成 free
```
所以 free-space management 的本質就是：
```
allocate block → bit 從 1 改成 0
free block     → bit 從 0 改成 1
```
## Bit Vector 的優點
Bit vector 的最大優點是：簡單，而且容易找 free blocks。

尤其現代 CPU 有很多 bit manipulation instructions，可以快速找出哪個 word 裡面有 1。

課本提到找第一個 free block 的方法：
```
先以 word 為單位掃 bitmap
如果某個 word 全部都是 0，代表那一整段 blocks 都已經被使用
找到第一個不是 0 的 word
再找這個 word 裡第一個 1 的位置
```
公式是：
```
block number =
(number of bits per word) × (number of 0-valued words)
+ offset of first 1 bit
```
意思是：
```
前面跳過多少個全 0 的 word
× 每個 word 有幾個 bits
+ 在目前 word 中第一個 1 的 offset
= 第一個 free block 的 block number
```
## Bit Vector 的缺點
缺點是：整個 bitmap 必須佔用空間，而且最好放在 main memory 才有效率。

如果 bitmap 放在 disk，每次找 free block 都要讀 disk，效能會很差。
但如果 disk 很大，bitmap 本身也會變大，全部放進 memory 會有成本。

課本例子：

1.3 GB disk，512-byte blocks
每個 block 需要 1 bit，所以需要超過 332 KB 的 bitmap。

1 TB disk，4 KB blocks

1 TB = 2^40 bytes
block size = 4 KB = 2^12 bytes

block 數量 = 2^40 / 2^12 = 2^28 blocks

每個 block 用 1 bit，所以 bitmap 需要：

2^28 bits = 2^25 bytes = 32 MB

所以 disk 越大，bitmap 也越大。這就是 bit vector 的主要成本。
# 14.5.2 Linked List
這節的 **Linked List（鏈結串列）** 不是在講「檔案資料 blocks 怎麼串」，而是在講 **free blocks（空閒區塊）怎麼串起來管理**。

前面 14.5.1 的 bitmap 是用 0/1 表示哪些 blocks free；這裡的 linked list 則是把所有 free blocks 用 pointer 串起來。

---

## 核心概念

File system 會在某個特殊位置記住：

```text
free-space list head
```

也就是「第一個 free block 在哪裡」。

然後第一個 free block 裡面會存下一個 free block 的位置，下一個 free block 又存再下一個 free block 的位置，以此類推。

概念像：

```text
free-space list head
→ block 2
→ block 3
→ block 4
→ block 5
→ block 8
→ block 9
→ block 10
→ ...
→ null
```

所以整個 free-space list 就是一條由 free blocks 組成的 linked list。

---

## Figure 14.9 在畫什麼？

圖中灰色 blocks 是 free blocks。這些 free blocks 被箭頭串起來。

根據課本例子，free blocks 是：

```text
2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 17, 18, 25, 26, 27
```

所以 linked free-space list 會像：

```text
head → 2 → 3 → 4 → 5 → 8 → 9 → 10 → 11 → 12 → 13 → 17 → 18 → 25 → 26 → 27 → null
```

圖中的 **free-space list head** 指向 block 2，代表 block 2 是目前第一個可用的 free block。Block 2 裡面再存下一個 free block 的位置，也就是 block 3。

---

## 配置新 block 時怎麼做？

如果 file system 要分配一個 free block 給新檔案，最簡單就是拿 list head 指到的第一個 free block。

例如目前：

```text
head → 2 → 3 → 4 → 5 → ...
```

分配 block 2 給某個檔案後，head 就改成指向 block 3：

```text
head → 3 → 4 → 5 → ...
```

所以 allocate 一個 block 很快，只要拿掉 linked list 的第一個節點即可。

---

## 刪除檔案時怎麼做？

如果某個檔案刪除，釋放出 block 20，那 file system 可以把 block 20 接回 free-space list。

例如原本：

```text
head → 3 → 4 → 5 → ...
```

把 block 20 加回去，可以變成：

```text
head → 20 → 3 → 4 → 5 → ...
```

也就是讓 block 20 指向原本的 head，再把 head 改成 block 20。

---

## 優點

Linked list 的優點是：**不需要一整張 bitmap 放在 memory 裡**。只要記住第一個 free block 的位置，就能管理 free space。

而且每次只需要一個 free block 時，操作很簡單：

```text
拿 head 指到的 block
更新 head
```

這對 linked allocation 或 indexed allocation 這種「不一定需要連續 blocks」的方法很方便。

---

## 缺點

最大的缺點是：**不容易快速找到連續的 free blocks**。

因為 free blocks 是用 pointer 串起來的，可能分散在 disk 各處。如果想找一段連續空間，例如 contiguous allocation 需要 5 個連續 blocks，就必須沿著 linked list 慢慢檢查：

```text
2 → 3 → 4 → 5 → 8 → ...
```

還要判斷哪些 blocks 是連續的。這會很慢。

另一個缺點是：如果要 traverse 整條 free-space list，就必須讀很多 blocks。對 **HDD = Hard Disk Drive（硬碟）** 來說，這會造成大量 seek time。不過課本也說，完整走訪 free list 不是很常發生；通常 OS 只是拿第一個 free block 來用。

---

## 跟 FAT 的關係

課本最後提到 **FAT = File-Allocation Table（檔案配置表）**。FAT 本來就是用表格記錄 block 之間的鏈結關係，所以它也可以順便管理 free blocks。

在 FAT 裡，每個 block 都有一個 table entry。這個 entry 可以表示：

```text
這個 block 的下一個 block 是誰
或
這個 block 是 free
或
這個 block 是 EOF
```

因此 FAT 系統不需要另外建立一套 free-space linked list，因為 block 是否 free 已經可以從 FAT entries 看出來。

---

## 和 Bit Vector 比較

```text
Bit Vector / Bitmap：
每個 block 用 1 bit 表示 free 或 allocated。
優點：容易找連續 free blocks。
缺點：大磁碟需要較大的 bitmap，通常要放 memory 才快。

Linked List：
把所有 free blocks 用 pointer 串起來。
優點：分配單一 free block 很簡單，不需要大型 bitmap。
缺點：不容易找連續空間，完整走訪很慢。
```

---

## 本節一句話總結

**Linked-list free-space management 是把所有 free blocks 串成一條 linked list，file system 只要記住第一個 free block 的位置。配置空間時取出 list head，釋放空間時把 block 接回 list。它簡單、省 memory，但不適合快速尋找連續 free blocks，也不適合頻繁完整掃描整條 free list。**

# 刪除檔案不是單純把檔案名稱拿掉而已
這張補充在講一件很重要的觀念：**刪除檔案不是單純把檔案名稱拿掉而已，file system 背後要更新很多 metadata，所以刪除其實很忙。**

尤其是 UNIX / Linux 類檔案系統裡，一個檔案可能牽涉：

```text
directory entry → inode → direct blocks / indirect blocks → data blocks
```

所以刪除時，這些東西都要處理乾淨。

---

## 1. Data blocks back to free list

**Data blocks（資料區塊）** 是真正存檔案內容的 disk blocks。

刪除檔案時，這些 blocks 不會真的被「清成 0」，而是被標記成 **free**，加入 **free list / free-space structure（空閒空間管理結構）**。之後新檔案可以重新使用這些 blocks。

所以刪除檔案比較像：

```text
這些 data blocks 以前屬於 file A
現在不屬於任何檔案
可以重新分配給其他檔案
```

這也解釋為什麼資料救援有時可行：因為刪除後資料內容可能還留在 disk 上，只是 file system 不再把它當成有效檔案。

---

## 2. Coalescing free space

**Coalescing free space（合併空閒空間）** 是指把相鄰的 free blocks 合併成較大的連續空間。

例如刪除前：

```text
free 10~12
used 13~15
free 16~20
```

如果刪掉 blocks 13~15，就可以變成：

```text
free 10~20
```

這對 **contiguous allocation（連續配置）** 或 extent-based file system 很重要，因為它們偏好找到較大的連續空間。

---

## 3. Indirect blocks back to free list

**Indirect blocks（間接區塊）** 不是存檔案內容，而是存 data block pointers。

例如 UNIX inode 裡有：

```text
direct blocks
single indirect block
double indirect block
triple indirect block
```

如果檔案很大，它會用 indirect blocks 去指向更多 data blocks。刪除大檔案時，不只 data blocks 要回收，這些 indirect blocks 本身也要回收到 free list。

所以大檔案刪除成本高，是因為 file system 要一路走過很多 index / indirect blocks，找到所有 data blocks，再一個一個釋放。

---

## 4. Expensive for large files, an ext3 problem

**ext3 = third extended file system（第三代延伸檔案系統）**，是 Linux 早期常見的 journaling file system。

這裡說「large files 很 expensive」是因為 ext3 使用類似 UNIX inode 的 direct / indirect block 結構。大檔案可能有大量 indirect blocks 和 data blocks。刪除時，系統必須更新很多 block bitmap、inode bitmap、inode metadata、journal 等等。

所以刪除一個超大檔案不是 O(1) 的操作，而可能要掃過大量 block pointers。這就是投影片說的 ext3 problem。

---

## 5. Inodes cleared

**inode = index node（索引節點）**，它記錄檔案 metadata，例如權限、owner、size、timestamps，以及 data blocks 的位置。

刪除檔案時，inode 也要被清掉或標記為 unused。
一旦 inode 被清除，原本那些 data blocks 就變成「dead data blocks」：資料可能還在 disk 上，但已經沒有有效 inode 指向它們。

可以理解成：

```text
inode 還在 → OS 知道檔案在哪裡
inode 清掉 → OS 不再知道這些資料屬於誰
```

---

## 6. Inode free list written

刪除後，原本使用中的 inode 也可以被重新利用。因此 file system 要更新 **inode free list / inode bitmap**，告訴系統：

```text
這個 inode number 現在空出來了
以後可以分配給新檔案
```

所以刪檔不只釋放 data blocks，也要釋放 inode。

---

## 7. Directory updated

**Directory（目錄）** 裡面有 directory entry，負責記錄：

```text
file name → inode number
```

例如：

```text
report.txt → inode 123
```

刪除檔案時，directory entry 也要更新，把 `report.txt` 這個名稱移除。否則使用者還是會在目錄裡看到這個檔案。

所以刪檔至少牽涉三個層次：

```text
directory entry 移除檔名
inode 被清除 / 釋放
data blocks 回到 free list
```

---

## 8. The order of updates matters

這句是整張最重要的觀念：**更新順序很重要。**

例如，不能太早把 data block 放回 free list。因為如果某個 inode 還指向那個 block，但 block 已經被標記成 free，系統可能把它分配給新檔案。結果會變成：

```text
舊 inode 還指向 block X
新檔案也拿到 block X
```

這會造成嚴重 file-system corruption（檔案系統毀損）。

所以投影片最後說：

```text
Can put block on free list only after no inode points to it
```

意思是：**只有確定沒有任何 inode 還指向某個 block 時，才能把它加入 free list。**

反過來，如果先清掉 inode，但還沒把 data blocks 加回 free list，系統當機，就可能造成 **space leak（空間洩漏）**：那些 blocks 沒有 inode 指向，也沒有在 free list 裡，等於磁碟空間憑空消失。

---

## 整體刪檔流程可以理解成

```text
1. 從 directory 移除檔名對應
2. 檢查 inode 是否還有其他 hard links 或 open file references
3. 如果沒有其他 references，釋放 data blocks
4. 釋放 indirect blocks
5. 清除 inode
6. 把 inode 放回 inode free list
7. 更新 free-space management 結構
8. 依正確順序寫回 disk / journal
```

補充一下，UNIX / Linux 裡真正的刪除通常是 **unlink**。如果檔案還有其他 hard link，或仍然被 process open，檔案資料不會立刻釋放。只有當 link count 變成 0，而且沒有 process 再開著它，data blocks 才會真的回到 free list。

---

## 一句話總結

刪除檔案很麻煩，因為 file system 不只要移除 directory entry，還要釋放 data blocks、indirect blocks、inode，更新 free list / bitmap，並且特別注意更新順序。順序錯了可能造成 data corruption；中途當機則可能造成 lost blocks 或 inconsistent metadata。

# 14.7 Recovery
## 1. Recovery：傳統復原方式
第一張投影片講三個重點：consistency checking、backup、restore。

**Consistency checking（一致性檢查）** 是檔案系統發現可能不一致時，用系統工具掃描 disk，檢查 directory structure、inode、free-space list、data blocks 之間有沒有矛盾。

例如刪除檔案時，本來應該完成：
```
directory entry 移除
inode 清除
data blocks 回到 free list
inode 回到 inode free list
```
如果中途斷電，可能變成：
```
directory 已經沒有檔名
但 inode 還存在

或

inode 還指向 data block
但 data block 已經被放回 free list
```
這種狀況就叫 inconsistency（不一致）。**Consistency checker** 會嘗試找出這些問題並修復。UNIX / Linux 常見工具是 `fsck`，意思是 **file-system check**。

不過缺點是：它可能很慢，因為可能要掃描整個 file system；而且有些錯誤不一定能完全修好。例如 metadata 還在但內容亂掉，系統可能只能盡力修結構，不一定能救回正確資料。

Backup（備份） 是把資料從 disk 複製到另一個 storage device，例如磁帶、另一顆硬碟、光碟、NAS 或雲端。它處理的是更嚴重的情況：資料真的遺失、disk 壞掉、file system 被破壞時，可以靠備份還原。

Restore（還原） 則是從 backup 把資料救回來。
Consistency checking 比較像「修理檔案系統結構」；backup / restore 則是「資料真的沒了時，從另一份副本救回」。
## 2. Log Structured / Journaling File Systems
第二張投影片是在講比較現代的 recovery 方法：**log structured / journaling file system（日誌式檔案系統）**。

這裡的重點是：檔案系統在真正修改 metadata 之前，會先把「等等要做哪些修改」寫進一個 **log（日誌）**。每一組相關修改稱為一個 **transaction（交易）**。

例如刪除檔案不是單一動作，而是一組 transaction：
```
刪除 directory entry
釋放 inode
釋放 data blocks
更新 free-space bitmap
更新 inode bitmap
```
這些操作必須被視為一組。要嘛全部完成，要嘛當作沒有完成，不能只完成一半。
## 3. Transaction 和 committed 是什麼？
**Transaction（交易）** 是一組必須共同完成的 file-system updates。
**Committed（提交）** 的意思是：這筆 transaction 已經完整寫入 log，因此系統承認「這件事應該被完成」。

注意重點：**committed 不代表真正的 file system structure 已經全部更新完**。

流程可能是：
```
1. 把 transaction 寫到 log
2. log 寫完，標記 committed
3. 之後再慢慢把 transaction 寫到真正的 file system structures
4. 寫完後，把 transaction 從 log 移除
```
所以 log 是一種保險。只要 transaction 已經 committed，就算中途 crash，重開機後也可以根據 log 把剩下的更新補完。
## 4. Crash 發生時怎麼 recovery？
如果系統 crash，重開機後 file system 不需要掃整顆 disk，而是先檢查 log。

如果 log 裡有 committed transaction，代表這些操作已經被承認，但可能還沒真正全部寫到 file system，所以 recovery 時要 redo（重做），把它們補寫完成。

如果 log 裡有 uncommitted transaction，代表當機前那筆交易還沒完整寫入 log，所以可以丟掉，不執行。**不把那一筆尚未 committed 的 transaction 所描述的檔案系統更新套用到真正的 file-system structures 上**。

概念如下：
```
committed transaction
→ recovery 時重做，確保完成

uncommitted transaction
→ recovery 時忽略，當作沒發生
```
這樣可以避免 metadata 處於半完成狀態。

## 5. 為什麼 journaling recovery 比 consistency checking 快？

傳統 consistency checking 可能要掃整個 file system，找出所有可能不一致的地方。

Journaling 則只需要看 log：

檢查 log
→ 找到未完成但已 committed 的 transactions
→ redo
→ 完成 recovery

所以 recovery 通常快很多，而且可以大幅降低 metadata inconsistency 的機率。

## 6. 這兩張的核心比較
Consistency checking:
事後掃描整個 file system，找出並修復不一致。
缺點是慢，而且不一定完全修得好。

Backup / restore:
資料真的遺失或 disk 壞掉時，從備份還原。
重點是資料保存。

Journaling / log structured:
事前把 metadata updates 寫成 transaction 到 log。
Crash 後靠 log redo committed transactions。
優點是 recovery 快，metadata 較不容易壞。
# 專有名詞與解釋
| 專有名詞                     | 中文          | 解釋                                                                                                    |
| ------------------------ | ----------- | ----------------------------------------------------------------------------------------------------- |
| File System              | 檔案系統        | OS 用來管理檔案、目錄、metadata、磁碟空間分配與資料存取的整體機制。使用者看到的是檔名與目錄，底層其實是 block、inode、FCB、free-space structure 等資料結構。 |
| Storage Device           | 儲存裝置        | 存放 file system 的硬體，例如 HDD、SSD、NVM device。                                                             |
| Disk                     | 磁碟          | 傳統 secondary storage，特色是可以 rewrite in place，也可以 direct access 任意 block。                               |
| NVM = Nonvolatile Memory | 非揮發性記憶體     | 斷電後資料仍保存的儲存裝置，例如 SSD 類裝置。沒有傳統 HDD 的磁頭 seek 問題。                                                        |
| Block                    | 區塊          | File system 和 storage device 之間讀寫資料的基本單位。OS 通常不是一 byte 一 byte 讀 disk，而是一次讀寫一個 block。                  |
| Sector                   | 磁區          | 磁碟硬體層級的基本單位。File-system block 可能由一個或多個 sectors 組成。                                                    |
| Logical Block            | 邏輯區塊        | 從「檔案角度」看到的第 0、1、2... 個 block。                                                                         |
| Physical Block           | 實體區塊        | 真正在 disk / SSD 上的 block number。File system 要把 logical block 對應到 physical block。                       |
| Metadata                 | 中介資料 / 描述資料 | 描述檔案的資料，例如檔案大小、權限、owner、timestamps、資料 block 位置等。metadata 不是檔案內容本身。                                    |
| Data Block               | 資料區塊        | 真正存放檔案內容的 block。                                                                                      |
| FCB = File Control Block | 檔案控制區塊      | 每個檔案的控制資料結構，記錄檔案權限、大小、擁有者、時間、資料 block 指標等。                                                            |
| inode = index node       | 索引節點        | UNIX / Linux 類系統中的 FCB。inode 通常記 metadata 和 data block pointers，但通常不直接存檔名。                            |
| Directory Entry          | 目錄項目        | Directory 裡的一筆資料，通常是「檔名 → inode number / FCB」的對應。                                                     |
| Directory Structure      | 目錄結構        | 組織檔案名稱與檔案內部資料結構的方式。可以理解成 file name 到 inode / FCB 的查找結構。                                               |
| Pathname Lookup          | 路徑查找        | OS 根據路徑名稱一層一層查 directory，例如 `/home/sj/a.txt` 要查 `/`、`home`、`sj`、`a.txt`。                              |
| Symbolic File Name       | 符號檔名        | 使用者看到的檔名，例如 `report.txt`。OS 內部還要把它轉成 inode / FCB 才能找到真正資料。                                            |

---

| 專有名詞                       | 中文      | 解釋                                                                          |
| -------------------------- | ------- | --------------------------------------------------------------------------- |
| Layered File System        | 分層式檔案系統 | File system 的實作被分成多層，上層處理檔名與 metadata，下層處理 block I/O 和硬體控制。                 |
| Logical File System        | 邏輯檔案系統  | 最接近使用者抽象的一層，負責 directory、metadata、FCB / inode、權限等。                          |
| File-Organization Module   | 檔案組織模組  | 負責把 file 的 logical blocks 對應到 physical blocks，也處理 free-space management。    |
| Basic File System          | 基本檔案系統  | 負責發出基本的 read block / write block request，不太管檔名與權限。                          |
| I/O Control                | 輸入輸出控制層 | 最靠近硬體的一層，包含 device driver 和 interrupt handler，把 OS 命令轉成硬體 controller 能懂的操作。 |
| Device Driver              | 裝置驅動程式  | OS 與硬體裝置之間的翻譯器，把高階 I/O request 轉成特定裝置控制命令。                                  |
| Interrupt Handler          | 中斷處理程式  | 當 I/O 完成或裝置發出事件時，CPU 會執行的處理程式。                                              |
| DMA = Direct Memory Access | 直接記憶體存取 | 讓裝置和 memory 直接傳資料，減少 CPU 一 byte 一 byte 搬資料的負擔。                              |

---

| 專有名詞                        | 中文         | 解釋                                                                         |
| --------------------------- | ---------- | -------------------------------------------------------------------------- |
| On-storage Structures       | 儲存裝置上的資料結構 | 長期存在 disk / SSD 上的 file-system structures，斷電後仍存在。                          |
| In-memory Structures        | 記憶體內資料結構   | OS 執行時載入 RAM 的資料結構，用來加速查找、追蹤 open files、cache metadata。                    |
| Boot Control Block          | 開機控制區塊     | 每個 volume 可能有的開機資訊區。UFS 稱為 boot block，NTFS 稱為 partition boot sector。       |
| Volume                      | 卷宗         | 一個 file system 所在的儲存單位，常對應一個 partition。                                    |
| Volume Control Block        | 卷宗控制區塊     | 記錄整個 volume 的資訊，例如 block size、block 數量、free blocks、free FCB 等。             |
| Superblock                  | 超級區塊       | UFS / UNIX 類 file system 中的 volume control block。它壞掉可能導致 file system 無法掛載。 |
| Master File Table, MFT      | 主檔案表       | NTFS 用來存放檔案與目錄 metadata 的核心資料結構。                                           |
| Mount Table                 | 掛載表        | 記錄目前系統掛載了哪些 volume / file system，例如 `/home` 對應哪個磁碟分割區。                     |
| Directory-Structure Cache   | 目錄結構快取     | 把最近查過的 directory entries 放在 memory 中，加速 pathname lookup。                   |
| System-wide Open-file Table | 系統共用開檔表    | 整個系統共用的 open file 資料表，保存每個開啟檔案的 FCB copy、offset、open count 等。              |
| Per-process Open-file Table | 每行程開檔表     | 每個 process 自己的 open file table。file descriptor 本質上就是這張表的 index。            |
| File Descriptor, fd         | 檔案描述符      | UNIX / Linux 中 process 操作檔案用的整數代號，例如 `fd = 3`。                             |
| File Handle                 | 檔案控制代號     | Windows 中類似 file descriptor 的概念。                                           |
| File Offset                 | 檔案偏移量      | 目前讀寫到檔案的哪個位置，例如下一次 `read()` 從第幾 byte 開始。                                   |
| Open Count                  | 開啟計數       | 記錄同一個檔案目前被幾個 process 或 entry 開著。歸零時才真正清理 system-wide open-file entry。      |
| Buffer                      | 緩衝區        | 暫存正在讀寫的 file-system blocks。                                                |
| Cache                       | 快取         | 暫存最近使用過的 metadata 或 data，避免每次都碰 disk。                                      |

---

| 專有名詞             | 中文   | 解釋                                                                    |
| ---------------- | ---- | --------------------------------------------------------------------- |
| Linear List      | 線性列表 | Directory implementation 的簡單方法，把 directory entries 排成一串，查檔案時從頭一個一個比對。 |
| Linear Search    | 線性搜尋 | 從頭掃到尾找目標。Directory 很大時會慢。                                             |
| Sorted List      | 排序列表 | Directory entries 依名稱排序，可用 binary search，但新增刪除可能要搬動 entries。          |
| Hash Table       | 雜湊表  | 用 hash function 把 file name 轉成 index，快速找到 directory entry。            |
| Hash Function    | 雜湊函數 | 把 file name 轉成 hash table index 的函數。                                  |
| Collision        | 碰撞   | 不同 file names 算出同一個 hash value / index。                               |
| Linear Probing   | 線性探測 | Collision 發生時，往下一格找空位。                                                |
| Chained Overflow | 鏈結溢位 | Collision 發生時，把同一個 hash index 的 entries 用 linked list 串起來。            |
| Rehashing        | 重新雜湊 | Hash table 擴大或 hash function 改變時，重新計算所有 entries 的位置。                  |

---

| 專有名詞                          | 中文          | 解釋                                                                   |
| ----------------------------- | ----------- | -------------------------------------------------------------------- |
| Allocation Methods            | 配置方法        | File system 把檔案分配到 disk blocks 的方法。主要有 contiguous、linked、indexed 三種。 |
| Contiguous Allocation         | 連續配置        | 每個 file 佔用一段連續 disk blocks。Directory 只需記 start block 和 length。       |
| Start Block                   | 起始區塊        | Contiguous allocation 中檔案第一個 block 的位置。                              |
| Length                        | 長度          | Contiguous allocation 中檔案佔用多少 blocks。                                |
| Sequential Access             | 循序存取        | 按照檔案順序一個 block 接著一個 block 讀。                                         |
| Direct Access / Random Access | 直接存取 / 隨機存取 | 直接讀檔案第 i 個 block，不必從頭讀到那裡。                                           |
| External Fragmentation        | 外部碎片        | Free space 被切成很多小洞，總空間夠但沒有足夠大的連續空間。                                  |
| Internal Fragmentation        | 內部碎片        | 分配給檔案的空間比實際使用多，剩下的部分浪費在 allocated region 裡。                          |
| First Fit                     | 首次適配        | 找到第一個夠大的 free hole 就分配。通常較快。                                         |
| Best Fit                      | 最佳適配        | 找最剛好的 free hole。可能留下很多很小的碎片。                                         |
| Compaction                    | 緊縮 / 壓縮     | 把已使用 blocks 搬到一起，讓 free space 變成一大段連續空間。成本很高。                        |
| Extent                        | 延伸區段        | 一段連續 blocks。Extent-based allocation 允許一個 file 由多段 extents 組成。        |

---

| 專有名詞                        | 中文              | 解釋                                                                                    |
| --------------------------- | --------------- | ------------------------------------------------------------------------------------- |
| Linked Allocation           | 鏈結配置            | 一個 file 的 blocks 可以散在 disk 各處，每個 block 存下一個 block 的 pointer。                          |
| Pointer                     | 指標              | 在 linked allocation 中用來指向下一個 block；在 indexed allocation 中用來指向 data block。             |
| EOF = End of File           | 檔案結尾            | 表示 linked list 或 FAT chain 到此結束。                                                      |
| Cluster                     | 叢集              | 多個 blocks 合成一組來配置，減少 pointer overhead，但可能增加 internal fragmentation。                   |
| FAT = File-Allocation Table | 檔案配置表           | 把「下一個 block 是誰」集中放在一張表中，例如 `FAT[217] = 618`。                                          |
| Indexed Allocation          | 索引配置            | 每個 file 有一個 index block，裡面集中存放該檔案所有 data block 的 physical block numbers。              |
| Index Block                 | 索引區塊            | 存放 data block addresses 的 block，不存檔案內容本身。                                             |
| Single Indirect Block       | 單層間接區塊          | inode 指向一個 index block，該 index block 再指向 data blocks。                                 |
| Double Indirect Block       | 雙層間接區塊          | inode 指向第一層 index block，第一層再指向第二層 index blocks，第二層才指向 data blocks。                    |
| Triple Indirect Block       | 三層間接區塊          | inode 經過三層 index blocks 才找到 data blocks，用於超大檔案。                                       |
| Direct Blocks               | 直接區塊指標          | inode 裡直接指向 data blocks 的 pointers。小檔案常靠 direct blocks 就能存完。                          |
| Combined Scheme             | 混合式配置           | UNIX inode 採用 direct blocks + single / double / triple indirect blocks，兼顧小檔案效率與大檔案容量。 |
| UNIX UFS-inode              | UNIX 檔案系統 inode | UFS 的 inode 結構，包含 metadata、direct blocks、indirect blocks。                             |

---

| 專有名詞                           | 中文                     | 解釋                                                              |
| ------------------------------ | ---------------------- | --------------------------------------------------------------- |
| Free-Space Management          | 空閒空間管理                 | 管理哪些 disk blocks 是 free，哪些已被 allocated。刪除檔案後的 blocks 要能重新使用。    |
| Free-Space List                | 空閒空間清單                 | 記錄所有 free blocks 的結構。名稱叫 list，但實作不一定真的是 linked list。            |
| Bit Vector / Bitmap            | 位元向量 / 位元圖             | 每個 block 用 1 bit 表示狀態。常見設定是 `1 = free`，`0 = allocated`。         |
| Allocate Block                 | 配置區塊                   | 把 free block 分配給某個檔案使用。Bitmap 中通常是 1 變 0。                       |
| Free Block                     | 釋放區塊                   | 檔案刪除後，原本的 data block 回到 free-space structure。Bitmap 中通常是 0 變 1。 |
| Linked Free-Space List         | 鏈結式空閒空間清單              | 把所有 free blocks 用 pointer 串起來，只需記住 free-space list head。        |
| Free-Space List Head           | 空閒清單頭                  | 指向第一個 free block 的 pointer。                                     |
| Coalescing Free Space          | 合併空閒空間                 | 把相鄰的 free blocks 合併成較大的連續空間。                                    |
| Inode Free List / Inode Bitmap | inode 空閒清單 / inode 位元圖 | 記錄哪些 inode 可重新分配給新檔案。                                           |

---

| 專有名詞                              | 中文       | 解釋                                                                                     |
| --------------------------------- | -------- | -------------------------------------------------------------------------------------- |
| Deleting a File                   | 刪除檔案     | 不只是刪檔名，還要更新 directory、inode、data blocks、indirect blocks、free list / bitmap 等 metadata。 |
| Directory Updated                 | 更新目錄     | 刪檔時要移除 `file name → inode number` 的 directory entry。                                   |
| Inode Cleared                     | 清除 inode | 把 inode 標記為 unused，使它之後可被新檔案使用。                                                        |
| Dead Data Blocks                  | 死資料區塊    | 資料可能仍在 disk 上，但已經沒有有效 inode 指向，file system 不再認得它們。                                     |
| Indirect Blocks Back to Free List | 間接區塊回收   | 大檔案使用的 single / double / triple indirect blocks 也要被釋放。                                 |
| Update Order Matters              | 更新順序很重要  | 不能在 inode 還指向 block 時就把 block 放回 free list，否則可能造成兩個檔案共用同一 block。                       |
| File-System Corruption            | 檔案系統毀損   | Metadata 關係錯亂，例如 inode 指向已被別人使用的 block。                                                |
| Space Leak                        | 空間洩漏     | 某些 blocks 沒有 inode 指向，也沒有在 free list 裡，等於空間消失。                                         |
| unlink                            | 取消連結     | UNIX / Linux 中刪除檔案名稱的操作。若還有 hard link 或 process 正在 open，資料 blocks 不一定馬上釋放。             |
| Link Count                        | 連結計數     | 記錄有多少 hard links 指向同一個 inode。歸零後才可能真正釋放檔案內容。                                           |

---

| 專有名詞                       | 中文           | 解釋                                                                          |
| -------------------------- | ------------ | --------------------------------------------------------------------------- |
| Recovery                   | 復原           | 系統當機、斷電或 file system 不一致後，修復 file system 的機制。                               |
| Consistency Checking       | 一致性檢查        | 掃描 directory、inode、free-space list、data blocks，找出並嘗試修復不一致。                  |
| fsck = File-System Check   | 檔案系統檢查工具     | UNIX / Linux 常見的一致性檢查工具。                                                    |
| Inconsistency              | 不一致          | File system metadata 互相矛盾，例如 inode 指向某 block，但該 block 又在 free list 中。       |
| Backup                     | 備份           | 把資料複製到另一個 storage device，避免 disk 壞掉或資料遺失。                                   |
| Restore                    | 還原           | 從 backup 把資料救回來。                                                            |
| Log / Journal              | 日誌           | 在真正修改 file-system structures 前，先記錄即將執行的 metadata updates。                   |
| Journaling File System     | 日誌式檔案系統      | 透過 log / journal 記錄 metadata transactions，讓 crash recovery 更快且較安全。          |
| Log Structured File System | 日誌結構式檔案系統    | 把更新以 log-like 形式寫入的 file-system 設計。課堂中常和 journaling 放在一起談 recovery。         |
| Transaction                | 交易           | 一組必須一起完成的 metadata updates。要嘛全部完成，要嘛當作沒有發生。                                 |
| Committed                  | 已提交          | Transaction 已完整寫入 log，系統承認這組操作應該被完成。                                        |
| Uncommitted                | 未提交          | Transaction 尚未完整寫入 log。Crash 後 recovery 會忽略它，不套用到真正 file-system structures。 |
| Redo                       | 重做           | Crash recovery 時，把已 committed 但尚未真正完成的 transaction 補做完。                     |
| Metadata Inconsistency     | Metadata 不一致 | File-system metadata 更新一半造成的錯誤狀態，journaling 主要就是為了降低這類問題。                   |

---

最重要的一條主線可以這樣背：

```text
file name
→ directory entry
→ inode / FCB
→ logical blocks
→ physical blocks
→ data blocks
```

刪除與 recovery 則可以這樣背：

```text
delete file
→ remove directory entry
→ release data blocks
→ release indirect blocks
→ clear inode
→ update free-space structure
→ update order matters
→ crash 時靠 consistency checking 或 journaling recovery
```

allocation 三兄弟可以這樣背：

```text
Contiguous allocation：
連續放，最快，但會 external fragmentation。

Linked allocation：
用 pointer 串，不怕外部碎片，但 random access 很慢。

Indexed allocation：
用 index block 查表，支援 random access，但有 index overhead。
```
