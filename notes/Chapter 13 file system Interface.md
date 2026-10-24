---
title: Chapter 13 file system Interface

---

# 13.3 Directory Structure
## 前言
核心是在說：**directory（目錄）本質上可以看成一張 symbol table（符號表）**，負責把「人看得懂的 file name」轉換成 OS 管理檔案所需的內部資訊。

更白話講，檔案系統底層不是真的靠檔名在找資料，而是靠一些內部結構，例如 **FCB = File Control Block（檔案控制區塊）** 或 **inode**。Directory 的工作就是建立這種對應關係：
```
file name
→ directory entry
→ file control block / inode
→ 檔案大小、權限、位置、資料 block 等資訊
```
所以 directory 不是單純「資料夾畫面」，而是一種資料結構。它裡面放很多 **directory entries（目錄項目）**，每個 entry 通常記錄一個檔名，以及該檔案對應到哪個檔案控制結構。

---

![image](https://hackmd.io/_uploads/HySpOwabzg.png)
圖 13.6 可以拿來類比這件事。左邊的 index file 像是一張索引表，用 last name 找到右邊 relative file 裡真正的 record。例如看到 Smith，就能找到右邊對應的 smith, john 資料。Directory 也類似：用檔名當 key，找到該檔案真正的控制資訊。

---

這段也列出 directory 必須支援的基本操作。

**Search for a file：搜尋檔案**。
Directory 要能根據檔名找到對應 entry，也可能支援 pattern matching，例如找出所有 .txt 或名字符合某種格式的檔案。

**Create a file：建立檔案**。
新檔案建立時，directory 裡要新增一筆 entry，把新檔名放進目錄結構中。

**Delete a file：刪除檔案**。
刪除檔案時，要把 directory entry 移除。不過刪除後可能會留下洞，所以有些系統需要整理目錄結構，類似 defragmentation。

**List a directory：列出目錄**。
也就是像 `ls` 或檔案總管那樣，列出 directory 裡有哪些檔案，以及每個 entry 的資訊。

**Rename a file：重新命名檔案**。
因為檔名是使用者理解檔案內容的重要符號，所以 directory 要能修改檔名。有些系統中，rename 也可能改變檔案在 directory structure 中的位置。

**Traverse the file system：走訪整個檔案系統**。
也就是從某個目錄開始，遞迴掃過所有 subdirectories 和 files。這常用於 backup、搜尋、磁碟檢查、統計空間使用量等。

**Directory 是 file system 中負責「檔名 → 檔案內部控制資訊」轉換的資料結構；好的 directory structure 必須支援搜尋、建立、刪除、列出、重新命名與走訪整個檔案系統。**
## 13.3.1 Single-Level Directory（單層目錄）
是最簡單的 directory structure。它的做法是：**整個系統所有檔案都放在同一個 directory 裡面**，沒有子目錄、沒有資料夾階層。

概念像這樣：
```
directory
├── cat
├── bo
├── a
├── test
├── data
├── mail
├── cont
├── hex
└── records
```
![image](https://hackmd.io/_uploads/HJR2Fv6bGg.png)
圖 13.7 中，上面那一整條就是唯一的 directory，每個 entry 都對應到一個 file。也就是說，所有檔案名稱都集中放在同一張表裡。

這種設計的優點是**非常簡單**。OS 很容易實作，使用者也很容易理解。找檔案時只要在唯一的 directory 裡搜尋；新增檔案就新增一筆 entry；刪除檔案就移除一筆 entry。

但缺點也很明顯：**所有檔案名稱必須唯一**。因為沒有不同資料夾可以區隔，所以不能有兩個檔案都叫 `test.txt`。如果很多使用者共用同一個系統，就很容易撞名。例如兩個學生都想建立 `prog2.c`，系統就無法分辨是哪一個人的檔案。

另一個問題是：**檔案一多就很難管理**。全部檔案混在同一層，沒有分類、沒有階層，使用者要記住所有檔名會很痛苦。就像電腦桌面上放了幾百個檔案，完全沒有資料夾分類，找東西會直接爆炸。**因此它只適合很小型、單使用者或檔案數量很少的系統**。
## 13.3.2 Two-Level Directory（雙層目錄）
### 目的
是在解決前面 Single-Level Directory（單層目錄） 的最大問題：所有檔案都放在同一個 directory，檔名容易衝突，也不好管理。

雙層目錄的做法是：**每個 user 都有自己的 directory**，所以檔名只需要在「自己的目錄內」唯一，不需要在整個系統唯一。
### 一、兩層分別是什麼？
這裡有兩個重要名詞：

**MFD = Master File Directory（主檔案目錄）**
這是第一層，負責記錄每個 user。可以想成「使用者總表」。

**UFD = User File Directory（使用者檔案目錄）**
這是第二層，每個 user 都有自己的 UFD，裡面放該 user 的檔案。

結構像這樣：
```
MFD
├── user 1 → UFD of user 1 → cat, bo, a, test
├── user 2 → UFD of user 2 → a, data
├── user 3 → UFD of user 3 → a, test
└── user 4 → UFD of user 4 → x, data, a
```
![image](https://hackmd.io/_uploads/ryX8sva-Gx.png)
圖 13.8 裡，上面那排 `user 1, user 2, user 3, user 4` 就是 **MFD**。下面每一排藍色小格子就是各自的 **UFD**。
### 二、為什麼可以解決檔名衝突？
因為每個 user 的檔案是分開管理的。

例如：
```
user 1 有 a
user 2 也有 a
user 3 也有 a
user 4 也有 a
```
這在 single-level directory 不行，因為整個系統只有一個目錄，`a` 會撞名。

但在 two-level directory 可以，因為完整來看它們其實是不同檔案：
```
/user1/a
/user2/a
/user3/a
/user4/a
```
所以規則變成：**同一個 UFD 裡檔名不能重複，但不同 user 的 UFD 之間可以有相同檔名**。
### 三、登入後怎麼找檔案？
當 user login 後，系統會先透過 **MFD** 找到該 user 對應的 **UFD**。之後該 user 讀寫檔案時，OS 通常只搜尋自己的 UFD。例如 user 3 登入後，要找 test：
```
MFD 找到 user 3
↓
進入 user 3 的 UFD
↓
搜尋 test
```
這樣搜尋範圍比較小，也比較不會混到其他 user 的檔案。
### 四、Path name 的概念
這段也引入 **path name（路徑名稱）**。

在 two-level directory 中，如果要唯一指定一個檔案，通常要同時指定：
```
user name + file name
```
例如：
```
/userb/test.txt
```
意思是：到 `userb` 的 UFD 裡找 `test.txt`。

所以 two-level directory 可以看成一棵高度為 2 的 tree：
```
root / MFD
↓
user directory / UFD
↓
file
```
每個檔案都有自己的 path name，這樣才能明確知道它屬於哪個 user。
### 五、缺點：user 之間不容易共享檔案
Two-level directory 解決了檔名衝突，但也帶來一個問題：不同 user 之間被隔離了。

這種 isolation 有好處，因為 user 不容易誤刪或誤改別人的檔案。
但如果 user 之間要合作，就不太方便。例如 user A 想讀 user B 的 test.txt，就不能只打：
```
test.txt
```
而要明確指定：
```
/userb/test.txt
```
而且系統還要檢查權限，確定 user A 真的能存取 user B 的檔案。
### 六、System files 與 Search Path
後面那段在講一個實際問題：系統程式、指令、library 要放哪裡？

如果每個 user 的 UFD 都各自放一份 system files，會很浪費空間。例如系統檔案 5 MB，有 12 個 users，就要 60 MB，只是重複存一樣的東西。

所以標準做法是：建立一個特殊的 system directory，裡面放共用的系統程式與檔案。當 user 輸入一個檔名或 command 時，OS 會先找 user 自己的 UFD；如果找不到，再去 system directory 找。

這串搜尋順序叫：

**Search path（搜尋路徑）**

例如：
```
先找 user 自己的 UFD
↓
找不到，再找 system directory
↓
再找其他設定好的 directory
```
UNIX / Linux 和 Windows 都大量使用 search path。像命令列輸入 gcc、python、ls 時，系統就是沿著 search path 找對應的執行檔。
## 13.3.3 Tree-Structured Directories：樹狀目錄結構
Tree-Structured Directory（樹狀目錄） 是把前面的 Two-Level Directory（雙層目錄） 再推廣成任意高度的樹。也就是說，不再只有「使用者目錄 → 檔案」兩層，而是可以在目錄裡再建立子目錄，形成多層資料夾結構。

它的概念像現在常見的檔案系統：
```
/
├── bin
├── home
│   └── user
│       ├── documents
│       └── pictures
└── programs
```
最上層叫 **root directory（根目錄)**。從 root 開始，每個 directory 可以包含 files，也可以包含 subdirectories。這種結構是目前最常見的 directory structure。

![image](https://hackmd.io/_uploads/Sk9Mb9TZfx.png)
### Directory 本身也可以看成一種特殊檔案
課本提到，在很多實作中，directory is simply another file, but treated specially。

意思是 directory 本質上也可以是一種檔案，只是它的內容不是一般資料，而是一堆 directory entries。每個 entry 可能代表一個一般 file，也可能代表一個 subdirectory。

課本也提到，每個 directory entry 可能會有一個 bit 用來標記：
```
0 → file
1 → subdirectory
```
所以 OS 看到某個 entry 時，可以知道它是一般檔案，還是下一層目錄。

### Current Directory：目前目錄
Current directory（目前目錄 / 工作目錄） 是每個 process 目前所在的 directory。

平常使用 shell 時，如果目前在：
```
/home/user/project
```
那麼只輸入：
```
main.c
```
OS 就會先在 current directory 裡找：
```
/home/user/project/main.c
```
這樣不用每次都打完整路徑。

每個 process 可以有自己的 current directory。當 process fork 出 subprocess 時，subprocess 通常會繼承 parent process 的 current directory。
### Absolute Path Name 與 Relative Path Name
這段很重要，樹狀目錄會自然引入兩種 path name。

Absolute path name（絕對路徑名稱） 是從 root 開始寫完整路徑。在 UNIX / Linux 中，root 用 / 表示。

例如：
```
/spell/mail/prt/first
```
這代表不管目前在哪個 directory，都從 root 開始找。

Relative path name（相對路徑名稱） 是從 current directory 開始找。

例如目前 current directory 是：
```
/spell/mail
```
那麼：
```
prt/first
```
就會指到：
```
/spell/mail/prt/first
```
所以 relative path 的意義會受到 current directory 影響。
### Tree-Structured Directory 的優點
最大優點是**方便分類與管理檔案**。使用者可以自己建立 subdirectories，把不同用途的檔案分開放。

例如：
```
bin        → executable binary files
programs   → source programs
mail       → mail-related files
documents  → reports or notes
```
這樣比所有檔案都擠在同一層好很多，也比 two-level directory 更彈性。

第二個優點是每個檔案都有唯一的 path name。即使不同 directory 裡有同名檔案，只要完整 path 不同，就可以區分。

例如：
```
/spell/bin/count
/programs/e/count
```
兩個檔名都叫 `count`，但因為路徑不同，所以是不同檔案。
### 刪除 Directory 的問題
樹狀目錄帶來一個麻煩問題：如果要刪除一個 directory，裡面還有東西怎麼辦？

有兩種常見策略。

第一種是：**只允許刪除 empty directory（空目錄）**。
如果 directory 裡還有 files 或 subdirectories，就不能刪。必須先把裡面的內容刪掉，才能刪目錄。這種方式比較安全。

第二種是：**recursive delete（遞迴刪除）**。
刪除一個 directory 時，連同裡面的 files 和 subdirectories 全部刪掉。例如 UNIX / Linux 的：
```
rm -r directory
```
這種方法方便，但也危險。因為一個指令打錯，可能整個目錄樹都被刪光，超級刺激但不值得 🫠
### Tree-Structured Directory 和共享檔案
樹狀目錄也可以讓使用者存取其他使用者的檔案，只要知道對方檔案的 path name，並且權限允許。

例如 user B 要存取 user A 的某個檔案，可以用 absolute path 或 relative path 指定：

/home/userA/report.txt

或者先切換 current directory 到 user A 的目錄，再用檔名存取。

不過這仍然是「用路徑找到別人的檔案」，真正更彈性的共享方式會在後面 Acyclic-Graph Directories（無環圖目錄） 或 link 的概念中出現。
## 13.3.4 Acyclic-Graph Directories：無環圖目錄
![image](https://hackmd.io/_uploads/rk1hfqTZGx.png)
這節是在解決 Tree-Structured Directory（樹狀目錄） 的限制：樹狀結構下，一個檔案或目錄只能出現在一個位置；但實際上常常需要**共享檔案或共享子目錄**。例如兩個 programmer 做同一個 project，希望同一個 project directory 同時出現在兩個人的工作目錄底下。這時如果只是複製一份，就會變成兩個不同檔案；其中一人修改後，另一份不會同步，這不是共享。

**Acyclic graph = 無環圖**，意思是目錄結構不再是單純 tree，而是 graph。某個 file 或 subdirectory 可以被多個 directory 指到，所以它可以「出現在多個地方」。但它不能形成 cycle，也就是不能繞一圈又指回自己，否則遍歷檔案系統時可能無限迴圈。
```
Tree：
一個節點只能有一個 parent。

Acyclic graph：
一個節點可以有多個 parents，
但不能形成 cycle。
```
圖 13.10 中，有些藍色圓形 file 被兩個不同 directory entry 指向，代表它是同一份 shared file。重點是：**不是兩份 copy，而是同一個實體檔案有多個 path name 可以到達**。 所以其中一個 path 修改檔案，另一個 path 看到的內容也會一起改變。

### Link：用來實作共享
常見做法是建立 link（連結）。
Link 可以理解成「某個 directory entry 指到另一個既有檔案或目錄」。

課本提到一種常見形式是 **symbolic link（符號連結）**，也常叫 **soft link（軟連結）**。Symbolic link 裡面通常存的是一個 pathname，可以是 absolute path name，也可以是 relative path name。

例如：
```
/project/report.txt
/home/userA/report-link -> /project/report.txt
/home/userB/report-link -> /project/report.txt
```
report-link 不是檔案本體，而是指向真正檔案的路徑。OS 存取 link 時，會 resolve the link（解析連結），也就是根據 link 裡的 pathname 找到真正檔案。

### Shared file 不等於 copied file
這頁很重要的一點是：shared file 跟 copied file 不一樣。

Copied file 是兩份資料：
```
A/report.txt
B/report.txt
```
其中 A 改了，B 不會變。

Shared file 是同一份資料有兩個名字或兩個位置可以找到：
```
A/report.txt  ┐
              ├── same actual file
B/report.txt  ┘
```
所以 A 改了，B 看到的也會變。這對 project directory 特別重要，因為新增的檔案也會自動出現在所有共享該 directory 的位置。
### Acyclic-Graph Directory 的問題
第一個問題是 **aliasing（別名問題）**。同一個 file 可能有多個 absolute path names，所以統計檔案數、計算磁碟空間、backup 時可能重複算同一份檔案。OS 遍歷整個 file system 時，必須避免同一個 shared structure 被拜訪兩次。

第二個問題是 **deletion（刪除）**。如果某個 shared file 被刪除，但其他 link 還指向它，就可能留下 **dangling pointer / dangling link（懸空指標 / 失效連結）**。Symbolic link 比較容易處理：刪除真正檔案後，link 可以留下來；等到日後使用該 link 時，OS 發現目標不存在，就回報錯誤。UNIX 和 Windows 通常採這種方式處理 symbolic link。

第三個問題是 reference 管理。另一種策略是：檔案只有在所有 references 都消失後才真正刪除。這需要記錄有多少 directory entries 或 links 指向它。最常見做法是 **reference count（參考計數）**。新增一個 link 就加 1，刪除一個 link 就減 1；當 count 變成 0，檔案才真的刪除。UNIX 對 **hard links（硬連結）** 就使用這種概念。
### Hard Link 和 Symbolic Link 的差別
**Hard link（硬連結）** 是多個 directory entries 直接指向同一個 inode。因為它們地位幾乎相同，所以沒有「原始檔」和「捷徑」的差別。UNIX 通常不允許 hard link 指向 directory，主要是避免目錄結構產生 cycle。

**Symbolic link（符號連結 / 軟連結）** 則是一個特殊檔案，內容是 pathname。它比較像捷徑。目標被刪掉後，symbolic link 可以留下，但會變成失效連結。
```
Hard link：
directory entry 直接指向同一個 inode。
靠 reference count 管理刪除。

Symbolic link：
link 檔案內存 pathname。
目標刪除後可能變成 dangling link。
```
### Hard link：多個名字指向同一個 inode
在 UNIX / Linux 檔案系統裡，真正代表檔案本體的是 **inode**。
**inode 會記錄檔案的 metadata，例如權限、大小、時間、資料 block 位置等**。

Hard link 的概念是：
```
name A ─┐
        ├── inode 100 ── actual file data
name B ─┘
```
也就是 name A 和 name B 都直接指向同一個 inode。它們地位完全平等，沒有誰是原檔、誰是捷徑。

例如：
```Bash
ln a.txt b.txt
```
這會建立 hard link。結果是：
```
a.txt ─┐
       ├── same inode
b.txt ─┘
```
所以修改 a.txt，b.txt 看到的內容也會改；修改 b.txt，a.txt 看到的內容也會改。因為根本是同一份檔案資料。

刪除其中一個名字時，檔案本體不一定消失：
```Bash
rm a.txt
```
只會讓 `a.txt` 這個 directory entry 消失，但 `b.txt` 還在，所以 inode 和資料還存在。只有當最後一個 hard link 都被刪掉，檔案資料才會真的被釋放。

| 比較              | Hard link              | Symbolic link    |
| --------------- | ---------------------- | ---------------- |
| 中文              | 硬連結                    | 符號連結 / 軟連結       |
| 本質              | 多個檔名指向同一個 inode        | 一個特殊檔案，內容是目標路徑   |
| 是否有原檔概念         | 沒有，大家地位相同              | 有，link 指向 target |
| 目標刪除後           | 只要還有其他 hard link，檔案仍存在 | link 變成失效連結      |
| 可否跨 file system | 通常不行                   | 可以               |
| 可否連到 directory  | 通常不允許，避免 cycle         | 可以，但需小心          |

## 13.3.5 General Graph Directory：一般圖形目錄
General Graph Directory（一般圖形目錄） 是比 Acyclic-Graph Directory（無環圖目錄） 更自由的目錄結構。前面的 tree directory 只能往下分支；acyclic graph directory 允許共享檔案或共享目錄，但不能形成 cycle；general graph directory 則允許更一般的圖形結構，因此**可能出現 cycle（環）**。
```
Tree directory：
目錄像一棵樹，沒有共享，也沒有 cycle。

Acyclic-graph directory：
可以共享檔案或目錄，但不能有 cycle。

General graph directory：
可以共享，也可能形成 cycle。
```
![image](https://hackmd.io/_uploads/Bk16-i6Wfl.png)
General graph 的最大問題就是 cycle。例如圖 13.11 裡面出現類似：
```
book → avi → book → avi → ...
```
這不是為了指到某個真正的 file，而是展示 **directory 指向 directory**，結果繞回原本的 directory。這種 cycle 會讓檔案系統在搜尋、備份、列出所有檔案、統計磁碟空間時可能無限迴圈。

例如 OS 要 traverse file system：
```
root
→ avi
→ book
→ avi
→ book
→ ...
```
如果沒有記錄哪些 directory 已經拜訪過，搜尋就可能永遠不結束。

第二個問題是** deletion（刪除）變困難**。在沒有 cycle 的 acyclic graph 中，可以用 reference count（參考計數） 判斷檔案或目錄是否還被使用。當 reference count 變成 0，就代表沒有任何地方指向它，可以刪除。

但在 general graph 中，如果兩個目錄互相指向：
```
A → B
B → A
```
即使從 root 已經到不了 A 和 B，它們仍然互相引用，所以 reference count 不會變成 0。這會造成「其實已經沒人能存取，但系統以為還有人參考」的垃圾資料。

因此 general graph 可能需要 **garbage collection（垃圾回收）**。
Garbage collection 的做法是從 **root directory（根目錄）** 開始走訪整個檔案系統，標記所有可以到達的 files 和 directories。走訪完成後，沒有被標記到的東西就代表無法從 root 到達，可以回收它們佔用的 disk space。
```
從 root 開始走訪
→ 標記所有可到達的檔案與目錄
→ 沒被標記到的就是垃圾
→ 回收磁碟空間
```
缺點是成本很高，因為可能要掃完整個 file system。對 disk-based file system 來說，這個動作非常耗時。

所以實務上很多系統會避免 general graph directory 的複雜性。常見做法是限制 links 的使用，例如不允許對 directory 建立 hard link，或在走訪目錄時忽略 symbolic link，避免 cycle 造成無限迴圈。

考試重點可以記成：**General Graph Directory 允許 cycle，因此比 acyclic graph 更彈性，但會造成 traversal 無限迴圈、reference count 失效、刪除困難，可能需要 expensive garbage collection**。
# 13.4 Protection 前言重點
## 前言
這段在講 file system 裡面「保護資料」其實有兩個層面：第一個是 **reliability（可靠性）**，第二個是 **protection（保護 / 存取控制）**。兩者都跟資料安全有關，但方向不太一樣。

**Reliability** 關心的是：資料不要因為硬體故障、系統錯誤、斷電、磁碟損壞、head crash、灰塵、溫度異常、檔案系統 bug、誤刪等原因消失。也就是防止「資料壞掉或不見」。常見做法是建立 duplicate copies，也就是備份。例如定期把 disk files 複製到 tape、其他 secondary storage、network storage 或 cloud。這部分比較像「資料保存」問題。

**Protection** 關心的是：資料不要被不該存取的人存取、修改、刪除。也就是防止 **improper access（不當存取）**。例如某個使用者不應該讀別人的私人檔案，不應該修改系統檔案，也不應該刪掉沒有權限刪的資料。這部分比較像「權限管理」問題。

可以把兩者分成這樣：
```
Reliability：
防止資料因故障或意外而遺失。
重點：backup、duplicate copies、recovery。

Protection：
防止未授權或錯誤的存取。
重點：authentication、authorization、encryption、access control。
```
課本舉的 protection 方法包含幾種。第一種是 username and password authentication（使用者名稱與密碼驗證），也就是確認「使用者是不是本人」。第二種是 encryption（加密），即使有人把 laptop 的硬碟拆下來，也不容易直接讀出內容。第三種是 firewalling network access（防火牆控管網路存取），避免系統在網路連線時被外部入侵。

不過這些方法在單人 laptop 上可能還算夠用，在 multiuser system（多使用者系統） 裡就不夠了。因為多使用者系統中，不同 user 都是合法登入者，但不代表每個人都能看所有資料。因此需要更細的 protection mechanism，例如檔案權限、owner、group、access-control list 等，來確保只有被允許的人才能做合法操作。

**Reliability protects files from physical damage or accidental loss, while protection prevents improper access by users or processes.**

## 13.4.1 Types of Access
這節的核心概念是：**file protection 的目的不是完全禁止存取，也不是完全開放存取**，而是提供 **controlled access（受控存取）**。也就是說，OS 要能根據使用者、檔案、權限設定，決定某個操作可以被允許或拒絕。

如果系統完全不允許使用者存取彼此的檔案，那確實不需要太多 protection；但這樣太不方便。反過來，如果所有人都能自由讀寫刪除所有檔案，也太危險。因此實際系統需要細分「可以做哪些操作」。
### 可控制的 access types
1. **Read（讀取）：** 允許從檔案讀資料。例如打開文字檔、讀取圖片、讀取程式碼。
2. **Write（寫入）：** 允許寫入或覆寫檔案內容。例如修改檔案原本內容。
3. **Execute（執行）：** 允許把檔案載入 memory 並執行。常見於 executable program、script。
4. **Append（附加）：** 允許把新資料加到檔案尾端，但不一定允許改掉原本內容。例如 log file 很常用 append權限。
5. **Delete（刪除）：** 允許刪除檔案，並釋放它占用的 storage space。
6. **List（列出）：** 允許列出檔案名稱與屬性。對 directory 來說，就是能不能看到目錄裡有哪些檔案。
7. **Attribute change（修改屬性）：** 允許修改檔案 attributes，例如權限、擁有者、時間戳記、檔案旗標等。

### 高階操作其實會拆成低階操作
課本後面提醒，像 **rename**、**copy**、**edit** 這些操作也可以被控制，但很多時候它們不是最底層的 primitive operation，而是由比較低階的 system calls 組合出來。

例如 **copy file（複製檔案）** 看起來是一個動作，但底層可能只是：
```
read 原檔案
create 新檔案
write 到新檔案
```
所以如果某個 user 有 read 權限，通常也代表該 user 可以把檔案內容讀出來後複製、列印或另存。這也是 protection 要小心的地方：**只要允許 read，資料內容基本上就可能被帶走**。
## 13.4.2 Access Control 重點整理
### Concept
這節在講 **Access Control（存取控制）**，也就是 OS 要怎麼判斷「某個 user 對某個 file / directory 可不可以做某種操作」。前面 13.4.1 先定義了 access types，例如 read、write、execute、delete；這節則是在講：**這些權限要怎麼記錄與檢查**。

最一般化的做法是使用 **ACL = Access-Control List（存取控制串列）**。每個 file 或 directory 都附帶一份 ACL，裡面列出哪些 user 可以做哪些操作。當 user 要存取某個檔案時，OS 會查看該檔案的 ACL；如果該 user 在清單中，而且要求的操作被允許，就放行，否則拒絕。

概念像這樣：
```
file: book.tex

ACL:
Sara → read, write, execute, delete
Jim  → read, write
Dawn → read, write
Jill → read, write
Other users → read only
```
這種方法很彈性，因為可以針對每個 user 設定不同權限。但缺點是 ACL 可能變得很長。如果要讓很多人都有 read 權限，就要把很多 user 都列進清單，管理起來很麻煩，而且 directory entry 或 metadata 也會變成可變長度，增加 storage management 的複雜度。
### Owner、Group、Other：壓縮版的 ACL
為了避免 ACL 太長，很多系統把使用者分成三類：

**Owner（擁有者）：** 建立該檔案的人，通常有最多權限。
**Group（群組）：** 一群需要相似權限的 users，例如同一個 project team。
**Other / Universe（其他人）：** 系統中不屬於 owner 或 group 的所有其他 users。

這就是 UNIX / Linux 常見的權限模型：
```
owner
group
other
```
每一類再搭配三種基本權限：
```
r = read
w = write
x = execute
```
所以一個 file 只需要 9 個 permission bits：
```
owner: rwx
group: rwx
other: rwx
```
例如：
```
-rw-r--r--
```
可以拆成：
```
-     → 一般檔案
rw-   → owner 可以 read/write，不能 execute
r--   → group 只能 read
r--   → other 只能 read
```
如果第一個字元是 d，表示 directory：
```
drwxr-xr-x
```
意思是這是一個 directory，owner 可以讀、寫、進入；group 和 other 可以讀與進入，但不能修改目錄內容。
### Directory 上的 r、w、x 意義不太一樣
UNIX 對 file 和 directory 都用 rwx，但 directory 的意思要特別記。

對一般 file：
```
r：讀檔案內容
w：改檔案內容
x：執行檔案
```
對 directory：
```
r：可以列出 directory 內容，例如 ls
w：可以新增、刪除、重新命名 directory 裡的 entry
x：可以進入 / 穿越 directory，例如 cd 或透過路徑存取裡面的檔案
```
所以 directory 的 x 很重要。沒有 x，即使知道檔名，也可能不能進入該目錄去存取檔案。
### ACL 和 owner/group/other 的混合使用
**ACLs take precedence（ACL 優先）**
Access Control 是用來決定 user 對 file / directory 可以做哪些操作的機制。最一般化的方法是 ACL，可以針對每個 user 設權限；但 ACL 太長不易管理，所以 UNIX 常用 owner、group、other 三分類，加上 rwx permission bits。現代系統常把兩者結合，平常用簡單 permission bits，需要細部控制時再使用 ACL。
## 13.4.3 Other Protection Approaches 重點

這節是在補充除了 **ACL = Access-Control List（存取控制串列）**、UNIX `rwx` 權限以外，還有哪些保護檔案的方法。

第一種方法是：**幫每個 file 設 password（密碼）**。概念很直覺，存取某個檔案前必須輸入該檔案的密碼。這種方法如果密碼夠隨機、常更換，在小範圍使用時可以有效。但缺點很明顯：如果每個檔案都有不同密碼，使用者要記太多密碼；如果所有檔案共用同一個密碼，只要密碼外洩，所有檔案都失去保護。因此這種方法不太適合大型系統。

第二個重點是：有些系統不是對每個檔案設密碼，而是對 **subdirectory（子目錄）** 設密碼。這樣可以一次保護一整批檔案，比每個檔案各設密碼更實用。不過現代系統更常用的是 **encryption（加密）**，例如加密某個 partition、folder 或 individual file。這比單純密碼保護更強，但密碼或金鑰管理仍然是關鍵。

---

接著課本提到：在 **multilevel directory structure（多層目錄結構）** 裡，不只 file 本身要保護，**directory 也要保護**。因為 directory 裡面存著檔案名稱與目錄結構，如果可以隨便列出目錄內容，就可能洩漏檔案是否存在、檔名是什麼，甚至暴露系統結構。

所以 directory protection 要控制的操作會跟 file 不太一樣，例如：

```text id="58r06j"
能不能在目錄中建立檔案
能不能刪除目錄中的檔案
能不能列出目錄內容
能不能搜尋某個檔案是否存在
能不能透過路徑進入該目錄
```

也就是說，**知道某個檔案存在** 本身有時就是敏感資訊。因此 `list directory` 也需要權限控制。

---

最後一個重點是：在有 link、acyclic graph、general graph directory 的系統裡，同一個 file 可能有多個 **path name（路徑名稱）**。這會造成一個問題：同一個檔案透過不同路徑進入時，可能有不同 access rights。

例如同一份檔案可能可以從：

```text id="mcqyur"
/project/shared/report.txt
/home/user/link/report.txt
```

兩條路徑到達。系統就必須決定：權限到底看檔案本身、看路徑上的 directory，還是兩者都看。這會讓 protection 變得更複雜。

---

### 核心總結

**13.4.3 的重點是：除了 ACL 和 rwx 權限外，還可以用 password、directory-level protection、encryption 等方式保護檔案。不過 password per file 管理困難，directory 也需要保護，因為列出檔名或知道檔案存在本身可能就是敏感資訊；在有多重路徑或 link 的系統中，同一檔案也可能因不同 path name 而牽涉不同權限。**
