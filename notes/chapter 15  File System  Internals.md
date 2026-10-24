---
title: chapter 15  File System  Internals

---

# 15.5 Virtual File Systems(VFS)
![image](https://hackmd.io/_uploads/HJZqbfJGMl.png)
VFS = Virtual File System（虛擬檔案系統）。

這節的核心問題是：現代作業系統通常同時支援很多種 file system，例如 ext4、FAT、NTFS、ISO 9660、NFS 等等。可是對 application 來說，不可能每一種 file system 都用不同 API 操作，不然程式會爆炸難寫。

所以 OS 中間加了一層 VFS layer（虛擬檔案系統層），讓上層 application 都用一樣的介面，例如：
```
open()
read()
write()
close()
mmap()
```
但底下真正處理資料的 file system 可以是本機 ext4、USB 的 FAT32、光碟的 ISO 9660，甚至是網路上的 NFS。VFS 的作用就是把這些不同 file system 包裝成統一介面。
## 一、為什麼需要 VFS？
如果沒有 VFS，OS 可能要針對每一種 file system 各自寫一套完整流程：
```
open_ext4()
open_fat()
open_ntfs()
open_nfs()

read_ext4()
read_fat()
read_ntfs()
read_nfs()
```
這樣 application 和 kernel 都會變得很亂。

VFS 的做法是：上層永遠只看到標準 file-system interface，下層不同 file system 各自實作自己的細節。

所以使用者或 application 只需要：
```c
fd = open("/home/sj/a.txt", O_RDONLY);
read(fd, buffer, 100);
close(fd);
```
不需要知道 `/home/sj/a.txt` 到底位於 ext4、FAT32、NTFS，還是 NFS。這就是 VFS 的抽象能力。

## 二、Figure 15.5 在畫什麼？
圖中分成三層：
```
file-system interface
↓
VFS interface
↓
local file system type 1 / local file system type 2 / remote file system type 1
↓
disk / disk / network
```
最上層 file-system interface（檔案系統介面） 是 application 看得到的系統呼叫介面，例如：
```
open()
read()
write()
close()
file descriptor
```
中間層 VFS interface（虛擬檔案系統介面） 是 OS kernel 內部的抽象層。它負責把「一般化的檔案操作」轉交給正確的底層 file system。

最下層是實際 file system implementation，例如：
```
local file system type 1 → ext4 on disk
local file system type 2 → FAT32 on USB disk
remote file system type 1 → NFS over network
```
所以這張圖的意思是：**application 都透過同一套 file-system interface 操作檔案；VFS 在中間判斷這個檔案屬於哪種 file system，再呼叫對應的實作**。
## 三、VFS 的兩個主要功能
課本列出 VFS 的兩個重要功能。

第一，VFS 會把 **file-system-generic operations（檔案系統共通操作）** 和 **具體 file system implementation（實際檔案系統實作）** 分開。

例如 `read()` 這個動作對 application 來說都一樣，但對不同 file system 來說，底層流程可能不同：
```
ext4 read：
查 inode、extent、block group、journal metadata

FAT read：
查 FAT table，沿著 block chain 找資料

NFS read：
組成 network request，送到遠端 server
```
VFS 不需要知道這些細節。VFS 只負責說：「這是一個 read operation，請對應的 file system 去處理。」

第二，VFS 讓檔案可以在 network-wide environment 中被唯一表示。這裡課本提到 vnode。
## 四、vnode 是什麼？
**vnode = virtual node（虛擬節點）**。

在 UNIX 裡，inode = index node（索引節點） 通常只在單一 file system 內唯一。也就是說，同一台機器上不同 file system 可能都有 inode number 100，所以單靠 inode number 不一定能在整個系統或網路中唯一代表一個檔案。

VFS 用 vnode 來抽象表示一個 active file 或 directory。它可以包裝本地檔案，也可以包裝遠端檔案。

可以理解成：
```
inode：
偏向某個具體 local file system 裡的檔案節點

vnode：
VFS 層用來統一代表檔案 / 目錄的抽象節點
```
所以 vnode 比 inode 更抽象。它讓 VFS 可以不管檔案來自本機 disk 還是 network file system，都用類似方式管理。
## 五、Local file system 和 Remote file system
**Local file system（本機檔案系統）** 是資料存在本機 disk / SSD 上，例如 ext4、FAT32、NTFS。

**Remote file system（遠端檔案系統）** 是資料其實在其他機器上，例如：
```
NFS = Network File System（網路檔案系統）
```
NFS 讓一台電腦可以把遠端 server 的資料夾掛載到本機，看起來像本機目錄。

例如：
```
/mnt/nfs/project/a.txt
```
看起來像本機檔案，但實際讀取時可能會透過 network 傳 request 到遠端 server，再把資料傳回來。

對 application 來說，一樣是：
```c
read(fd, buffer, size);
```
但 VFS 下面可能不是讀 disk，而是呼叫 NFS protocol。
## 六、VFS 的三層架構
課本說 file-system implementation 通常可以分成三層：
```
1. File-system interface
2. VFS layer
3. File-system-specific implementation / remote-file-system protocol
```
第一層是 file-system interface，也就是 application 使用的 system calls，例如 open()、read()、write()、close()。

第二層是 VFS layer。它負責提供統一抽象，判斷這個檔案屬於哪個 file system，然後呼叫正確的底層函式。

第三層是具體實作。若是本機檔案，就呼叫 ext4、FAT、NTFS 等 file system 的操作；若是遠端檔案，就呼叫 NFS protocol 或其他 network file-system protocol。
## 七、Linux VFS 的四個主要 object
課本提到 Linux VFS 定義四種主要 object：
```
inode object
file object
superblock object
dentry object
```
這些 object 不是一般 OOP 語言中的 class object，而是 kernel 裡的資料結構。Linux kernel 用 C 寫成，所以通常是用 `struct` 加上 function pointer table 來達成類似物件導向的效果。
## 八、inode object

**inode object（索引節點物件）** 代表一個 individual file，也就是一個檔案或目錄本身。

它記錄檔案的核心 metadata，例如：

```text
file type
permissions
owner
size
timestamps
data block location
inode operations
```

注意，inode object 代表的是「檔案本體的 metadata」，不是「某次 open 的狀態」。

例如同一個檔案被兩個 process 同時 open，通常它們會對應到同一個 inode，但會有不同的 file object。

---

## 九、file object

**file object（檔案物件）** 代表一個 **open file（已開啟檔案）**。

這很重要：file object 不是單純代表 disk 上的檔案，而是代表「某個 process 開啟檔案後形成的使用狀態」。

它可能包含：

```text
current file offset
open mode
read / write state
pointer to inode
file operations table
```

例如：

```c
fd = open("a.txt", O_RDONLY);
```

這個 `open()` 會在 kernel 裡建立一個 file object。之後 `read(fd, ...)` 會透過 file descriptor 找到這個 file object，再進一步找到 inode 和底層 file system operation。

---

## 十、superblock object

**superblock object（超級區塊物件）** 代表一整個 mounted file system。

它不是代表單一檔案，而是代表整個 file system instance。例如 `/home` 掛載的是一個 ext4 file system，那這個 ext4 file system 在 memory 中就會有一個 superblock object。

它通常記錄：

```text
file system type
block size
mount information
root dentry
superblock operations
free-space metadata
```

可以理解成：**inode 是單一檔案的 metadata；superblock 是整個 file system 的 metadata。**

---

## 十一、dentry object

**dentry = directory entry（目錄項目）**。

**dentry object（目錄項目物件）** 代表一個 path component 和 inode 的對應關係。

例如路徑：

```text
/home/sj/a.txt
```

會拆成：

```text
/
home
sj
a.txt
```

每一層名字都可能有 dentry object。Dentry 主要用來加速 pathname lookup。

它的概念是：

```text
file name component → inode
```

例如：

```text
"a.txt" → inode 12345
```

Dentry cache 很重要，因為每次 `open("/home/sj/a.txt")` 如果都從 disk 查 directory，會很慢。VFS 可以把常用 dentry 留在 memory 裡，下次查路徑就快很多。

---

## 十二、VFS 的 function table 是什麼？

課本提到，每種 VFS object 都有一組 operations。每個 object 裡面會有 pointer 指向 function table。這就是 kernel 用 C 模擬物件導向的地方。

例如 file object 可能有一張 function table，裡面放：

```text
open
close
read
write
mmap
```

不同 file system 會提供不同實作。

例如同樣是 `read()`：

```text
ext4 的 file object
→ read pointer 指向 ext4_read()

FAT 的 file object
→ read pointer 指向 fat_read()

NFS 的 file object
→ read pointer 指向 nfs_read()
```

VFS 上層只需要呼叫：

```text
file->operations->read(...)
```

它不需要知道底下到底是 ext4、FAT 還是 NFS。這就是 **dynamic dispatch（動態分派）** 的概念。

---

## 十三、用 read() 流程理解 VFS

假設 application 執行：

```c
read(fd, buffer, size);
```

整體概念如下：

```text
application
→ read(fd, buffer, size)
→ system call 進入 kernel
→ VFS 根據 fd 找到 file object
→ file object 指向 inode / dentry / superblock
→ VFS 呼叫 file object 的 read function pointer
→ 對應 file system 執行真正 read
```

如果是 ext4：

```text
VFS → ext4_read() → 查 inode / extent → block I/O → disk
```

如果是 NFS：

```text
VFS → nfs_read() → 透過 network 發 request → remote server 回傳資料
```

對 application 來說，兩者完全一樣，都是 `read()`。

---

## 十四、VFS 不知道也不在乎底層細節

投影片最後一句很重要：

VFS 不知道，也不需要在乎，某個 inode 代表的是：

```text
disk file
directory file
remote file
```

因為正確的 read function 已經放在該 object 的 function table 裡。VFS 只要呼叫那個 function pointer，真正的底層 file system 會自己處理。

這就是 VFS 的核心價值：

```text
同一套 system call interface
不同 file system 各自實作
VFS 負責中間抽象與轉接
```

---

## 十五、完整例子：同一個 read，不同底層

假設有三個檔案：

```text
/home/sj/a.txt        → ext4
/mnt/usb/b.txt        → FAT32
/mnt/server/c.txt     → NFS
```

application 都可以用：

```c
fd = open(path, O_RDONLY);
read(fd, buffer, 100);
close(fd);
```

但 kernel 內部可能是：

```text
/home/sj/a.txt
→ VFS
→ ext4 operations
→ local disk

/mnt/usb/b.txt
→ VFS
→ FAT32 operations
→ USB storage

/mnt/server/c.txt
→ VFS
→ NFS operations
→ network
```

所以 VFS 讓「不同 file system」看起來像「同一套檔案操作」。

---

## 十六、VFS 和前面章節的關係

前面 14 章談的是 file system 怎麼實作，例如：

```text
directory implementation
allocation methods
free-space management
inode
superblock
open-file table
```

15.5 的 VFS 則是更上一層的整合問題：**當 OS 同時支援很多 file system 時，如何用統一架構管理它們。**

所以可以這樣接起來：

```text
Chapter 14：
單一 file system 內部怎麼配置 blocks、管理 inode、管理 free space。

Chapter 15.5：
OS 如何同時支援多種 file systems，並讓 application 用同一套 API 操作。
```

---

## 一句話總結

**Virtual File System（VFS）是 kernel 中的抽象層，用統一的 file-system interface 包裝不同 file system。Application 只需要呼叫 open、read、write、close；VFS 透過 inode object、file object、superblock object、dentry object，以及各 object 的 operation function table，把操作分派給 ext4、FAT、NTFS、NFS 等實際 file system。這讓本機檔案、遠端檔案、不同格式的 file system 都能被整合到同一棵 directory tree 裡。**



# VFS 補充
## VFS 具體怎麼實作？

**VFS = Virtual File System（虛擬檔案系統）** 的實作核心其實只有一句話：

**VFS 在 kernel 裡建立一套共同的資料結構，然後每個真正的 file system 把自己的函式掛到這些資料結構上。**

也就是：

```text
共同資料結構 + function pointer table
```

---

## 1. VFS 先定義「共同格式」

不管底下是 ext4、FAT、NTFS、NFS，VFS 都希望它們看起來像同一種東西。

所以 kernel 會定義幾種共同 object：

```text
superblock object  → 代表整個 file system
inode object       → 代表一個檔案本體
dentry object      → 代表檔名到 inode 的對應
file object        → 代表一次 open 出來的檔案狀態
```

其中考試最常用到的是：

```text
file object + inode object
```

---

## 2. 每個 object 裡面會放 function table

例如一個 `file object` 裡面會有一個指標，指向這個檔案可以執行哪些操作：

```text
file object
→ file_operations
    → open
    → read
    → write
    → mmap
    → close
```

重點是：**不同 file system 會提供不同版本的 function。**

例如：

```text
ext4 file_operations
→ ext4_read()
→ ext4_write()

FAT file_operations
→ fat_read()
→ fat_write()

NFS file_operations
→ nfs_read()
→ nfs_write()
```

所以 VFS 不需要知道 ext4 或 FAT 內部怎麼讀資料。
VFS 只要知道：「這個 file object 的 read function 在哪裡」。

---

## 3. open() 時決定這個檔案屬於哪個 file system

假設執行：

```text
open("/mnt/usb/a.txt")
```

如果 `/mnt/usb` 是 FAT32 掛載點，VFS 在 pathname lookup 時會知道：

```text
/mnt/usb/a.txt
→ 屬於 FAT32 file system
```

所以 VFS 會建立一個 `file object`，並讓它的 operation table 指向 FAT32 的函式：

```text
file object
→ FAT32 file_operations
```

之後這個檔案的 `read()`、`write()` 就會走 FAT32 的版本。

---

## 4. read() 時直接透過 function pointer 呼叫

之後執行：

```text
read(fd, buffer, size)
```

流程就變成：

```text
fd
→ per-process open-file table
→ file object
→ file_operations
→ read function
```

如果這個檔案是 ext4：

```text
read(fd)
→ file->operations->read()
→ ext4_read()
```

如果這個檔案是 FAT：

```text
read(fd)
→ file->operations->read()
→ fat_read()
```

如果這個檔案是 NFS：

```text
read(fd)
→ file->operations->read()
→ nfs_read()
```

所以 VFS 的關鍵不是「自己讀資料」，而是：

```text
找到正確的 file object
呼叫它掛著的 read function
```

---

## 最核心的圖像

```text
Application
    |
    | open(), read(), write()
    v
VFS
    |
    | 根據 file object 的 operation table
    v
ext4 / FAT / NTFS / NFS
    |
    v
disk / USB / network
```

---

## 最精簡考試版

**VFS 的具體實作是利用共同的 kernel object 和 function pointer table。Linux VFS 定義 file object、inode object、superblock object、dentry object 等抽象資料結構。不同 file system 會實作自己的 open、read、write 等函式，並把它們放入 operation table。當 process 呼叫 read() 時，VFS 透過 file descriptor 找到 file object，再透過 file object 中的 function pointer 呼叫實際 file system 的 read function。因此同一個 read() 介面可以對應到 ext4_read()、fat_read() 或 nfs_read()。**

一句話記：

```text
VFS = 用共同 object 包裝不同 file system，再用 function pointer 分派操作。
```

# Remote File systems
![image](https://hackmd.io/_uploads/Sy3-IG1Gfg.png)
這兩個紅字是：

```text
LDAP = Lightweight Directory Access Protocol
IETF = Internet Engineering Task Force
```

---

## LDAP = Lightweight Directory Access Protocol

**LDAP（輕量級目錄存取協定）** 是一種用來「查詢與管理目錄服務」的網路協定。

這裡的 **directory（目錄）** 不是 file system 裡的資料夾，而比較像一個「組織資訊資料庫」。它可以存：

```text
使用者帳號
群組
部門
Email
密碼驗證資訊
權限資訊
電腦設備
印表機
網路資源
```

所以 LDAP 常用在公司、學校、組織內部，用來做身分認證與資源管理。

例如登入學校系統時，系統可能會問：

```text
這個帳號存在嗎？
密碼對嗎？
這個使用者屬於哪個群組？
有沒有權限使用某個服務？
```

這些查詢就可以透過 LDAP 完成。

---

## LDAP 為什麼叫 Lightweight？

**Lightweight** 是「輕量級」的意思。

早期有一套比較複雜的目錄協定叫 **X.500 Directory Access Protocol**。LDAP 是比較簡化、比較適合 TCP/IP 網路環境的版本，所以叫 **Lightweight Directory Access Protocol**。

簡單講：

```text
LDAP = 用比較輕量的方式，讓 client 查詢 directory server 裡的組織資料
```

---

## Active Directory 和 LDAP 的關係

投影片前面提到 **Microsoft Active Directory**，它是一種 directory service。

可以這樣理解：

```text
Active Directory = 微軟做的目錄服務系統
LDAP = 可以拿來存取這類目錄服務的協定
```

所以 LDAP 比較像「溝通規則」，Active Directory 比較像「實際系統」。

---

## IETF = Internet Engineering Task Force

**IETF（網際網路工程任務組）** 是負責制定許多 Internet 標準的組織。

它不是公司，而是一個開放的技術標準組織。很多網路協定都是由 IETF 制定或維護，例如：

```text
TCP/IP 相關標準
HTTP
SMTP
DNS 相關標準
LDAP
TLS
```

IETF 通常會把標準寫成文件，稱為：

```text
RFC = Request For Comments
```

所以當投影片說：

```text
LDAP of the Internet Engineering Task Force (IETF)
```

意思是：

**LDAP 是由 IETF 標準化的網路協定。**

---

## 一句話整理

**LDAP 是一種用來查詢與管理 directory service 的網路協定，例如查帳號、群組、權限；IETF 則是制定 LDAP 等網際網路標準的組織。**

# 15.8 NFS

## 0. 本節核心觀念

**NFS = Network File System（網路檔案系統）**。NFS 的目標是讓不同機器上的檔案系統可以透過網路共享，並且讓使用者在操作遠端檔案時，看起來就像操作本機檔案一樣。

NFS 通常被整合到整體 directory structure 裡。也就是說，遠端 server 上的某個 directory 可以被掛載到 client 本機的某個路徑下。掛載完成後，client 上的程式可以用一般的 `open()`、`read()`、`write()`、`close()` 等 system calls 存取遠端檔案，不需要知道資料實際上在另一台機器。

NFS 是一種 **client-server network file system（客戶端－伺服器式網路檔案系統）**。某台機器可以是 client，也可以同時是 server。Client 負責提出檔案操作請求；server 負責提供遠端檔案系統的資料與操作服務。

---

## 1. NFS 的基本架構

NFS 讓多台機器各自保有自己的 local file system，但又可以選擇性地把部分目錄分享出去。Client 可以把 server export 出來的 remote directory 掛載到本機目錄樹中的某個位置。

例如 server `S1` 把 `/usr/shared` 分享出來，client `U` 可以把它掛載到：

```text
/usr/local
```

掛載完成後，client `U` 上的使用者看到的是：

```text
/usr/local/dir1
```

但實際上這些資料可能存放在 server `S1` 的：

```text
/usr/shared/dir1
```

重點是：**mount 後，遠端目錄會變成 client 本機 directory tree 的一部分。**

---

## 2. Figure 15.6：Three Independent File Systems
![image](https://hackmd.io/_uploads/SJfSdM1fMe.png)
這張圖表示三台機器 `U`、`S1`、`S2` 一開始各自有自己的檔案系統。它們彼此獨立，每台機器只能直接看到自己的 local files。

在還沒有 NFS mount 之前：

```text
U  只能看到 U 自己的 /usr/local
S1 只能看到 S1 自己的 /usr/shared/dir1
S2 只能看到 S2 自己的 /usr/dir2
```

也就是說，每台機器的 directory tree 是分開的，沒有互相整合。

---

## 3. Figure 15.7：Mounting in NFS

![image](https://hackmd.io/_uploads/ByKEufyfzg.png)
這張圖表示 `U` 把 `S1` 上的某個 remote directory 掛載到自己的 `/usr/local` 位置。掛載後，`U` 原本的 `/usr/local` 內容會被 remote directory 覆蓋在同一個 pathname 上。

此時對 `U` 的使用者來說，遠端資料就像本機資料：

```text
U:/usr/local/dir1
```

但實際來源是：

```text
S1:/usr/shared/dir1
```

這就是 NFS 的 transparent access（透明存取）：使用者不用特別知道資料在遠端 server 上。

---

## 4. Figure 15.7：Cascading Mounts

![image](https://hackmd.io/_uploads/ryI4_GyGzg.png)
**Cascading mount（串接掛載）** 是指一個已經掛載進來的 remote file system 裡面，又包含另一個 mount point。

例如：

```text
U 掛載 S1 的某個 directory
S1 的該 directory 裡又掛載了 S2 的某個 directory
```

結果 `U` 可能可以透過一條 pathname 間接看到 `S2` 的資料。

課本強調，這種 mount mechanism 不具有 transitivity（傳遞性）。也就是說，某台機器掛載某個 remote file system，不代表所有其他機器都會自動得到相同的掛載結果。每台 client 的 mount namespace 可以不同。

---

## 5. NFS 和 VFS 的關係

NFS 通常透過 **VFS = Virtual File System（虛擬檔案系統）** 整合進 OS。

對 application 來說，不管檔案在本機 ext4、FAT，還是在遠端 NFS 上，都可以使用同一套介面：

```text
open()
read()
write()
close()
```

VFS 會根據該檔案所在的 file system，把操作分派到對應的實作。如果檔案屬於 NFS，VFS 就會呼叫 NFS client 的操作函式，進一步透過網路向 NFS server 發出請求。

可以整理成：

```text
Application
→ system call
→ VFS
→ NFS client
→ RPC/XDR
→ network
→ NFS server
→ server-side VFS
→ server local file system
→ disk
```

---

## 6. Figure 15.8：NFS Architecture

![image](https://hackmd.io/_uploads/H1-QOf1MMe.png)
這張圖是 NFS 架構最重要的圖。左邊是 client，右邊是 server。

在 client 端：

```text
system-calls interface
→ VFS interface
→ NFS client
→ RPC/XDR
→ network
```

在 server 端：

```text
network
→ RPC/XDR
→ NFS server
→ VFS interface
→ UNIX file system
→ disk
```

這張圖的重點是：**client 和 server 其實都有 VFS。**

Client 的 VFS 看到這是一個 NFS 檔案操作後，會把請求交給 NFS client。NFS client 把操作轉成 RPC request，透過網路送到 server。Server 的 NFS server 收到 RPC request 後，再透過 server 端的 VFS 去操作真正的 local file system。

因此，NFS 不是讓 client 直接操作 server disk，而是：

```text
client 發出 remote file operation request
server 接收 request 並代為執行 file operation
server 再把結果回傳給 client
```

---

## 7. RPC 和 XDR

NFS 使用 **RPC = Remote Procedure Call（遠端程序呼叫）** 來實作遠端操作。

RPC 的想法是：client 可以像呼叫本地函式一樣，呼叫 server 上的遠端服務。實際上，這個呼叫會被包裝成網路封包送到 server。

NFS 也使用 **XDR = External Data Representation（外部資料表示法）**。XDR 用來處理不同機器之間的資料格式差異。例如不同 CPU 架構可能有不同 byte order，XDR 可以把資料轉成共同格式，讓異質系統之間可以正確溝通。

所以 NFS 能跨不同硬體、不同 OS、不同網路架構運作，原因之一就是 RPC/XDR 提供了標準化的遠端呼叫與資料表示方式。

---

## 8. 15.8.2 The Mount Protocol

**Mount protocol（掛載協定）** 負責建立 client 和 server 之間的初始邏輯連線。

當 client 想掛載 server 的某個 remote directory 時，client 會發出 mount request。這個 request 會包含：

```text
remote directory name
server name / host name
local mount point
```

Server 端會檢查自己的 **export list（匯出清單）**。Export list 記錄 server 允許哪些 local file systems 或 directories 被遠端 client 掛載，以及哪些 client 有權限掛載。

若 mount request 符合 export list，server 會回傳一個 **file handle（檔案控制代號）** 給 client。

---

## 9. File Handle

NFS 的 **file handle** 是 client 後續操作遠端檔案時使用的重要識別資料。

File handle 需要能夠唯一識別 server 上的某個檔案或目錄。一般來說，它可能包含：

```text
file-system identifier
inode number
generation number
```

其中 file-system identifier 用來指出是哪一個 exported file system；inode number 用來指出該 file system 中的特定檔案；generation number 可用來避免 inode 被刪除後又重用造成混淆。

Mount 完成後，client 後續的 NFS operation 通常會帶著 file handle，server 就能知道 client 要操作哪個遠端檔案或目錄。

---

## 10. Export List

**Export list（匯出清單）** 是 server 端用來控制哪些檔案系統可以被分享出去的清單。

它包含：

```text
server 要分享哪些 directory / file system
哪些 client 可以 mount
有哪些存取限制
```

在 UNIX 類系統中，這類設定常見於 export 設定檔，例如 `/etc/exports`。課本以 Solaris 為例時提到 `/etc/dfs/dfstab`。

Export list 的功能類似「server 對外開放目錄的授權表」。

---

## 11. 15.8.3 The NFS Protocol

**NFS protocol（NFS 協定）** 定義遠端檔案操作的 RPC procedures。它支援的操作包含：

```text
在 directory 中搜尋檔案
讀取 directory entries
操作 links 和 directories
取得 file attributes
讀取檔案
寫入檔案
```

但 NFS protocol 有一個非常重要的特色：**NFS server 是 stateless（無狀態）的。**

---

## 12. Stateless Server

**Stateless server（無狀態伺服器）** 表示 server 不保存 client 的操作狀態。

傳統 local file system 會有 open-file table，記錄某個 process 開了哪些檔案、目前 offset 在哪裡。但 NFS server 不維護 client 的 open-file state。

因此，NFS protocol 故意省略 `open()` 和 `close()` 的遠端操作。Client 每次發出的 request 都必須包含足夠資訊，使 server 可以獨立完成該次操作。

例如讀取遠端檔案時，request 需要包含：

```text
file handle
offset
read length
```

這樣 server 不需要記得「client 上次讀到哪裡」，每次 request 都能單獨處理。

---

## 13. Stateless 的好處與代價

Stateless 的最大好處是 crash recovery 容易。若 server crash 後重新啟動，server 不需要重建每個 client 的 open-file state。Client 只要重新送 request，server 就可以繼續處理。

但 stateless 也帶來一些限制。

首先，NFS 不容易完整模擬 UNIX local file system 的語意。例如 UNIX local file system 可以依賴 open file state、file lock、concurrent access control；但這些狀態管理會讓 server 變成 stateful，因此早期 NFS 避免在核心 protocol 中支援。

其次，每個 request 必須帶完整資訊，例如 file handle 和 absolute offset，request 會比較冗長。

再者，為了避免重複 request 造成錯誤，NFS operation 需要具備 **idempotent（冪等性）**。

---

## 14. Idempotent Operation

**Idempotent operation（冪等操作）** 是指同一個操作執行一次或多次，結果應該相同。

NFS 需要這個特性，是因為網路可能會遺失封包。Client 若沒有收到 server 回覆，可能會重送 request。此時 server 可能其實已經執行過第一次 request。

若 operation 是 idempotent，即使重送，也不會造成錯誤結果。

例如讀取同一個 file handle、同一個 offset、同一個 length，重複讀多次通常結果相同。

但 write 比較麻煩。NFS 需要特別設計 sequence number 或其他機制來判斷 duplicated request，避免重複寫入造成錯誤。

---

## 15. Write Semantics

NFS 中，write 的一致性語意比較複雜。

課本提到，因為 NFS server 是 stateless，所以當 server 回傳 write 成功給 client 時，通常需要保證資料已經安全寫到 server disk，或至少寫到 nonvolatile cache。否則 server crash 後，client 以為寫入成功，但資料其實遺失。

因此，NFS 的 write 效能可能會受到影響，因為同步寫入 disk 比只寫 memory cache 慢。

有些系統使用 nonvolatile cache 改善效能。資料先寫入不會因斷電消失的 cache，再由系統之後慢慢寫回 disk。

---

## 16. 15.8.4 Path-Name Translation

**Path-name translation（路徑名稱轉換）** 是指把 pathname 解析成遠端檔案或目錄的過程。

例如：

```text
/usr/local/dir1/file.txt
```

會被拆成多個 component：

```text
usr
local
dir1
file.txt
```

在 NFS 中，client 會逐段查找 pathname。當路徑跨過 mount point 時，client 就知道接下來要對遠端 server 發出 NFS lookup request。

NFS 不會一次把整條 pathname 送給 server 處理，因為每台 client 的 mount layout 可能不同。同一條 pathname 在不同 client 上不一定代表同一個遠端位置。

因此，path-name translation 通常是由 client 端逐段處理，server 只處理某個 directory vnode / file handle 底下的 lookup。

---

## 17. Directory-Name-Lookup Cache

NFS client 會使用 **directory-name-lookup cache（目錄名稱查找快取）** 加速 pathname lookup。

當 client 查過某個 remote directory name 對應的 vnode / file handle 後，可以暫存在 cache 中。下次查同一個 pathname component 時，就不用再立刻向 server 發出 RPC request。

這可以減少網路往返次數，提高效能。

但 cache 也可能造成一致性問題。如果 server 上的 directory 已經改變，而 client 還使用舊 cache，就可能看到過期資訊。因此 NFS 需要透過 timeout、attribute revalidation 等方法降低 stale cache 的問題。

---

## 18. 15.8.5 Remote Operations

NFS 的 remote operations 大致遵循 remote-service paradigm。也就是 client 的某些 file system calls 會被轉成對應的 RPC request。

例如：

```text
read()   → NFS read RPC
write()  → NFS write RPC
lookup() → NFS lookup RPC
getattr() → NFS get attributes RPC
```

但並不是每個 UNIX system call 都一對一對應到一個 NFS RPC。部分操作可能由 client 端 cache、buffering 或 VFS 處理。

---

## 19. NFS 的兩種 Cache

課本提到 NFS client 端有兩種重要 cache：

```text
file-attribute cache
file-block cache
```

**File-attribute cache（檔案屬性快取）** 儲存 inode-like information，例如檔案大小、timestamps、permissions 等。當檔案被 open 時，client 可以先檢查 cache 中的 attributes 是否仍有效。

**File-block cache（檔案區塊快取）** 儲存遠端檔案的資料 blocks。若同一段資料短時間內再次被讀取，就可以直接從 client cache 回傳，不必再次透過網路向 server 要資料。

Cache 可以提升效能，但也會讓 NFS 的 consistency semantics 較難保證。

---

## 20. NFS 的一致性問題

因為 NFS 允許不同 client 同時存取同一份遠端檔案，所以 cache 可能造成不一致。

例如：

```text
Client A 修改 server 上的 file.txt
Client B 的 cache 中仍保留舊版 file.txt
```

此時 Client B 可能短時間內看到舊資料。

因此，NFS 通常不提供完全等同 local UNIX file system 的一致性語意。它採用較弱的一致性模型，透過 timeout、revalidation、flush 等方式在效能和一致性之間折衷。

課本也提到，NFS 的實用性與效能很好，因此即使語意不是完全嚴格，仍然被廣泛使用。

---

## 21. NFS 的重點流程總結

### Mount 階段

```text
client 發出 mount request
→ server 檢查 export list
→ 若允許，server 回傳 file handle
→ client 把 remote directory 接到 local mount point
```

### Open / Lookup 階段

```text
application 呼叫 open(path)
→ VFS 解析 pathname
→ 遇到 NFS mount point
→ NFS client 發出 lookup RPC
→ server 回傳目標 file handle / attributes
```

### Read 階段

```text
application 呼叫 read(fd)
→ VFS 找到 NFS file object
→ NFS client 產生 read RPC
→ RPC/XDR 透過 network 傳給 server
→ NFS server 經由 server VFS 讀 local file system
→ server 回傳資料
→ client 把資料交給 application
```

### Write 階段

```text
application 呼叫 write(fd)
→ VFS 找到 NFS file object
→ NFS client 產生 write RPC
→ server 寫入 local file system / stable storage
→ server 回傳結果
```

---

## 22. NFS 的優缺點整理

### 優點

```text
遠端檔案看起來像本機檔案
整合進同一棵 directory tree
支援異質系統之間共享檔案
透過 VFS 與一般 file system interface 結合
stateless server 讓 crash recovery 較簡單
client-side cache 可提升效能
```

### 缺點

```text
網路延遲會影響效能
cache 可能造成一致性問題
stateless 設計使 locking、open-file state、concurrent access 較難處理
write 需要穩定儲存保證時效能較差
不完全符合 local UNIX file system semantics
```

---

## 23. 考試常考觀念

### NFS 是什麼？

NFS 是一種 client-server network file system，讓 client 可以把 server 上的 remote directory 掛載到本機 directory tree 中，並使用一般檔案操作透明地存取遠端檔案。

### NFS 和 VFS 的關係？

NFS 透過 VFS 整合到 OS。Application 使用一般 system calls，VFS 判斷該檔案屬於 NFS 後，將操作交給 NFS client，再透過 RPC/XDR 傳到 server。

### Mount protocol 做什麼？

Mount protocol 建立 client 和 server 之間的初始邏輯連線。Server 檢查 export list 後，回傳 file handle 給 client。

### NFS protocol 做什麼？

NFS protocol 定義真正的遠端檔案操作，例如 lookup、read directory entries、get attributes、read、write 等。

### 為什麼 NFS server 是 stateless？

為了讓 server crash recovery 簡單。Server 不保存 client open-file state，每個 request 都帶足夠資訊獨立完成。

### Stateless 的代價？

Request 需要攜帶較多資訊，open / close 不在 NFS protocol 中，file locking 和一致性語意較難完整支援。

### 為什麼需要 idempotent？

網路 request 可能遺失或重送，idempotent operation 可避免重複執行造成不同結果。

### Path-name translation 為什麼在 client 端做？

因為每台 client 的 mount layout 可能不同，server 不一定知道 client 上完整 pathname 的意義。

### NFS cache 有什麼問題？

Cache 提升效能，但可能造成 client 看到過期資料，因此 NFS 的一致性通常比 local file system 弱。

---

## 24. 專有名詞速查

| 名詞 | 全稱 / 中文 | 重點 |
|---|---|---|
| NFS | Network File System，網路檔案系統 | 讓遠端檔案系統像本機檔案系統一樣被存取 |
| VFS | Virtual File System，虛擬檔案系統 | OS 中整合不同 file system 的抽象層 |
| RPC | Remote Procedure Call，遠端程序呼叫 | Client 呼叫 server 上遠端服務的機制 |
| XDR | External Data Representation，外部資料表示法 | 跨不同機器傳送資料時使用的共同資料格式 |
| Mount protocol | 掛載協定 | 建立 client 與 server 的初始掛載關係 |
| NFS protocol | NFS 協定 | 定義 lookup、read、write 等遠端檔案操作 |
| Export list | 匯出清單 | Server 端列出允許被遠端掛載的 directories / file systems |
| File handle | 檔案控制代號 | NFS 用來唯一識別遠端檔案或目錄 |
| Stateless | 無狀態 | Server 不保存 client 的 open-file state |
| Idempotent | 冪等 | 同一操作執行一次或多次，結果相同 |
| Path-name translation | 路徑名稱轉換 | Client 端逐段解析 pathname 並進行遠端 lookup |
| Attribute cache | 屬性快取 | 快取 file size、timestamps、permissions 等 |
| File-block cache | 檔案區塊快取 | 快取遠端檔案資料 block |
| Cascading mount | 串接掛載 | 掛載進來的 remote file system 中又包含另一個 mount point |
| Transparent access | 透明存取 | 遠端檔案看起來像本機檔案 |

---

## 25. 一句話總結

NFS 讓遠端檔案系統可以被掛載到本機 directory tree 中，使 application 透過一般 `open()`、`read()`、`write()` 操作遠端檔案。它透過 VFS 整合進 OS，透過 RPC/XDR 在 client 和 server 間傳送檔案操作。Mount protocol 負責建立遠端掛載並取得 file handle；NFS protocol 負責 lookup、read、write 等遠端操作。NFS server 採 stateless 設計以簡化 crash recovery，但也造成 cache consistency、locking、write semantics 等問題。
