---
title: ch12  I/O Systems

---

# 12.1 I/O Systems Overview
這頁是 **I/O Systems Overview**，也就是 I/O 系統的總覽。重點是：**I/O management 是作業系統設計與執行中很重要的一部分**，因為電腦不只 CPU 和 memory，還要控制很多外部裝置，例如鍵盤、滑鼠、硬碟、SSD、USB、網路卡、螢幕、印表機等等。

**I/O = Input / Output**，中文是 **輸入 / 輸出**。
Input 是資料進入電腦，例如鍵盤輸入、滑鼠移動、磁碟讀取、網路接收封包。Output 是資料從電腦送出去，例如螢幕顯示、磁碟寫入、網路傳送封包。

這頁說 **I/O devices vary greatly**，意思是 I/O 裝置差異非常大。滑鼠很慢、硬碟和 SSD 是大量資料傳輸、網路卡會連續接收封包、GPU 又是高效能資料處理裝置。因為每種裝置的速度、資料格式、控制方式都不同，所以 OS 不能用同一種低階方法直接控制全部裝置，而需要各種控制方法。

**Performance management** 是效能管理。I/O 通常比 CPU 慢很多，所以如果 I/O 管理不好，CPU 可能一直在等裝置，整體系統效能會很差。因此 OS 要處理 buffering、caching、scheduling、interrupt、DMA 等機制，讓 I/O 盡量有效率。

投影片中間提到 **ports, buses, device controllers**：

**Port** 是連接埠或 I/O 埠，是 CPU / OS 和裝置控制器溝通的介面。
**Bus** 是匯流排，例如 PCIe、USB、SATA，負責連接 CPU、memory 和 I/O devices。
**Device controller** 是裝置控制器，是控制實際硬體裝置的電路。例如 disk controller 控制磁碟讀寫，network controller 控制網路封包收發。

最後最重要的是 **Device driver**。

**Device driver = 裝置驅動程式**。
它通常是 kernel 裡的軟體模組，負責封裝某個硬體裝置的細節。也就是說，driver 知道這個裝置要怎麼初始化、怎麼下命令、怎麼讀寫 register、怎麼處理 interrupt、怎麼回報錯誤。

投影片寫：

**Device drivers encapsulate device details**

意思是 driver 把硬體細節包起來，不讓 OS 其他部分直接面對每個裝置亂七八糟的差異。

接著：

**Present uniform device-access interface to I/O subsystem**

意思是 driver 對 OS 的 **I/O subsystem** 提供統一的裝置存取介面。
**I/O subsystem** 是 kernel 裡負責管理 I/O 的子系統。它不用知道每個硬體的底層細節，只要透過 driver 提供的標準介面操作裝置即可。

可以想成這樣：

```text
Application
↓
System call，例如 read(), write()
↓
Kernel I/O subsystem
↓
Device driver
↓
Device controller
↓
Actual I/O device
```

本頁核心整理：

**I/O management 是 OS 的重要組成，因為 I/O devices 種類多、速度差異大、控制方式不同。硬體上透過 ports、buses、device controllers 連接裝置；軟體上透過 device drivers 封裝硬體細節，並提供統一介面給 I/O subsystem。**

# 12.2 I/O Hardware
## 序言
### 前言統整
這一段在講 I/O hardware（輸入 / 輸出硬體） 的基本架構。作業系統要管理非常多不同種類的 I/O devices，所以需要先理解裝置是怎麼和電腦連接、溝通、傳資料的。

I/O = Input / Output，輸入 / 輸出。
I/O devices 可以分成幾大類：

**Storage devices（儲存裝置）：** 例如 disk、SSD、magnetic tape。
**Transmission devices（傳輸裝置）：** 例如 network controller、Bluetooth controller。
**Human-interface devices（人機介面裝置）：** 例如 screen、keyboard、mouse、audio input/output。

雖然裝置種類很多，但從 OS 角度來看，重點是掌握幾個共同概念：**port**、**bus**、**controller**、**device driver**。
### 1. Port：連接點
**Port（埠 / 連接埠）** 是裝置和電腦系統溝通的連接點。
例如 serial port、USB port、I/O port 都可以看成某種 port。

一個 device 可以透過 port 和電腦交換訊號。這裡的訊號可能是透過實體線材，也可能是無線，例如 Bluetooth。
**重點是：**
    * **Port 是單一裝置或控制器接入系統的入口**。
### 2. Bus：匯流排
**Bus（匯流排）** 是一組共同的線路，讓多個裝置共享同一套傳輸通道。

課本提到，如果多個 devices 共用一組 wires，這種 connection 就叫做 bus。Bus 上通常會有明確的 protocol，規定資料怎麼送、什麼時候送、訊號格式怎麼定義。

**例如：**
    * **PCI(Peripheral Component Interconnect) bus：** 常見於 PC 和 server，用來連接較高速的 devices。
    * **PCIe(Peripheral Component Interconnect Express)** = 是 PCI 的現代高速版本。
    * **Expansion bus（擴充匯流排）：** 通常用來接比較慢的 devices，例如 keyboard、serial port、USB port 等。
### 3. Daisy chain：菊鏈式連接
**Daisy chain（菊鏈）** 是一種裝置串接方式。
例如： `Device A → Device B → Device C → Computer port`
A 接到 B，B 接到 C，C 再接到電腦。這種串接結構稱為 daisy chain。雖然形式上是一個接一個串起來，但整體常常仍可視為一種 bus。
### 4. Controller / Host Adapter：控制器 / 主機介面卡
**Controller（控制器）** 是操作 port、bus 或 device 的電子電路。
它負責接收 CPU 或 OS 的命令，然後實際控制裝置。

投影片也寫：

**Controller = host adapter**

**Host adapter（主機介面卡）** 是一種控制器，通常負責把主機連接到某種 bus 或 device。例如 SCSI controller、SAS controller、disk controller。

Controller 可能是：

整合在主機板上。
獨立的一張擴充卡。
內含 processor、microcode、private memory、bus controller 等。

**Microcode（微程式碼）** 是控制器內部執行的低階控制邏輯。
**Private memory（私有記憶體）** 是 controller 自己使用的小型記憶體。
**Bus controller（匯流排控制器）** 則負責管理 bus 上的資料傳輸。

重點是：
* **CPU 不會直接控制每個硬體細節，而是透過 controller 操作裝置。**
### 5. PCI / PCIe 重點
**PCI = Peripheral Component Interconnect**，中文常稱 周邊元件互連匯流排。
**PCIe = Peripheral Component Interconnect Express**，中文是 PCI Express。

課本提到 PCIe 是現代 PC 常見的 system bus。PCIe 使用 lanes（通道） 傳輸資料。每個 lane 由兩組 signaling pairs 組成，一組負責接收，一組負責傳送，所以 PCIe 可以高速雙向傳輸。

簡單記法：
* **PCIe 是高速匯流排，常用來連接顯示卡、SSD、網路卡、SAS controller 等高速裝置**。
### 6. SAS / SCSI / IDE 名詞
投影片圖中出現幾個儲存相關介面。

**SCSI = Small Computer System Interface**
中文：小型電腦系統介面。
它是一種主機和儲存裝置之間的介面標準，可用來連接多顆 disks 或其他 I/O devices。

**SAS = Serial Attached SCSI**
中文：序列式連接 SCSI。
它是 SCSI 的序列化版本，常用於伺服器硬碟與儲存系統。

**IDE = Integrated Drive Electronics**
中文：整合式磁碟電子介面。
是較早期常見的硬碟介面，後來多被 SATA 等取代。
### 7. Typical PC Bus Structure
![image](https://hackmd.io/_uploads/SJNjO9tWze.png)
這張圖是在畫一台典型 PC 裡，各種元件如何透過 bus 和 controller 連接。
中間最重要的是一條 PCIe bus。它像主幹道路，連接高速元件與控制器。

左邊：
```
monitor
↓
graphics controller
↓
PCIe bus
```
螢幕不是直接接 CPU，而是透過 graphics controller（圖形控制器） 接到 PCIe bus。Graphics controller 負責處理顯示輸出。

中間：
```
processor
↓
bridge / memory controller
↔ cache
↔ memory
↓
PCIe bus
```
Processor（處理器） 透過 bridge / memory controller（橋接器 / 記憶體控制器） 連接 memory、cache 和 PCIe bus。
這裡的 bridge 負責在 CPU、memory 和 I/O bus 之間轉接資料。

右下：
```
expansion bus interface
↓
expansion bus
↓
USB port / keyboard / serial port
```
比較慢的裝置不一定直接掛在 PCIe bus 上，而是透過 expansion bus interface 接到 expansion bus。例如 keyboard、USB port、serial port 這些相對低速裝置。

左下 / 右上儲存裝置：

課本圖中有 SAS controller，連接多顆 disks。
投影片圖中也出現 SCSI controller 和 IDE disk controller，表示不同儲存介面可以透過 controller 接到主系統 bus。

重點是：
* **I/O devices 通常不是直接接 CPU，而是透過 controller 接到 bus，再由 OS 經由 driver 和 controller 溝通**。
### 本節考試重點整理
Port：裝置接入電腦系統的連接點。
Bus：多個裝置共享的傳輸通道，例如 PCI、PCIe、expansion bus。
Daisy chain：多個裝置串接成一條鏈。
Controller / Host adapter：控制 port、bus、device 的電子元件，負責實際操作硬體。
PCIe = Peripheral Component Interconnect Express：現代高速 PC bus。
SCSI = Small Computer System Interface：儲存與周邊裝置介面標準。
SAS = Serial Attached SCSI：序列式 SCSI，常用於伺服器儲存。
IDE = Integrated Drive Electronics：早期硬碟介面。

一句話總結：
* **I/O hardware 的核心架構是：devices 透過 ports 或 buses 連到 computer system，controller / host adapter 負責實際操作硬體，而 OS 透過 device driver 與 controller 溝通，藉此管理種類繁多的 I/O devices**。
## 12.2.1 Memory-Mapped I/O
### 重點
這段在講：processor 要怎麼把命令與資料交給 I/O controller，讓裝置完成 I/O transfer。

I/O = Input / Output，輸入 / 輸出。
Controller = 控制器，是 CPU 和實際裝置之間的硬體中介。CPU 通常不會直接控制硬碟、鍵盤、螢幕這些裝置，而是透過 controller 裡面的暫存器來下命令、送資料、讀狀態。
### 1. Controller 裡面有 registers
課本說 controller 通常有一個或多個 **registers（暫存器）**，用來存放資料與控制訊號。
CPU 和 controller 溝通的方式，就是去讀寫這些 registers 裡的 bit pattern。
例如 CPU 想叫硬碟讀資料，不是直接去操作硬碟馬達，而是寫入某些控制暫存器：
```
我要讀取
磁碟位置是 X
資料要放到 memory address Y
讀取長度是 Z
開始執行
```
Controller 收到這些資訊後，才去操作實際硬體。
### 2. 第一種方式：Special I/O Instructions
第一種方式是使用**特殊的 I/O instructions**。

這種方法會把 I/O device 的 registers 放在一個**獨立的 I/O address space** 裡。CPU 要存取它們時，不是用一般 memory load/store，而是用特殊指令去讀寫 I/O port。

以 x86 來說，常見概念是：
```
IN  instruction：從 I/O port 讀資料
OUT instruction：寫資料到 I/O port
```
這種方式叫 **port-mapped I/O** 或 **isolated I/O**。
### Figure 12.2：PC 上的 I/O port address
![image](https://hackmd.io/_uploads/BydOc9tWMl.png)
圖 12.2 是在列出早期 PC 裡常見裝置的 I/O port address range。
這代表每個裝置控制器都有一段固定的 I/O port 位址。CPU 如果對 `3F8–3FF` 這段 I/O port 做讀寫，就是在和 primary serial port 溝通。

幾個全稱順便記：
* **DMA = Direct Memory Access**，直接記憶體存取。
* DMA controller 可以讓裝置直接和 main memory 傳資料，不必每個 byte 都靠 CPU 搬。
### 4. 第二種方式：Memory-Mapped I/O
另一種更常見的方式是 **memory-mapped I/O**。

**Memory-mapped I/O** 的意思是：==把 device-control registers 映射到 CPU 的 memory address space 裡==。

也就是說，從 CPU 角度看，某些 physical memory address 其實不是 RAM，而是對應到某個裝置控制器的 registers。

所以 CPU 可以用一般的 memory load/store 指令去控制裝置：
```
讀某個 memory address
→ 實際上是在讀 device register

寫某個 memory address
→ 實際上是在寫 device register
```
這樣的好處是，CPU 不需要另外一套特殊 I/O 指令，而是用標準的資料傳輸指令完成 I/O request。
Especially for large address spaces (graphics)
### 5. 為什麼現在常用 Memory-Mapped I/O？
課本用 graphics controller 當例子。

顯示卡通常有一些 I/O ports 用來做基本控制，但真正螢幕內容通常會放在一塊 memory-mapped region 裡。程式或 OS 把資料寫到這塊 memory-mapped display memory，graphics controller 就根據這些內容產生螢幕畫面。

如果用 I/O instruction 一個 byte 一個 byte 寫，會非常慢。
但如果把顯示記憶體映射成 memory region，CPU 就可以大量寫入 memory，效率比較高。

所以現代系統大多偏向使用 **memory-mapped I/O**。
### 6. I/O device control 的四種 registers
#### 慣例
課本說 I/O device control 通常包含四種 registers：
```
status register
control register
data-in register
data-out register
```
#### Data-in register
Data-in register 是給 host 讀取輸入資料用的。

例如鍵盤 controller 收到按鍵資料後，CPU 可以從 data-in register 讀出按鍵碼。

簡單記：
```
device → host
```
**所以 data-in 是裝置傳資料進來給主機**。
#### Data-out register
**Data-out register** 是給 host 寫輸出資料用的。

例如 CPU 要送一個 byte 到 serial port，就把資料寫進 data-out register，controller 再把資料送出去。

簡單記：
```
host → device
```
所以 data-out 是主機送資料出去給裝置。
#### Status register
**Status register** 是狀態暫存器。CPU 會讀它，判斷裝置目前狀態。

它可能包含：
```
目前命令是否完成
data-in register 裡是否有資料可讀
device 是否忙碌
是否發生錯誤
```
例如 status register 某個 bit = 1，可能代表「資料已經準備好，可以讀了」。
#### Control register
**Control register** 是控制暫存器。CPU 會寫它，用來啟動命令或改變裝置模式。

例如 serial port 的 control register 可以設定：
```
full-duplex 或 half-duplex
是否啟用 parity checking
word length 是 7 bits 還是 8 bits
傳輸速度 baud rate
```
簡單說，status register 是「看狀態」，control register 是「下命令 / 設定模式」。
### 7. FIFO buffer
最後一段提到，有些 controller 內部有 FIFO chip / FIFO buffer。
FIFO = First-In, First-Out，先進先出。
這是一個小型 buffer，可以暫存好幾個 bytes 的 input 或 output data。
如果 controller 只有 1～4 bytes 的 data register，很容易太小。FIFO 可以讓 controller 先累積一小段資料，等 device 或 host 有空時再慢慢處理。
例如網路卡或 serial port 收到一小串資料時，可以先放進 FIFO，不用每收到一個 byte 就立刻中斷 CPU。
這樣可以降低 CPU 負擔，也讓資料傳輸比較順。
### 本段核心整理
**Processor 控制 I/O device 的方式，是讀寫 controller 裡的 registers。這些 registers 可以透過特殊 I/O instructions 存取，也可以映射到 memory address space，形成 memory-mapped I/O。現代系統多使用 memory-mapped I/O，因為可以用一般 memory load/store 指令大量傳輸資料。I/O controller 常見 registers 包含 data-in、data-out、status、control，而 FIFO buffer 則用來暫存小批資料，減少 CPU 和裝置速度不一致造成的問題。**
## 12.2.2 Polling
### Concept
中文常翻成 輪詢 或 查詢式 I/O。它的核心概念是：host 反覆讀取 device controller 的狀態暫存器，檢查裝置是否準備好。

這裡的 host 指 CPU / 主機端；controller 指 I/O 裝置控制器。CPU 不直接控制裝置本體，而是透過 controller 的 registers 來下命令、傳資料、看狀態。
### 重要名詞
**Status register（狀態暫存器）**
用來讓 host 讀取裝置目前狀態。例如裝置是否忙碌、是否完成、是否有錯誤。

**Busy bit（忙碌位元）**
存在 status register 裡。
`busy bit = 1` 表示 controller 正在忙，還不能接收下一個命令。
`busy bit = 0` 表示 controller 空閒，可以接收命令。

**Command register（命令暫存器）**
host 寫入命令的地方，例如 read、write、start、command-ready 等控制資訊。

**Command-ready bit（命令就緒位元）**
host 用它告訴 controller：「命令已經準備好了，可以開始執行。」

**Data-out register（資料輸出暫存器）**
host 要送資料給 device 時，會把資料寫到 data-out register。

**Error bit（錯誤位元）**
通常在 status register 裡，用來表示 I/O 過程是否發生錯誤。

**Set / Clear**
`set a bit` 是把 bit 設成 `1`。
`clear a bit` 是把 bit 清成 `0`。
### Polling 的 handshaking 流程
這段用一個「host 要輸出一個 byte 到 port」的例子說明 **handshaking（握手協定）**。所謂 handshaking，就是 host 和 controller 透過一些 bit 互相確認狀態，避免資料還沒準備好就亂讀寫。

流程如下：

第一步，host 一直讀 **busy bit**。如果 busy bit 還是 `1`，代表 controller 還在忙，host 就繼續等。直到 busy bit 變成 `0`，才表示 controller 可以接收新命令。

第二步，host 在 **command register** 裡設定 **write bit**，表示這次是寫入操作，並且把要輸出的 byte 寫進 data-out register。

第三步，host 設定 **command-ready bit**，通知 controller：「命令和資料都準備好了。」

第四步，controller 看到 command-ready bit 被設成 `1` 後，會把 busy bit 設成 `1`，表示它開始忙著處理這個 I/O request。

第五步，controller 讀取 command register，知道這次是 write command，接著從 data-out register 拿到那個 byte，然後把資料送到真正的 I/O device。

第六步，controller 完成 I/O 後，會清掉 command-ready bit，清掉 error bit 表示沒有錯誤，最後清掉 busy bit，表示工作完成，可以接收下一個命令。

這個流程每傳一個 byte 就可能重複一次，所以如果資料量很大，純 polling 會變得很沒效率。

---
**controller 裡面有一些 registers，而那些 busy bit、command-ready bit、write bit、error bit 通常就是這些 registers 裡面的某幾個 bit 欄位**。

可以把 controller 想成一個小硬體模組，裡面有幾個給 CPU / host 讀寫的暫存器：
```
I/O Controller
├── status register
│   ├── busy bit
│   ├── error bit
│   ├── ready bit
│   └── ...
│
├── control / command register
│   ├── write bit
│   ├── read bit
│   ├── command-ready bit
│   └── ...
│
├── data-in register
│   └── device 傳給 host 的資料
│
└── data-out register
    └── host 要送給 device 的資料
```
**status register** 主要是給 host 讀的，用來看 controller 現在狀態。例如 `busy bit = 1` 表示 controller 正在忙，`busy bit = 0` 表示可以接收新命令；`error bit = `1 表示剛剛 I/O 發生錯誤。

**control / command register** 主要是給 host 寫的，用來下命令。例如設定 `write bit = 1` 表示這次要做輸出；設定 `command-ready bit = 1` 表示命令已經準備好，controller 可以開始執行。

**data-out register** 是 host 要送資料給 device 時寫入的地方。課本例子中，host 要輸出一個 byte，就會把那個 byte 寫進 data-out register。

**data-in register** 則是 device 傳資料回 host 時，host 讀取資料的地方。

所以 polling 那個流程其實就是 host 和 controller 透過這幾個 registers 裡的 bit 做溝通：
```
host 看 status register 的 busy bit
↓
host 寫 command register 的 write bit
↓
host 寫 data-out register
↓
host 設 command-ready bit
↓
controller 看到後設 busy bit
↓
controller 執行 I/O
↓
controller 清 command-ready、error、busy bit
```
一句話整理：
**register 是一整個暫存器；bit 是 register 裡面的某一位。Polling handshaking 就是 host 和 controller 透過讀寫這些 register 裡的特定位元來同步 I/O 操作**。
### Polling 的優點與缺點
Polling 的優點是簡單，控制流程很直接。若裝置很快，或等待時間非常短，polling 其實可以很有效率。課本也提到，在很多架構中，poll 一次裝置可能只需要少量 CPU instruction cycles，例如讀 register、取出 status bit、判斷是否完成。

但 polling 的缺點是：如果裝置很慢，CPU 會浪費大量時間反覆檢查。若 OS 讓 CPU 去做別的 task，又會遇到另一個問題：host 什麼時候要回來檢查裝置？如果回來太晚，某些資料可能已經遺失。

例如 serial port 或 keyboard 這類裝置可能有小 buffer。資料持續進來時，如果 host 太久沒有讀取，controller buffer 可能 overflow，導致資料遺失。
### 本節核心重點
**Polling 是 host 反覆讀取 controller 的 status register，檢查 busy bit 或其他狀態位元，直到裝置準備好為止。它透過 busy bit、command-ready bit、data-out register、command register 等完成 host 和 controller 之間的 handshaking。Polling 簡單但可能造成 busy-waiting，浪費 CPU time；因此若裝置等待時間較長，通常改用 interrupt 讓 device 主動通知 CPU**。
## 12.2.3 Interrupts
### 1. Interrupt 的基本想法
Polling 是 CPU 一直主動查詢 controller 狀態：
```
CPU：好了沒？
CPU：好了沒？
CPU：好了沒？
```
Interrupt 則反過來，讓 I/O controller 在需要服務時主動通知 CPU：

I/O controller：資料好了，CPU 來處理！

所以 interrupt 的好處是：CPU 不需要一直 busy-waiting，可以先去執行其他工作。等裝置完成 I/O、輸入資料準備好，或發生錯誤時，controller 再透過 interrupt 通知 CPU。
### 2. Interrupt-request line 是什麼？
課本說 CPU hardware 有一條線叫：

**interrupt-request line**，中斷請求線。

這條線是 I/O controller 用來通知 CPU 的硬體訊號線。當某個裝置需要 CPU 注意時，controller 會在這條線上送出訊號。這個動作叫：

**raise an interrupt**
也就是「發出中斷」。

CPU 會在每次 instruction 執行完之後檢查 interrupt-request line。如果發現有中斷訊號，就會暫停目前程式，轉去執行 interrupt handler。

### 3. Interrupt handler 是什麼？
**Interrupt handler** 是中斷處理程式。
也可以叫 **interrupt-service routine**，**ISR**，中斷服務常式。

它是一段 kernel code，專門處理某種 interrupt。例如鍵盤中斷、磁碟中斷、網路卡中斷、timer interrupt 等。

當 interrupt 發生時，CPU 會做幾件事：
```
1. 儲存目前執行狀態
2. 跳到對應的 interrupt handler
3. handler 處理中斷事件
4. handler 回復狀態
5. CPU 回到原本被中斷的程式繼續執行
```
這裡的 **state save / state restore** 很重要。
因為 CPU 原本可能正在執行某個 process，如果中斷處理完後要回去繼續跑，必須保存原本的 program counter、registers、狀態資訊等等。

**CPU‘fetch-execute cycle”**
![image](https://hackmd.io/_uploads/SkvvHpFZGe.png)
### Figure 12.3：Interrupt-driven I/O cycle
![image](https://hackmd.io/_uploads/SkVlLTYWMx.png)
這張圖是在說 **interrupt-driven I/O** 的完整流程。

流程可以照圖中的 1 到 7 看：
```
1. Device driver initiates I/O
```
第一步，CPU 端的 device driver 發起 I/O。
例如程式想讀 disk，OS 透過 disk driver 對 disk controller 下命令。
```
2. I/O controller initiates I/O
```
第二步，I/O controller 收到命令後，開始真正操作裝置。
例如 disk controller 開始讀磁碟資料。
```
3. Device is ready / output complete / error occurs
```
第三步，I/O controller 完成工作，或發現輸入資料準備好，或發生錯誤，於是產生 interrupt signal。
```
4. CPU receives interrupt and transfers control to interrupt handler
```
第四步，CPU 偵測到 interrupt-request line 有訊號，於是停止目前正常執行流程，跳到 interrupt handler。
```
5. Interrupt handler processes data and returns from interrupt
```
第五步，interrupt handler 處理事件。
例如讀取 controller 狀態、確認 I/O 成功、把資料交給 OS buffer、喚醒等待 I/O 的 process。
```
6. CPU resumes processing of interrupted task
```
第六步，中斷處理完後，CPU 回復原本被中斷的 task。
```
7. 回到原本流程，之後可能再發起下一次 I/O
```
這張圖的核心是：**I/O controller 完成工作後，用 interrupt 通知 CPU，而不是讓 CPU 一直 polling**。
### 5. 為什麼需要更複雜的 interrupt 機制？
課本列出 4 個需求。

第一，**需要能延後某些 interrupt handling**。
在 critical processing 期間，有些程式碼不能被打斷，例如正在修改 kernel 重要資料結構。如果中途被 interrupt，可能造成資料不一致。所以 OS 需要能暫時延後某些中斷。

第二，**需要快速找到正確的 interrupt handler**。
如果每次 interrupt 來了，都要一個一個問所有裝置「是不是發出中斷」，效率會很差。因此需要一種快速 dispatch 到正確 handler 的方式。

第三，**需要 multilevel interrupts**。
也就是不同 interrupt 有不同 priority。高優先權 interrupt 可以比較快被處理，低優先權 interrupt 可以稍後處理。

第四，**需要讓 software 也能觸發類似 interrupt 的機制**。
例如 page fault、division by zero、system call 不是一般 I/O device 觸發，但也需要讓 CPU 進入 kernel 處理。這類通常稱為 **exception** 或 **trap**。
### 6. Maskable interrupt 與 Nonmaskable interrupt
課本說 CPU 通常有兩種 interrupt request lines。

Maskable interrupt：可遮蔽中斷。
意思是 CPU 可以暫時關閉或延後這類 interrupt。一般 I/O device 大多使用 maskable interrupt，例如鍵盤、網路卡、磁碟 controller。

Nonmaskable interrupt，NMI：不可遮蔽中斷。
這種中斷不能被 CPU 忽略，通常用於非常嚴重的事件，例如 unrecoverable memory error、硬體故障等。

簡單記：
```
Maskable interrupt：
一般裝置中斷，可以暫時關掉或延後。

Nonmaskable interrupt：
嚴重硬體事件，不能忽略。
```
### 7. Interrupt vector 是什麼？
Interrupt vector 是中斷向量。
它是一張表，用來把 interrupt number 對應到正確的 interrupt handler address。

概念像這樣：
```
Interrupt number 32 → keyboard interrupt handler
Interrupt number 33 → disk interrupt handler
Interrupt number 14 → page fault handler
```
這樣 CPU 收到某個 interrupt number 時，不用慢慢找是哪個裝置發生中斷，而是直接查 interrupt vector，跳到對應 handler。

所以 interrupt vector 的目的就是：快速 dispatch 到正確的 interrupt handler。
### 8. Interrupt chaining 是什麼？
有時候裝置很多，但 interrupt vector 的 entries 不夠，或多個 devices 共用同一個 interrupt number。這時可以用：

Interrupt chaining，中斷鏈結。

意思是 interrupt vector 的某個 entry 不只指向一個 handler，而是指向一串 handler list。

流程像這樣：
```
interrupt number 40 發生
↓
呼叫 handler A，看是不是它的裝置
↓
不是就呼叫 handler B
↓
不是就呼叫 handler C
↓
直到找到真正需要服務的裝置
```
這是一種折衷：
不用做巨大 interrupt table，但也不用只有一個 handler 搜尋所有裝置。 
### 10. Interrupt priority levels
Interrupt priority levels 是中斷優先權層級。

不是所有 interrupt 都一樣急。例如：
```
高優先權：
嚴重硬體錯誤、timer interrupt、重要裝置事件

低優先權：
某些普通 I/O completion、背景處理工作
```
高優先權 interrupt 可以先處理，低優先權 interrupt 可以被延後。甚至高優先權 interrupt 可以打斷低優先權 interrupt handler。

這讓 OS 可以先處理最緊急的事情，避免重要事件被低優先權工作卡住。

### 11. Exception、Trap、System Call
#### 觀念
**interrupt mechanism 不只處理 I/O device interrupt，也用來處理 CPU 執行指令時發生的 exception / fault / trap**。核心概念是：當 CPU 執行某條 instruction 時，如果發生特殊事件，CPU 會暫停原本流程，跳到 kernel 對應的 handler 處理。
#### 1. Fault：可修復型例外，通常發生在 instruction 完成前
**Fault** 是一種 **exception（例外）**。它通常代表：目前這條 instruction 如果繼續執行下去會出問題，所以 CPU 在 instruction 真正完成前就偵測到異常，然後交給 OS 處理。

投影片寫：

**Instruction would be illegal to execute**

意思是這條指令目前不能合法完成。例如：
```
寫入 read-only memory segment
讀取目前不在 memory 的 page
執行 privileged instruction
```
第一個例子是程式想寫入唯讀區域，例如 code segment 或 read-only data。這通常是非法操作。

第二個例子是 page fault。程式要讀某個 virtual address，但該 page 目前不在 physical memory，而是在 disk / backing store 裡。這時 CPU 不能直接完成 memory access，所以產生 fault，交給 OS 把 page 載入 memory。

第三個例子是 user mode 程式想執行 privileged instruction，例如直接操作硬體或修改特殊 CPU 狀態。這通常不允許，所以會產生 exception。

---
**Fault 的重點是：**

Detected before incrementing the IP

IP = Instruction Pointer，也就是指向目前正在執行 instruction 的暫存器。
在 fault 發生時，CPU 記錄的返回位置通常仍然指向「造成 fault 的那條 instruction」。

這很重要，因為如果 fault 的原因可以被修好，CPU 可以重新執行同一條 instruction。

例如 page fault：
```
程式執行 load [address]
↓
該 page 不在 memory
↓
CPU 產生 page fault
↓
OS 把 page 從 disk 載入 RAM
↓
回到原本那條 load instruction
↓
重新執行，這次成功
```
所以 fault 常見特色是：

**可以修復，修復後可重新執行原本那條 instruction**。
#### 2. Trap：有意或預期的事件，通常發生在 instruction 完成後
**Trap** 也是一種 exception，但它通常不是「指令非法不能執行」，而是 **指令已經執行完，接著故意或預期地轉交控制權給 handler**。

投影片舉 debugger 的例子：

CPU 可以被設定成每執行完一條 instruction，就自動切到 debugger。這種情況就是 trap。

投影片寫：

**It is activated after incrementing the IP**

意思是 trap 發生時，IP 通常已經前進到下一條 instruction。因為造成 trap 的那條 instruction 已經執行完了，之後如果 handler return，通常會接著執行下一條 instruction，而不是重跑原本那條。

典型例子有：
```
debug trap
breakpoint
system call trap
```
例如 debugger 設 breakpoint，程式執行到某個位置後，CPU 觸發 trap，進入 debugger handler。這不是硬體壞掉，也不是 memory 不在，而是設計上故意讓 CPU 停下來交給 debugger。

System call 也常透過 trap 或 software interrupt 概念實作。User program 執行特殊 instruction，讓 CPU 從 user mode 進入 kernel mode，請求 OS 執行服務，例如 read、write、open。
#### 3. Error exceptions：錯誤型例外通常會轉成 signal
第三頁是在講作業系統，尤其是 Unix / Linux 類系統，怎麼處理很多錯誤型 exception。

常見錯誤 exception 包含：
```
divide by zero
invalid operation
illegal memory reference
invalid instruction
```
這些事件發生後，kernel 通常會把它轉成 signal（訊號） 傳給目前的 process。

例如：
```
divide by zero        → SIGFPE
illegal memory access → SIGSEGV
invalid instruction   → SIGILL
debug / breakpoint    → SIGTRAP
```
投影片中的：
```
force_sig(sig_number, current);
```
意思是 kernel 對目前 process，也就是 current，送出某個 signal。這個 signal 可能會被 process 自己的 signal handler 處理；如果沒有處理，預設結果通常是 process 被終止。

所以第三頁說：

That will probably kill the process, but that’s not the concern of the exception handler

意思是 exception handler 的工作主要是辨識錯誤並送出正確 signal。至於 process 最後會不會死，是 signal handling 機制後續決定的事。
#### 4. Page fault 是重要例外，不一定代表程式錯
這裡要特別注意：**page fault 雖然叫 fault，但不一定是錯誤**。

Page fault 有兩種可能：

第一種是正常的 demand paging。程式要存取的 page 目前不在 RAM，但它是合法位址，只是還在 disk。OS 可以把它載入 memory，然後重新執行 instruction。

第二種是非法 memory access。例如程式存取完全沒有 mapping 的位址，或寫入 read-only page。這種就無法修復，kernel 可能送出 `SIGSEGV` 給 process。

所以 page fault handler 會先判斷：
```
這個 memory reference 合法嗎？
合法但 page 不在 memory → 載入 page，重新執行 instruction
不合法 → 發 signal，通常殺掉 process
```
#### 5. Kernel 裡也可能發生 exception
投影片最後說：

**An exception can happen in the kernel**

如果 exception 發生在 user process 中，kernel 通常可以送 signal 給該 process。
但如果 exception 發生在 kernel 自己執行的程式碼中，事情就嚴重很多。

例如 kernel code 產生非法 memory access，這代表 OS 內部可能有 bug 或硬體狀況異常。Linux 中常見說法是：
```
kernel oops
```
也就是 kernel 偵測到嚴重錯誤，可能印出錯誤訊息、終止相關 kernel path，嚴重時甚至 kernel panic。

投影片上的：
```
die(); // kernel oops
```
就是在描述這種情況。

#### 6. 考試理解
| 類型              | 來源            | 發生時間            | IP 指向               | 常見結果                       |
| --------------- | ------------- | --------------- | ------------------- | -------------------------- |
| Interrupt       | 外部 I/O device | 非同步，和目前指令無直接關係  | 下一條指令附近             | 進入 interrupt handler       |
| Exception       | CPU 執行指令時偵測到  | 同步，和目前指令有關      | 視種類而定               | 進入 exception handler       |
| Fault           | Exception 的一種 | instruction 完成前 | 通常指向原 instruction   | 可修復則重跑                     |
| Trap            | Exception 的一種 | instruction 完成後 | 通常指向下一條 instruction | 常用於 debugger / system call |
| Error exception | CPU 偵測到錯誤     | 同步              | 視錯誤而定               | 常轉成 signal                 |
**Fault** 的重點是：問題在 instruction 完成前被偵測到，IP 通常還指向原本那條 instruction。如果 OS 能修好，例如 page fault 把 page 載入 memory，就可以重新執行原 instruction。

**Trap** 的重點是：instruction 通常已經執行完，IP 已經往後移。它常用於 debugger、breakpoint、system call 這種「有意進入 handler」的情境。

**Error exception** 的重點是：像 divide by zero、invalid instruction、illegal memory reference 這類錯誤，kernel 常把它轉成 signal 送給目前 process；若 process 沒處理，通常會被終止。page fault 是例外中的重要特例，因為它可能是正常 virtual memory 機制的一部分，也可能是真正非法存取。
#### 7. System call
System call（系統呼叫）
使用者程式請求 OS 服務的方式，例如 read、write、open、fork。
程式執行特殊 instruction 觸發 trap，CPU 切到 kernel mode，OS 執行對應服務。
### 12. FLIH 與 SLIH
課本提到很多系統會把 interrupt handling 分成兩層：

FLIH = First-Level Interrupt Handler
第一層中斷處理器。

SLIH = Second-Level Interrupt Handler
第二層中斷處理器。

原因是 interrupt handler 不能做太久，否則會擋住其他 interrupt。
所以第一層 handler 通常只做最緊急、最短的事情：

儲存狀態
確認 interrupt source
讀取必要狀態
清除 device interrupt
把後續工作排進 queue

第二層 handler 再做比較耗時的工作：

複製資料
整理 buffer
喚醒 process
執行較完整的 I/O completion

這樣可以讓系統快速回應硬體，又不會在高優先權 interrupt handler 裡做太久。

## 課堂補充
### 觀念
這組補充投影片是在把前面課本的 **interrupt mechanism（中斷機制）** 補得更硬體一點：外部裝置怎麼發出中斷、PIC / APIC 怎麼轉成 vector、CPU 怎麼靠 IDT 找到 handler，以及 mask / priority 怎麼控制中斷是否要被處理。
### 12.18 Interrupt Hardware：傳統 PC 的中斷硬體架構
![image](https://hackmd.io/_uploads/S1DgPYcbMg.png)
這頁是在講早期 x86 PC 的中斷硬體流程。

**IRQ = Interrupt Request，中斷請求。**
每個 I/O device 如果需要 CPU 處理，就會透過一條 **IRQ line（中斷請求線）** 發出訊號。例如 Ethernet、SCSI Disk、Real-Time Clock、Keyboard Controller 都可能發出 IRQ。

**PIC = Programmable Interrupt Controller，可程式化中斷控制器。**
PIC 的工作是收集多個裝置送來的 IRQ，判斷是哪個裝置需要服務，然後把 IRQ 轉成 CPU 看得懂的 **interrupt vector（中斷向量）**。

圖中有兩顆 PIC：

**Master PIC（主 PIC）**
直接連到 CPU。

**Slave PIC（從 PIC）**
接收更多外部裝置的 IRQ，再接到 Master PIC。這種做法叫 **cascade（串接 / 級聯）**，目的是擴充可支援的 IRQ 數量。

流程可以看成：

```text
I/O device 發出 IRQ
→ Slave PIC 或 Master PIC 收到
→ PIC 判斷是哪一條 IRQ
→ PIC 把 IRQ 對應成 interrupt vector
→ Master PIC 透過 INTR 通知 CPU
→ CPU 進入對應 interrupt handler
```

**INTR = Interrupt Request input，中斷請求輸入腳位 / 訊號。**
在 x86 裡，PIC 會透過 INTR 線通知 CPU 有 maskable interrupt 需要處理。

這頁重點：**I/O device 不一定直接跟 CPU 說話，而是先送 IRQ 給 PIC；PIC 再把 IRQ 轉成 interrupt vector，交給 CPU 處理。**

---

### 12.19 Programmable Interrupt Controller：PIC 的責任
這頁是在講 **PIC = Programmable Interrupt Controller** 的功能。

PIC 的第一個責任是告訴 CPU：**有外部裝置想要 interrupt CPU**。但更重要的是，它不只要說「有中斷」，還要說清楚「是哪一個裝置需要服務」。

PIC 主要做幾件事：

第一，**translate IRQ to vector**。
把 IRQ number 轉成 interrupt vector。IRQ 是硬體線路編號，vector 則是 CPU 用來查 handler 的編號。

第二，**raise interrupt to CPU**。
PIC 透過 INTR 線通知 CPU。

第三，**vector available in register**。
CPU 回應中斷後，PIC 會把對應的 vector number 提供給 CPU。

第四，**waits for ack from CPU**。
**ACK = Acknowledge，確認訊號。**
CPU 收到 interrupt 後會回覆 acknowledge，表示已經知道有中斷要處理。

第五，**prioritize multiple requests**。
如果多個裝置同時發出 IRQ，PIC 要根據 priority 決定哪個先處理。

第六，**mask interrupts**。
可以在 PIC 或 CPU 端暫時遮蔽某些 interrupt，也就是讓某些 IRQ 暫時不要打斷 CPU。

最後一行提到早期系統會使用兩顆 **8259A PIC**，每顆有 8 input，兩顆 cascade 起來，形成傳統 PC 的 IRQ 架構。

---

### 12.20 APIC、IO-APIC、LAPIC：現代多處理器中斷架構

這頁是在講比較現代的中斷控制器。

**APIC = Advanced Programmable Interrupt Controller，進階可程式化中斷控制器。**
APIC 是 PIC 的進階版，適合 **SMP systems**。

**SMP = Symmetric Multiprocessing，對稱多處理器系統。**
意思是系統中有多個 CPU / CPU cores，而且 OS 可以把工作分配給不同 CPU 執行。

傳統 PIC 比較適合單 CPU 系統；APIC 則支援多 CPU，能把 interrupt route 到指定 CPU 或一組 CPU。

---

這頁提到兩個重要元件：

**IO-APIC = Input/Output Advanced Programmable Interrupt Controller，輸入輸出進階可程式化中斷控制器。**
它負責接收外部 I/O devices 的 interrupt。

**LAPIC = Local Advanced Programmable Interrupt Controller，本地進階可程式化中斷控制器。**
每個 CPU core 通常有自己的 Local APIC。它負責接收送給該 CPU 的 interrupt，並和其他 CPU / APIC 溝通。

流程是：

```text
I/O device
→ IO-APIC
→ system bus
→ 某顆 CPU 的 LAPIC
→ CPU 處理 interrupt
```

**IPI = Inter-Processor Interrupt，處理器間中斷。**
這是 CPU 之間互相傳送的 interrupt。例如 CPU 0 想通知 CPU 1 做某件事，就可以送 IPI。

---

這頁還提到 **interrupt routing（中斷路由）**。APIC 可以決定 interrupt 要送給哪一顆 CPU。可以 broadcast 給多顆 CPU，也可以 selective routing 只送給某顆 CPU。

這樣的好處是可以 **distribute interrupt handling load**，把中斷處理工作分散到不同 CPU，不要全部壓在同一顆 CPU。

**TPR = Task Priority Register，任務優先權暫存器。**
APIC 可以參考 TPR 判斷 CPU 當前優先權，並把 interrupt route 到合適的 CPU。如果多顆 CPU 優先權相同，就可以用 round-robin 方式輪流分配。

---

### 12.21 Assigning IRQs to Devices：IRQ 怎麼分配給裝置

這頁是在講 **IRQ assignment（中斷請求線分配）**。

IRQ 分配跟硬體架構有關。有些 IRQ 是寫死的，有些是透過實體 jumper / 設定決定，有些則可以由系統程式化設定。

**PCI = Peripheral Component Interconnect，周邊元件互連匯流排。**
PCI bus 通常會在開機時分配 IRQ 給裝置。也就是 OS / BIOS / firmware 會掃描裝置，然後安排它們使用哪些 interrupt resource。

有些 IRQ 是固定的，例如：

```text
IRQ0：Interval timer
IRQ2：Cascade pin for 8259A
```

**Interval timer（間隔計時器）** 會週期性產生 timer interrupt。
**Cascade pin** 則是用來把 Slave PIC 接到 Master PIC 的那條 IRQ，所以 IRQ2 在傳統 PC 架構中有特殊用途。

投影片也提到 Linux device drivers 會在 device 被 opened 時 request IRQ。這對動態載入的 driver 很重要，例如：

**USB = Universal Serial Bus，通用序列匯流排。**
**PCMCIA = Personal Computer Memory Card International Association**，通常指早期筆電常見的 PC Card 擴充介面。

最後一句說，兩個不會同時使用的裝置可以共享同一個 IRQ，即使硬體不支援真正同時共享。意思是如果兩個裝置不會同時打斷 CPU，系統可以在軟體層面讓它們輪流使用同一個 IRQ。

---

### 12.22 / 12.23 Assigning Vectors to IRQs：IRQ 怎麼對應到 Vector

這兩頁內容幾乎一樣，重點是 **IRQ number 會被對應到 interrupt vector number**。

**Vector（中斷向量）** 是一個 0 到 255 的 index。CPU 會用這個 index 去查 **IDT = Interrupt Descriptor Table，中斷描述表**。

投影片寫：

```text
Vectors usually IRQ# + 32
```

意思是一般外部裝置 IRQ 會被映射到：

```text
vector = IRQ number + 32
```

為什麼要加 32？因為 x86 中：

```text
vector 0～31：保留給 CPU exceptions / non-maskable interrupts
vector 32～255：可用於 maskable interrupts 等用途
```

所以一般裝置 interrupt 不會放在 0～31，避免跟 CPU exception 衝突。

其他重點：

```text
Vector 128 used for syscall
Vectors 251–255 used for IPI
```

**syscall = system call，系統呼叫。**
早期 Linux 常用 `int 0x80`，也就是 vector 128，讓 user program 進入 kernel 請求服務。

**IPI = Inter-Processor Interrupt，處理器間中斷。**
多處理器系統中，CPU 之間互相通知用。

---

### 12.24 Interrupt Descriptor Table：IDT 是什麼

**IDT = Interrupt Descriptor Table，中斷描述表。**
它是一張表，CPU 用 interrupt vector 當 index，查出對應 interrupt handler 的入口位置。

也就是：

```text
interrupt vector
→ 查 IDT
→ 找到 handler entry point
→ CPU 跳到 handler 執行
```

IDT 裡面放的是 **gate descriptor（閘描述符）**。每個 descriptor 會描述如何進入某個 handler。

投影片列出 IDT descriptor 包含：

**Segment selector + offset for handler**
用來定位 handler 的程式碼位置。
在 x86 protected mode 裡，handler 位址通常由 code segment selector 加上 offset 組成。

**DPL = Descriptor Privilege Level，描述符特權等級。**
用來控制哪個 privilege level 可以使用這個 gate。這和 user mode / kernel mode 權限切換有關。

**TSS = Task State Segment，任務狀態段。**
Task gate 可能用到 TSS 來做 task transfer，不過投影片也註明 Linux 不使用這種方式。

---

IDT 裡有不同 gate：

**Task gate**
透過 TSS 轉換任務，Linux 不常用。

**Interrupt gate**
進入 handler 時會 disable further interrupts，也就是暫時關閉後續 maskable interrupts，避免中斷處理過程又被其他中斷打斷。

**Trap gate**
進入 handler 後仍允許 further interrupts。常用於 trap / exception 類型，例如 debug trap、system call trap 等場景。

簡單記：

```text
Interrupt gate：進 handler 後會關中斷
Trap gate：進 handler 後不關中斷
```

---

### 12.25 Interrupt Masking：中斷遮蔽

**Interrupt masking（中斷遮蔽）** 是讓某些 interrupt 暫時不要被處理。

這頁說有兩種：

**Global masking（全域遮蔽）**
延後所有 interrupts。也就是整體關閉或暫停中斷處理。

**Per-IRQ / selective masking（單一 IRQ / 選擇性遮蔽）**
只遮蔽某些特定 IRQ，其他 IRQ 還是可以進來。

通常比較需要的是 selective masking，因為干擾最常來自「同類型 interrupt 重複進來」。例如某個網路卡 interrupt handler 正在處理網路卡事件，如果同一張網路卡又一直送 interrupt，就可能造成重入或干擾，因此可以暫時 mask 該 IRQ。

重點是：

**Masking 不是把事件消失，而是延後或暫時不讓它打斷 CPU。**

---

### 12.26 Putting It All Together：整體流程圖
![image](https://hackmd.io/_uploads/Hy2mwtcZzg.png)
這張圖把前面所有東西串起來。

左邊是 **PIC = Programmable Interrupt Controller**。
它接收多條 **IRQs = Interrupt Requests**。N 表示有多條 IRQ line。

PIC 收到某個 IRQ 後，會根據設定做 priority / masking / vector mapping。若該 IRQ 沒有被 mask，PIC 就透過：

```text
INTR
```

通知 CPU。

**INTR = Interrupt Request input，中斷請求輸入訊號。**

CPU 收到 INTR 後，會取得對應的 **vector number**。CPU 內部有：

**IDTR = Interrupt Descriptor Table Register，中斷描述表暫存器。**
IDTR 存放 IDT 的基底位址，也就是 CPU 要去哪裡找 IDT。

接著 CPU 用 vector 當 index 去查：

**IDT = Interrupt Descriptor Table，中斷描述表。**

IDT 裡對應 entry 會指出 handler 的入口位置。CPU 依照 descriptor 跳到對應的：

**handler = interrupt handler，中斷處理程式。**

完整流程是：

```text
I/O device 發出 IRQ
→ PIC 收到 IRQ
→ PIC 檢查 masking / priority
→ PIC 轉成 vector
→ PIC 透過 INTR 通知 CPU
→ CPU 使用 IDTR 找到 IDT
→ CPU 用 vector index 查 IDT
→ IDT entry 指向 interrupt handler
→ CPU 執行 handler
```

圖上也標了 **Mask points**，代表 masking 可以在不同地方發生，例如 PIC 端可以 mask 某條 IRQ，CPU 端也可以全域延後 maskable interrupts。

---

### 整組投影片總結

這組補充的核心是：外部裝置不是直接跳進 CPU 執行 handler，而是先透過 **IRQ line** 發出請求。傳統系統由 **PIC** 收集 IRQ、排優先權、處理 masking，並把 IRQ 映射成 **interrupt vector**。CPU 收到 interrupt 後，透過 **IDTR** 找到 **IDT**，再用 vector 查出對應的 **interrupt handler**。

現代多處理器系統則使用 **APIC** 架構，其中 **IO-APIC** 接收外部裝置 interrupt，**LAPIC** 位於每顆 CPU 上，負責接收與處理被 route 過來的 interrupt。APIC 支援 interrupt routing、IPI、多 CPU 負載分配與 priority 控制，比傳統 PIC 更適合現代系統。

## 12.2.4 Direct Memory Access
### 觀念
DMA = Direct Memory Access（直接記憶體存取）。核心目的很簡單：大量資料搬移時，不要讓 CPU 一個 byte 一個 byte 親自搬，而是交給 DMA controller 直接在 I/O device 和 main memory 之間搬資料。
### 1. 為什麼需要 DMA？
如果不用 DMA，傳統方式叫 **programmed I/O**。

Programmed I/O 的做法是 CPU 一直執行指令，從 device controller 的 data register 讀一點資料，再寫到 memory；或從 memory 讀資料，再寫到 device register。

概念像這樣：
```
CPU 讀 device register 1 byte
CPU 寫 memory 1 byte
CPU 讀 device register 1 byte
CPU 寫 memory 1 byte
...
```
這對少量資料還可以，但如果是 disk read、network packet、audio stream、video frame 這種大量資料，CPU 會被資料搬運拖住，沒辦法做其他工作。

DMA 就是為了解決這件事。DMA 讓資料可以：
```
I/O device ↔ memory
```
直接傳，不必每個 byte 都經過 CPU register。

注意：DMA 不是完全不用 CPU。CPU 還是要負責「設定 DMA 任務」，只是設定完後，真正大量資料搬移交給 DMA controller 做。
### 2. DMA controller 是什麼？
**DMA controller = Direct Memory Access controller（直接記憶體存取控制器）**。

它是一個硬體控制器，可以代表 CPU 去操作 memory bus，把資料從裝置搬到 memory，或從 memory 搬到裝置。

在 DMA 傳輸中，CPU 的角色比較像「下命令的人」；DMA controller 則是「真正搬貨的人」。
```
CPU：請把 disk 的資料搬到 memory address X，總共搬 C bytes
DMA controller：收到，開始搬
CPU：去做其他事情
DMA controller：搬完後用 interrupt 通知 CPU
```
### 3. DMA command block 裡面放什麼？
**OS writes DMA command block into memory**

意思是 OS = Operating System（作業系統） 會在 memory 裡準備一塊資料結構，叫 **DMA command block**。這個 command block 告訴 DMA controller 這次傳輸要怎麼做。

通常會包含：
```
source address：來源位址
destination address：目的位址
read or write mode：讀取或寫入模式
count of bytes：要傳輸幾個 bytes
```
例如 disk read：
```
source：disk controller / disk block
destination：memory buffer address X
mode：read
count：4096 bytes
```
例如 disk write：
```
source：memory buffer address X
destination：disk controller / disk block
mode：write
count：4096 bytes
```
OS 把 command block 寫好後，會把這個 command block 的位置告訴 DMA controller。之後 DMA controller 就能照著這些資訊去搬資料。


### 4. ==DMA 的完整操作流程==
![image](https://hackmd.io/_uploads/BJukFF9Zfg.png)
Six Step Process to Perform DMA Transfer，也就是 DMA 傳輸的六步流程。
#### Step 1：Device driver 接到 I/O 要求
某個 process 要讀 disk，OS 進入 device driver。
**Device driver（裝置驅動程式）** 會準備一塊 memory buffer，例如位址是 X。
```
要把 disk data 傳到 memory buffer X
```
#### Step 2：Device driver 告訴 disk controller
Device driver 會告訴 **disk controller（磁碟控制器）**：
```
把 C bytes 從 disk 傳到 memory buffer X
```
這裡 driver 會設定 DMA command block 或 controller registers，包含 buffer address、byte count、傳輸方向等。
#### Step 3：Disk controller 啟動 DMA transfer
Disk controller 開始進行 DMA 傳輸。
也就是它準備把 disk 上讀到的資料，不經 CPU 逐 byte 搬運，而是交給 DMA 機制送到 memory。
#### Step 4：Disk controller 把資料交給 DMA controller
Disk controller 把資料 byte by byte 或 block by block 送給 DMA controller。

圖中可以看到 disk controller 在 PCI bus 上，DMA / bus / interrupt controller 連接到 CPU memory bus。
**PCI = Peripheral Component Interconnect（周邊元件互連匯流排）。
IDE = Integrated Drive Electronics（整合式磁碟電子介面）。**

#### Step 5：DMA controller 把資料寫進 memory buffer
DMA controller 把資料寫到 memory 的 `X`，每寫一段就更新內部狀態：
```
memory address X 遞增
count C 遞減
```
所以流程像：
```
寫到 X
X = X + 1
C = C - 1

寫到 X
X = X + 1
C = C - 1
...
直到 C = 0
```
這個階段大量資料搬移主要由 DMA controller 負責，CPU 不需要每個 byte 介入。

#### Step 6：DMA 完成後發 interrupt

當 `C = 0`，表示資料搬完。
DMA controller 會透過 **interrupt（中斷）** 通知 CPU：
```
DMA transfer completed
```
CPU 收到 interrupt 後，進入 interrupt handler，OS 會知道這次 I/O 完成，接著可能喚醒原本等待 disk read 的 process。
### 5. Bus mastering 是什麼？
投影片寫：

Bus mastering of DMA controller — grabs bus from CPU

Bus mastering（匯流排主控） 是指 DMA controller 可以暫時成為 bus master，也就是取得 bus 的控制權，直接對 memory 發出讀寫操作。

平常 CPU 是主要的 bus master，因為 CPU 要讀寫 memory。
但 DMA controller 要直接搬資料時，也需要使用 memory bus。因此它會向系統請求 bus 控制權。

概念像：
```
DMA controller：CPU，借一下 memory bus
CPU / bus arbiter：可以，暫停一下
DMA controller：搬資料
DMA controller：搬完或搬一小段後釋放 bus
CPU：繼續使用 bus
```
### 6. Cycle stealing 是什麼？
**Cycle stealing（週期竊取）** 是 DMA 使用 bus 的一種方式。

意思是 DMA controller 不是一次霸佔 bus 很久，而是從 CPU 那邊「偷幾個 memory cycles」來搬資料。

例如 CPU 原本可以連續使用 memory bus，但 DMA 插進來使用幾個 bus cycle：
```
CPU cycle
CPU cycle
DMA cycle
CPU cycle
DMA cycle
CPU cycle
...
```
這樣 CPU 會稍微變慢，因為部分 memory bus 時間被 DMA controller 拿走了。
但整體仍然比 programmed I/O 高效很多，因為 CPU 不用親自執行大量資料搬移指令。

所以投影片說：

**Cycle stealing from CPU but still much more efficient**

意思是 DMA 會搶一些 CPU 可用的 bus cycle，但比 CPU 一個 byte 一個 byte 搬資料有效率很多。
### 8. DVMA 是什麼？
投影片最後提到：

**DVMA = Direct Virtual Memory Access（直接虛擬記憶體存取）**。

一般 DMA 常用的是 **physical address（實體位址）**，因為 DMA controller 直接對 memory bus 發出 memory transaction。可是 OS 和 process 平常看到的是 **virtual address（虛擬位址）**。

這會造成一個問題：
```
Process buffer 是 virtual address
DMA controller 通常需要 physical address
```
所以一般做法是 OS / driver 要把 virtual address 轉成 physical address，並確保那段 memory 在 DMA 期間不會被換出或移動。

而 **DVMA** 或搭配 **IOMMU = Input-Output Memory Management Unit（輸入輸出記憶體管理單元）** 的系統，可以讓裝置使用類似 virtual address 的位址，由硬體替 DMA 做位址轉換與保護。

好處是：
```
裝置不必直接暴露所有 physical memory
OS 可以更安全地限制 device 可存取範圍
scatter-gather DMA 更容易管理
```
這也是現代系統中很重要的安全與效能設計。
### 9. DMA 和 cache 的小提醒

DMA 直接寫 memory，但 CPU 可能有 cache。這會引出 cache coherence（一致性） 問題。

例如 DMA 把 disk data 寫到 memory buffer，但 CPU cache 裡還保留舊資料。
如果 CPU 讀到 cache 舊資料，就會錯。

所以 OS / hardware 需要確保：
```
DMA 寫入後，CPU 看到的是新資料
DMA 讀取前，memory 裡的是 CPU 最新寫入的資料
```
有些系統靠硬體 cache coherence 解決，有些需要 driver 在 DMA 前後做 cache flush / invalidate。

這點課本通常不會展開太多，但技術上非常重要。
# 12.3 Application I/O Interface
## 序言
### 觀念
這節在講一個很重要的設計目標：**I/O 裝置差異很大，但應用程式不應該直接面對這些差異**。
例如程式要開啟磁碟上的檔案時，不需要知道底下是 SAS disk、USB flash drive、SSD、HDD，也不需要知道控制器細節。作業系統會把不同硬體包裝成幾種標準介面，讓應用程式用一致的方法操作。
### 核心概念：Abstraction、Encapsulation、Layering
這段課本一直在強調三個軟體工程概念。

**Abstraction（抽象化）：** 把複雜細節藏起來，只留下高層需要知道的功能。例如應用程式只知道 `read()`、`write()`，不用知道硬碟控制器怎麼運作。

**Encapsulation（封裝）：** 把硬體差異包在 device driver 裡。不同裝置有不同控制方式，但 driver 對 kernel I/O subsystem 提供標準介面。

**Layering（分層）：** I/O 系統被分成多層，每層只跟上下層互動，降低複雜度。

簡單講就是：
```
Application
↓
System call，例如 open(), read(), write()
↓
Kernel I/O subsystem
↓
Device driver
↓
Device controller
↓
Actual device
```
應用程式看到的是統一的 system call；kernel I/O subsystem 看到的是統一的 driver interface；driver 則負責處理真正硬體細節。
### Figure 12.7：A kernel I/O structure
![image](https://hackmd.io/_uploads/rybJEqcbzl.png)
這張圖是在畫 **kernel I/O structure（核心 I/O 架構）**。

最上層是 **kernel（核心）**，它是 OS 的主要部分。
中間是 **kernel I/O subsystem（核心 I/O 子系統）**，負責提供一般化的 I/O 管理，例如 buffering、caching、scheduling、error handling 等。
再下面是一排 **device drivers（裝置驅動程式）**，例如：
```
SAS device driver
keyboard device driver
mouse device driver
PCIe bus device driver
802.11 device driver
USB device driver
```
**SAS = Serial Attached SCSI，序列式連接 SCSI。
SCSI = Small Computer System Interface，小型電腦系統介面。
PCIe = Peripheral Component Interconnect Express，高速周邊元件互連匯流排。
802.11 是 IEEE 無線網路標準，也就是 Wi-Fi 常見標準系列。
USB = Universal Serial Bus，通用序列匯流排。**

圖的下半部是硬體層。每個 driver 下面對應一個 **device controller（裝置控制器）**，再下面才是真正的 device。例如 keyboard driver 對 keyboard controller，keyboard controller 再控制 keyboard。

這張圖的重點是：**kernel I/O subsystem 不直接碰每個硬體，而是透過 device driver。Driver 把硬體差異封裝起來，讓上層看到統一介面**。

### Figure 12.8：I/O devices 的差異分類
![image](https://hackmd.io/_uploads/r1O-Hc5bMg.png)
這張表是在說，I/O devices 可以從很多角度分類。OS 需要理解這些差異，但應用程式通常不需要直接面對。

**Data-transfer mode（資料傳輸模式）** 可以分成 **character-stream** 和 **block**。
Character-stream device 是一個 byte、一個 byte 傳，例如 terminal、keyboard。Block device 則是一整塊資料一起傳，例如 disk。

**Access method（存取方法）** 可以分成 **sequential** 和 **random**。
Sequential access 是資料照固定順序存取，例如 modem 或 tape。Random access 是可以直接跳到某個位置讀寫，例如 disk、CD-ROM。

**Transfer schedule（傳輸時程）** 可以分成 **synchronous** 和 **asynchronous**。
Synchronous device 的反應時間比較可預測，可以和系統其他動作協調。Asynchronous device 的事件比較不可預測，例如 keyboard 不知道使用者什麼時候按鍵。

**Sharing（共享性）** 可以分成 **dedicated** 和 **sharable**。
Dedicated device 同一時間通常只能給一個 process 使用，例如 tape drive。Sharable device 則可以被多個 process 或 thread 使用，例如某些磁碟或共享輸入資源。

**Device speed（裝置速度）** 差異很大，可能用 latency、seek time、transfer rate、delay between operations 來描述。
例如 disk 有 seek time，network device 重視 transfer rate，keyboard 則速度很慢但事件不固定。

**I/O direction（I/O 方向）** 可以分成 read-only、write-only、read-write。
例如 **CD-ROM = Compact Disc Read-Only Memory**，通常是 read-only；graphics controller 常被視為 write-only output device；disk 則是 read-write。
### OS 會把裝置整理成幾種標準存取模型
雖然硬體差異很多，OS 會把它們歸類成幾種常見介面：

**Block I/O：** 以 block 為單位讀寫，例如 disk。
**Character-stream I/O：** 以 byte stream 為單位，例如 keyboard、terminal、serial port。
**Memory-mapped file access：** 把檔案映射到 virtual memory，程式像讀寫 memory 一樣操作檔案。
**Network sockets：** 用 socket 介面處理網路傳輸。

這些介面讓應用程式不需要關心底下到底是什麼硬體，只要使用標準 API。
### ioctl：特殊控制通道
課本提到一個重要的 escape / back door：

**ioctl = input/output control，I/O 控制。**

一般 `read()`、`write()` 處理資料傳輸，但有些裝置需要特殊操作，例如設定 baud rate、查詢裝置狀態、切換模式、控制硬體功能。這些操作很難每個都設計成新的 system call，所以 UNIX / Linux 提供 `ioctl()`。

`ioctl()` 通常有三個重點參數：
```
device identifier：指定哪個裝置
command number：指定要執行哪個控制命令
data pointer：指向額外資料結構，用來交換控制資訊
```
所以 `ioctl()` 就像一個通用控制通道，讓 application 可以透過 driver 執行裝置特有功能。
### Major number / Minor number
最後課本講 UNIX / Linux 的 device identifier。

在 UNIX / Linux 裡，裝置通常用 **major number** 和 **minor number** 辨識。

**Major number（主裝置號）** 表示裝置類型，OS 用它找到對應的 device driver。
**Minor number（次裝置號）** 表示同一類裝置中的具體實例，例如第幾個 partition、第幾個裝置。

課本例子：
```
/dev/sda   major 8, minor 0
/dev/sda1  major 8, minor 1
/dev/sda2  major 8, minor 2
/dev/sda3  major 8, minor 3
```
這裡 `8` 是 major number，代表這類 block device 對應到某個 disk driver。
0、1、2、3 是 minor number，用來區分 /dev/sda 整顆磁碟和不同 partition。

所以 I/O request 的路由概念是：
```
看到 major number
→ 找到正確 device driver

看到 minor number
→ 找到該 driver 管理的具體裝置或分割區
```
## 12.3.1 Block and Character Devices
### 一、Block Device：區塊裝置
這小節在講作業系統如何把不同 I/O 裝置整理成兩大常見介面：**block-device interface** 和 **character-stream interface**。前面 12.3 提過，I/O 裝置種類很多，所以 OS 需要用抽象化把差異包起來。這裡就是開始介紹其中兩種最基本的抽象模型。

**Block device** 是以「一整塊資料」為單位傳輸的裝置。典型例子是 **disk**、**SSD**、**USB storage**、**CD-ROM** 這類儲存裝置。

**Block-device interface** 會提供幾個基本操作：
```
read()
write()
seek()
```
`read()` 是讀資料，`write()` 是寫資料，`seek()` 是移動到指定位置。因為 block device 通常支援 **random access（隨機存取）**，所以系統可以指定要讀哪一個 block，不一定要從頭開始順序讀。

例如磁碟可以被看成一個很長的一維 block array：
```
block 0, block 1, block 2, block 3, ...
```
程式或 OS 可以說：「讀 block 5000」或「寫 block 12000」。這就是 block device 的特性。

課本說 applications 通常不是直接操作 block device，而是透過 **file-system interface（檔案系統介面）** 存取。也就是應用程式只知道打開檔案、讀檔、寫檔，不需要知道檔案底下對應到哪幾個 disk blocks。

例如：
```
Application 呼叫 read(file)
→ File system 判斷檔案資料在哪些 blocks
→ Block-device driver 讀取對應 disk blocks
→ 資料回到 application
```
所以 block-device interface 的作用是把不同儲存裝置的底層差異隱藏起來。HDD、SSD、USB 隨身碟硬體不同，但上層都可以用類似的 block read / write 概念操作。
### 二、Raw I/O：直接把 block device 當線性 block array 用
課本接著提到 raw I/O。

**Raw I/O** 的意思是：繞過一般 file system，把 block device 當成單純的 block array 直接存取。

一般情況下，file system 會提供很多服務，例如 buffering、caching、locking、metadata management。但有些特殊應用，例如 database-management system（資料庫管理系統），可能想自己管理資料布局、buffer、locking。這時如果再經過 file system，反而會造成重複管理。

例如資料庫本身已經有 buffer pool，也有自己的 lock manager。如果 OS file system 又幫忙 buffer 和 locking，可能變成：
```
Database 自己 buffer 一次
File system 又 buffer 一次
→ 重複浪費記憶體

Database 自己 lock block
OS 又 lock file region
→ 可能冗餘甚至衝突
```
所以 raw I/O 的想法是：**讓 application 更直接控制 device，OS file system 盡量退到旁邊**。

缺點也很明顯：OS 原本提供的保護與管理服務變少，所以 application 必須自己負責更多細節。

### 三、Direct I/O：折衷做法
課本說現在常見的折衷做法是 direct I/O。

Direct I/O 不是完全繞過 file system，而是讓 application 仍然透過檔案操作，但要求 OS 對這些 I/O 不做一般 buffering 或 locking。

在 UNIX 世界中，這種做法叫 direct I/O。

它的概念是：
```
仍然使用 file system 的檔案名稱、權限、配置管理
但資料傳輸時盡量避免 OS page cache / buffering
```
所以 direct I/O 比 raw I/O 稍微安全、好管理，但又比一般 buffered I/O 更適合資料庫這種已經自己管理 buffer 的應用。

可以這樣區分：
```
一般 file I/O：
經過 file system，也經過 OS buffering / caching。

Raw I/O：
幾乎直接操作 block device，file system 退場。

Direct I/O：
仍走 file system，但關掉或減少 buffering / locking。
```
### 四、Memory-Mapped File Access：用記憶體方式操作檔案
接著課本提到 memory-mapped file access。

這個概念是把檔案映射到 process 的 virtual memory。程式不再明確呼叫 `read()` 或 `write()` 搬資料，而是把檔案看成一段 memory。
例如某個檔案被 map 到 virtual address：
```
0x40000000 ~ 0x4000FFFF
```
程式讀寫這段 memory，就像讀寫一般 array。但背後 OS 會在需要時，把對應的 disk blocks 載入 memory，或在適當時機把修改過的內容寫回 disk。

流程大概是：
```
程式存取 memory-mapped file 的某個位址
→ 該 page 不在 memory
→ page fault
→ OS 從 disk 把檔案對應部分載入 RAM
→ 程式繼續像操作 memory 一樣操作檔案
```
這種方式的好處是方便又有效率。因為資料傳輸由 demand paging 機制處理，不需要程式一直手動 `read()` / `write()`。

課本也提到 executable program 的載入就是常見例子。OS 可以把 executable file map 到 memory，然後跳到 entry address 開始執行。程式碼 page 需要時才真的從 disk 載入 memory。
### 五、Character Device：字元串流裝置
Character device 是以 byte stream 或 character stream 的形式傳輸資料的裝置。典型例子是：
```
keyboard
mouse
terminal
serial port
modem
printer
audio device
```
它不像 block device 那樣有「第幾個 block」的概念。Character device 比較像一條資料流，一個 byte 接著一個 byte 進來或出去。

課本舉 keyboard 為例。Keyboard 產生輸入時，不是 application 說「讀第 100 個 block」，而是使用者按鍵後，資料自然進入系統。這種輸入具有 **spontaneous（自發性）**，因為 application 無法準確預測使用者什麼時候按鍵。

Character-stream interface 基本 system calls 是：
```
get()
put()
```
`get()` 從 character stream 取得一個字元或 byte。
`put()` 把一個字元或 byte 輸出到 character stream。

在這層介面上，library 通常還會提供更高階功能，例如 line-at-a-time input，也就是一次讀一整行。這時 library 可能會處理 buffering 和 editing。

例如使用者在 terminal 輸入一行文字時，如果按 backspace，前一個字元會從 input stream 中移除。這些處理通常不是最底層硬體直接完成，而是由 OS / terminal driver / library 提供。
### 六、Block Device vs Character Device 差異
Block device 的資料有明確位置，可以用 block number 定位，也常支援 seek。Character device 則比較像資料流，資料按順序來，通常不能隨便跳到某個位置。
```
Block device：
以 block 為單位
通常支援 random access
常見操作 read / write / seek
例子：disk、SSD、USB storage

Character device：
以 byte stream 為單位
通常是 sequential stream
常見操作 get / put
例子：keyboard、mouse、terminal、printer
```
簡單比喻：

**Block device 像一本書，可以直接翻到第 500 頁。Character device 像電話對話，話是一句一句流過來，不能直接跳到還沒講出的第 500 句。**
## 12.3.2 Network Devices
## 12.3.3 Clocks and Timers
## 12.3.4 Nonblocking and Asynchronous I/O：非阻塞與非同步 I/O
### 觀念
這節在比較三種 I/O system call 的行為：**blocking I/O**、**nonblocking I/O**、**asynchronous I/O**。

這三者都在處理同一個問題：**I/O device 通常很慢，而且完成時間不一定能預測**。 例如鍵盤不知道何時會有輸入、磁碟讀取要等裝置、網路資料可能晚點才到。如果程式每次 I/O 都卡住等待，整體執行效率可能不好。

### 1. Blocking I/O：阻塞式 I/O
**Blocking I/O** 的意思是：程式發出 I/O system call 後，如果資料還沒準備好，呼叫它的 thread 會被暫停。

例如程式呼叫：
```
read(fd, buffer, size)
```
如果資料還沒從 disk 或 network 到達，這個 thread 不會繼續往下跑，而是被 OS 從 **run queue** 移到 **wait queue**。

**Run queue** 是可以被 CPU 排程執行的 thread / process queue。
**Wait queue** 是正在等待某個事件的 thread / process queue，例如等待 I/O 完成。

流程是：
```
thread 發出 blocking read()
→ I/O 尚未完成
→ thread 進入 wait queue
→ I/O 完成
→ thread 回到 run queue
→ 被 scheduler 選到後繼續執行
```
Blocking I/O 的好處是程式比較好寫，邏輯很直覺：
```
讀資料
→ 等資料讀完
→ 繼續處理資料
```
缺點是等待期間該 thread 不能做其他事情。
### 2. 為什麼需要 Nonblocking I/O？
有些程式不能因為某個 I/O 還沒完成就整個卡住。

例如圖形介面程式，一邊要接收 keyboard / mouse input，一邊要更新畫面。如果 `read()` keyboard 時卡住，畫面就可能不更新，使用者體驗很差。

另一個例子是 video player。它可能一邊從 disk 讀影片 frame，一邊解壓縮，一邊顯示畫面。如果讀檔卡住整個程式，就無法流暢播放。

所以需要 **nonblocking I/O**。
### 3. Nonblocking I/O：非阻塞式 I/O
Nonblocking I/O 的意思是：system call 不會讓 thread 長時間等待。它會很快返回，並告訴程式目前完成了多少資料傳輸。

例如 nonblocking `read()`：
```
如果資料已經有了
→ 立刻讀取可用資料並返回

如果資料還沒有
→ 立刻返回，表示目前沒有資料可讀
```
所以 nonblocking I/O 的重點是：不等待完整 I/O 完成，只拿目前可用的結果。

課本特別說，nonblocking read 會立即返回：
```
可能回傳完整資料量
可能回傳較少資料
也可能完全沒有資料
```
例如要求讀 1000 bytes，但目前只到 300 bytes，那 nonblocking read 可能只回傳 300 bytes；如果目前 0 bytes 可用，就回傳沒有資料可讀的狀態。

這種方式的缺點是 application code 會比較複雜，因為程式要自己處理「資料還沒到」、「只到一部分」、「之後再試」等情況。
### 4. Asynchronous I/O：非同步 I/O
**Asynchronous I/O** 和 **nonblocking I/O** 很像，都是呼叫後不會卡住 thread。但兩者差在完成方式。

**Asynchronous I/O** 的意思是：程式發出 I/O request 後，system call 立刻返回，thread 繼續執行；真正的 I/O transfer 會在背景完成。等到未來某個時間 I/O 完成時，OS 再通知 application。

通知方式可能是：
```
設定某個變數
發 signal
software interrupt
callback routine
```
**Callback routine（回呼函式）** 是指 I/O 完成後，由系統或 runtime 呼叫指定函式來處理結果。
所以 asynchronous I/O 的重點是：
```
先提出完整 I/O 要求
→ 不等待，立刻返回
→ I/O 在背景完成
→ 完成後通知程式
```
這和 nonblocking I/O 最大差異是：
```
Nonblocking I/O：
只檢查現在能不能做，能做多少就做多少，不能做就立刻返回。

Asynchronous I/O：
先提交完整 I/O request，之後由 OS 在背景完成，完成後再通知。
```
### 5. Figure 12.9：Synchronous vs Asynchronous I/O
#### Figure 12.9(a)：Synchronous I/O
![image](https://hackmd.io/_uploads/HkcNtZiZMe.png)
左圖中，requesting process 從 user land 進入 kernel，呼叫 device driver。接著 hardware data transfer 開始。重點是上方標了 **waiting**，表示 requesting process 在 I/O 完成前會等待。

流程如下：
```
requesting process 發出 I/O
→ 進入 kernel
→ device driver 啟動硬體傳輸
→ process 等待
→ hardware data transfer 完成
→ interrupt handler 處理完成事件
→ process 回到 user land 繼續執行
```
這就是 blocking / synchronous 的典型行為：**I/O 沒完成前，原本的 process 不繼續往下跑**。
#### Figure 12.9(b)：Asynchronous I/O
![image](https://hackmd.io/_uploads/HyNwFbjZMx.png)
右圖中，requesting process 發出 I/O request 後，很快就回到 user land 繼續執行。硬體傳輸仍然在下方背景進行。等 hardware data transfer 完成後，interrupt handler 再通知或處理完成事件。
流程如下：
```
requesting process 發出 I/O
→ 進入 kernel
→ device driver 啟動硬體傳輸
→ process 立刻回到 user land 繼續執行
→ hardware data transfer 在背景進行
→ 完成後 interrupt handler 處理完成事件
→ OS 通知 requesting process
```
這就是 asynchronous 的重點：**I/O request 和 process 後續執行可以重疊**。
### 6. OS 內部也大量使用 Asynchronous I/O
課本提到，很多 asynchronous activity 不一定直接暴露給 application，而是在 OS 內部使用。

例如 storage write。Application 呼叫 `write()` 後，OS 可能只是先把資料放進 buffer 或 page cache，然後就回傳。真正寫到 disk 可能晚一點才發生。

這樣做可以提升效能，因為 OS 可以合併多次寫入、重新排序磁碟 I/O、減少立即寫入的成本。

不過風險是：如果資料還在 memory buffer 裡，尚未真正寫到 disk，系統突然當機，這些 in-flight requests 可能會遺失。

In-flight request 指的是已經提交、但還沒真正完成的 I/O request。

因此 OS 通常會限制 buffer 保留時間。例如某些 UNIX 系統會每 30 秒 flush secondary-storage buffers。

**Flush** 的意思是把 buffer 中尚未真正寫出的資料強制寫到儲存裝置。
### 7. select()：一種 Nonblocking / Multiplexing 的例子
課本最後提到 **select()**。

`select()` 常用於 network sockets。它可以讓程式檢查某些 file descriptors 是否已經可以讀或寫。

Socket 是網路通訊端點。
File descriptor 是 UNIX / Linux 中代表已開啟檔案、socket、pipe、device 的整數識別碼。

select() 可以設定 timeout：
```
timeout = 0
→ 立刻檢查，不等待
→ 類似 polling，但不會 blocking

timeout > 0
→ 最多等待指定時間

timeout = 無限
→ 一直等到某個 I/O ready
```
重點是：select() 本身只告訴程式「哪些 I/O 目前可做」，它不真正搬資料。真正資料傳輸仍然要再呼叫 read() 或 write()。

課本也提到 Mac 的 blocking multiple-read call，概念是一次指定多個 devices，只要其中一個完成就返回。
### Blocking、Nonblocking、Asynchronous 比較
Blocking I/O：
呼叫後等待 I/O 完成，thread 進 wait queue。
優點是程式簡單，缺點是等待期間不能做其他事。

Nonblocking I/O：
呼叫後立刻返回，只處理目前可用資料。
優點是不會卡住，缺點是程式要自己反覆檢查與處理部分結果。

Asynchronous I/O：
呼叫後立刻返回，完整 I/O 在背景完成，完成後通知程式。
優點是 I/O 和 computation 可以重疊，缺點是程式模型更複雜。

Blocking I/O 會讓 calling thread 等待 I/O 完成，適合簡單程式。Nonblocking I/O 不等待完整 I/O，只立即回傳目前可用結果，適合互動式程式或事件驅動程式。Asynchronous I/O 則是提交完整 I/O request 後立刻返回，I/O 在背景完成，完成後透過 signal、callback 或其他方式通知。圖 12.9 的差異就在於：同步 I/O 的 requesting process 會等待硬體傳輸完成；非同步 I/O 則讓 process 先繼續執行，硬體傳輸和程式執行可以重疊。
### hardware data transfer
這裡的 hardware data transfer 指的是：資料真正被硬體搬動的那段 I/O 傳輸過程。

也就是說，前面的 read() / write() system call 只是「發出要求」，device driver 只是「設定控制器」，而 hardware data transfer 才是資料真的從裝置搬到記憶體，或從記憶體搬到裝置。

以 disk read 為例：
```
程式呼叫 read()
↓
OS / device driver 要求 disk controller 讀資料
↓
disk controller 開始從 disk 讀 block
↓
資料透過 DMA 搬到 memory buffer
↓
完成後 interrupt 通知 CPU
```
其中這一段：
```
disk controller 從 disk 讀資料
→ DMA 把資料放進 memory buffer
```
就是圖上的 hardware data transfer。
## 12.3.5 Vectored I/O
# 12.4 Kernel I/O Subsystem
## 開頭
這段在講 **Kernel I/O Subsystem（核心 I/O 子系統）** 負責提供一堆跟 I/O 有關的共同服務。前面 12.2、12.3 主要在看硬體與應用程式介面；到了 12.4，重點變成：**kernel 內部要怎麼管理大量 I/O requests，讓整個系統有效率又安全**。

Kernel I/O subsystem 提供的服務包含：
```
scheduling
buffering
caching
spooling
device reservation
error handling
```
這些服務都建立在前面提過的 **hardware（硬體）** 和 **device driver（裝置驅動程式）** 之上。也就是說，driver 負責跟具體裝置溝通，而 kernel I/O subsystem 負責提供更高層、共用的管理機制。

另外，課本也提到 I/O subsystem 要保護自己，避免錯誤 process 或惡意使用者亂操作裝置。例如不允許一般 process 直接亂寫 disk controller，或搶走不該使用的 device。
## 12.4.1 I/O Scheduling
**I/O Scheduling** 的意思是：當很多 I/O requests 同時出現時，OS 不一定照 application 發出 request 的順序執行，而是會重新安排順序，讓整體效率更好。

這跟 CPU scheduling 很像。CPU scheduling 是決定哪個 process 先用 CPU；I/O scheduling 則是決定哪個 I/O request 先被 device 處理。
> To schedule a set of I/O requests means to determine a good order in which to execute them.

也就是說，I/O scheduling 的目標不是單純「先來先服務」，而是找到一個比較好的執行順序。
### 為什麼 I/O request 不一定照原順序？
Application 發出 system call 的順序，不一定是 device 最有效率的服務順序。

例如有三個 application 都對同一顆 disk 發出 blocking read：
```
Application 1：要求讀 disk 尾端附近的 block
Application 2：要求讀 disk 開頭附近的 block
Application 3：要求讀 disk 中間附近的 block
```
如果照 1 → 2 → 3 處理，disk arm 可能要從中間跑到尾端，再跑回開頭，再跑到中間，移動距離很大。

但如果 OS 改成：
```
2 → 3 → 1
```
disk arm 可能從開頭往中間，再往尾端走，總移動距離比較短。

這就是 I/O scheduling 的核心精神：**重新排列 request order，減少等待時間、提升 throughput、增加公平性**。
### I/O Scheduling 的主要目標
第一個目標是 **improve overall system efficiency（提升整體效率）**。例如磁碟排程可以減少 seek distance，讓 disk head 不用亂跑。

第二個目標是 **reduce average response time（降低平均回應時間）**。Request 等越短，application 感覺越快。

第三個目標是 **fairness（公平性）**。不能讓某些 application 永遠等不到 I/O。

第四個目標是 **priority service（優先服務）**。某些 delay-sensitive requests 可以有比較高優先權。例如 virtual memory subsystem 的 page-in / page-out request 可能比一般 application read 更急，因為它影響 process 能不能繼續跑。

**Virtual memory subsystem（虛擬記憶體子系統）** 是 OS 管理 virtual memory、page fault、page replacement、swap 等機制的部分。
### OS 怎麼實作 I/O Scheduling？
OS 通常會替每個 device 維護一個 **wait queue（等待佇列）**。

當 application 發出 blocking I/O system call 時，如果 device 正忙，request 就會被放進該 device 的 queue 裡。之後 **I/O scheduler** 會重新安排 queue 中 requests 的順序。

概念如下：
```
Application 發出 I/O request
↓
Kernel 檢查 device 狀態
↓
如果 device idle：可以直接開始處理
↓
如果 device busy：request 放入該 device 的 wait queue
↓
I/O scheduler 排序 requests
↓
Device 依排好的順序處理
```
這裡的 **blocking I/O** 表示呼叫 I/O 的 thread 會進入 wait queue，等 I/O 完成後才回到 run queue。
### Figure 12.10：Device-status table
![image](https://hackmd.io/_uploads/SyELPpjbMe.png)
這張圖是在說 kernel 會維護一張 **device-status table（裝置狀態表）**。它用來追蹤每個 I/O device 目前的狀態，以及該裝置有哪些 pending requests 還沒處理。

圖左邊是一張 table，每一列代表一個 device：
```
device: keyboard       status: idle
device: laser printer  status: busy
device: mouse          status: idle
device: disk unit 1    status: idle
device: disk unit 2    status: busy
...
```
**idle** 表示裝置目前空閒，可以接受新的 I/O request。
**busy** 表示裝置正在處理某個 request，其他 request 需要排隊。
### Device-status table 的作用
Device-status table 至少記錄三類資訊：
```
1. Device type
   例如 keyboard、printer、disk

2. Device address
   OS 用來找到或識別該裝置

3. Device state
   例如 idle、busy、error

4. Pending requests
   如果 device 正忙，還會記錄等待中的 I/O requests
```
這樣 kernel 才知道：

哪個裝置可以立刻服務。
哪個裝置正在忙。
哪個裝置的 queue 裡還有 request。
I/O 完成後下一個 request 要處理誰。
## 12.4.2 Buffering
**Buffer（緩衝區）** 是一塊 memory area，用來暫存正在傳輸中的資料。它通常放在 main memory 裡，位於兩個速度不同或資料格式不同的端點之間，例如：
```
I/O device ↔ buffer ↔ application
network card ↔ buffer ↔ SSD
keyboard controller ↔ buffer ↔ process
```
Buffering 的核心目的不是「永久保存資料」，而是讓資料傳輸比較順，不要因為兩邊速度、資料大小、時機不同而卡住。

**Buffering 是 kernel I/O subsystem 用 memory 暫存資料的技術，用來吸收速度差、調整資料大小、保證 write() 等 I/O 操作的語意正確。它讓 I/O device、kernel、application 之間不必完全同步，因此能提升效率與可靠性。**
### 一、原因 1：處理速度不一致 Speed Mismatch
第一個用途是處理 **producer（生產者）** 和 **consumer（消費者）** 速度不一致的問題。

例如課本舉的例子：從 Internet 接收檔案，然後存到 SSD。

Network 可能慢很多，資料一點一點進來；SSD 寫入速度可能很快，但不適合每收到幾個 bytes 就立刻寫一次。這時 OS 會先在 memory 中建立 buffer，把 network 收到的 bytes 累積起來。等 buffer 滿了，再一次寫入 SSD。

流程像這樣：
```
Network 慢慢收到資料
↓
先放進 memory buffer
↓
Buffer 滿了
↓
一次寫入 SSD
```
這樣做的好處是：network 可以照自己的速度慢慢送，SSD 也可以用較有效率的大塊寫入，不必每來一點資料就被打擾。
### 二、Double Buffering：雙緩衝
課本接著提到 **double buffering（雙緩衝）**。

如果只有一個 buffer，當 buffer 滿了拿去寫 SSD 時，network 可能還有新資料進來，但原本 buffer 正在被使用，於是可能卡住。

所以可以準備兩個 buffers：
```
Buffer A：正在寫入 SSD
Buffer B：正在接收 network 資料
```
等 Buffer B 滿了，Buffer A 也可能已經寫完。接著交換角色：
```
Buffer B：拿去寫入 SSD
Buffer A：重新拿來接收 network 資料
```
這樣 producer 和 consumer 就被解耦了。Network 不需要等 SSD 寫完才繼續收資料；SSD 也不需要等 network 慢慢塞滿才工作。

簡單圖：
```
時間 1：
Network → Buffer A
Buffer B 空著

時間 2：
Buffer A → SSD
Network → Buffer B

時間 3：
Buffer B → SSD
Network → Buffer A
```
這就是課本說的：

**double buffering decouples the producer of data from its consumer**

意思是雙緩衝讓資料生產端和消費端不用完全同步。
### 四、原因 2：處理不同資料傳輸大小
第二個 buffering 用途是處理 **data-transfer size mismatch（資料傳輸大小不一致）**。

最典型例子是網路傳輸。

一個很大的 message 不能直接整包丟到網路上，通常會被切成很多小 packets：
```
Large message
↓ fragmentation
packet 1, packet 2, packet 3, ...
```
送到接收端後，接收端需要把這些 packets 暫存在 buffer 裡，然後重新組回原本的 message。這個動作叫 reassembly（重組）。

所以 buffer 在這裡的功能是：暫存片段資料，等片段收齊後重新組成完整資料。

概念如下：
```
Sender：
large message → fragments / packets

Network：
packet 1, packet 2, packet 3 分開傳

Receiver：
packets → reassembly buffer → original message
```
### 五、原因 3：支援 Copy Semantics
第三個用途是支援 **copy semantics（複製語意）**。

這個概念比較抽象，但非常重要。它是在問：

**當 application 呼叫 write() 後，真正寫到 disk 的資料應該是哪個版本？**

假設 application 有一個 buffer：
```
application buffer = "ABC"
```
它呼叫：
```
write(fd, buffer, 3)
```
照 copy semantics，OS 必須保證：寫到 disk 的資料是呼叫 `write()` 當下 buffer 裡的 "ABC"。

問題是，`write()` system call 回傳後，application 可能立刻改掉自己的 buffer：
```
application buffer = "XYZ"
```
如果 OS 還沒真的把資料寫到 disk，而只是晚點再從 application buffer 取資料，那就會出錯，因為 disk 可能最後寫到 "XYZ"，不是原本呼叫 `write()` 時的 "ABC"。

所以 OS 常見做法是：
```
write() 被呼叫
↓
OS 先把 application buffer 複製到 kernel buffer
↓
write() 回傳給 application
↓
application 之後怎麼改自己的 buffer 都不影響
↓
kernel 之後再慢慢把 kernel buffer 寫到 disk
```
這樣就能保證 copy semantics。
### 六、Copy Semantics 的代價與最佳化
Copy semantics 的缺點是需要多一次資料複製：

application buffer → kernel buffer → device

這會增加 CPU 和 memory bandwidth 的負擔。

課本提到，現代 OS 常用更聰明的方法降低成本，例如：

**virtual memory mapping（虛擬記憶體映射）**
讓 kernel 和 application 透過 page mapping 管理資料，不一定真的逐 byte 複製。

**copy-on-write page protection（寫入時複製頁面保護）**
先讓 kernel 和 application 暫時共享同一份 page，但把它設成特殊保護狀態。若 application 在 I/O 完成前想修改那頁，才真的複製一份。這樣如果 application 沒修改，就省掉實際 copy。

這跟前面 Chapter 10 的 **Copy-on-Write**, COW（寫入時複製） 是同一個精神：先共享，真的要改才複製。
## 12.4.3 Caching
Cache（快取） 是一塊比較快的 memory，用來保存某份資料的「副本」。之後如果又要用同一份資料，就可以直接從 cache 拿，不用重新去比較慢的原始位置讀。

例如一個程式正在執行，它的 instructions 原本存在 disk，執行時會先載入 physical memory，接著又可能被複製到 CPU = Central Processing Unit（中央處理器） 的 cache 裡。越靠近 CPU 的 cache，存取越快。
### Buffer 和 Cache 的差別
**Buffer** 主要是暫存「正在傳輸中的資料」，它可能是資料目前唯一存在的位置。
例如 application 呼叫 `write()` 後，OS 把資料先放在 kernel buffer，晚點才寫到 disk。這時 buffer 裡的資料可能是唯一最新版本。

**Cache** 則是保存「**某份資料的副本**」。原始資料還在別的地方，例如 disk 上，只是 memory 裡放了一份比較快的 copy。

簡單分法：
```
Buffer：
重點是協調資料傳輸。
資料可能只有 buffer 裡有。

Cache：
重點是加速重複存取。
資料是某個原始資料的副本。
```
### I/O 裡的 cache 怎麼用？
在 I/O subsystem 裡，OS 常用 main memory 當作 **disk cache / buffer cache / page cache**。

當 application 要讀檔案時，kernel 會先檢查這段檔案資料是不是已經在 memory cache 裡。

如果有，稱為 **cache hit**，就可以直接從 memory 回傳，不用真的讀 disk。
如果沒有，稱為 **cache miss**，kernel 才需要發出 disk I/O，把資料從 disk 讀進 memory。

流程大概是：
```
Application read file
↓
Kernel 檢查 cache
↓
有資料：直接從 memory 回傳
↓
沒有資料：從 disk 讀進 memory，再回傳
```
這樣對常被重複讀取的檔案非常有效，例如 shared libraries、常用設定檔、常被多個 process 存取的資料。
### Delayed Write：延遲寫入
課本也提到 disk write 會先累積在 cache 裡幾秒鐘，再一起寫到 disk。這叫 delayed write（延遲寫入） 或常見說法 write-back caching。

原因是如果每次 `write()` 都立刻寫 disk，I/O 次數會很多，效率很差。OS 可以先把資料放在 memory cache，之後再合併成較大的 I/O 寫入 disk。

好處是：
```
減少小量寫入次數
合併多個寫入
讓 disk scheduling 更有效率
提升整體 I/O 效能
```
但代價是：如果資料還在 cache 裡、尚未真正寫到 disk，系統突然斷電或 crash，就可能遺失尚未 flush 的資料。所以需要 `flush`、`fsync()` 這類機制要求資料真的寫回 storage。

**Caching 是用較快的 memory 保存資料副本，以避免重複存取較慢的原始位置。Buffer 偏向處理資料傳輸過程，cache 偏向加速重複存取。Kernel I/O subsystem 常用 main memory 作為 disk cache，先檢查資料是否已經在 memory 中，若有就避免 disk I/O；寫入時也可能先累積在 cache 中，再延遲寫回 disk，以提升 I/O 效率。**
## 12.4.4 Spooling and Device Reservation
## 12.4.5 Error Handling：錯誤處理
這段重點是：**I/O 操作很容易失敗，但 OS 不能因為一個小錯就整台系統掛掉**。
有 protected memory 的 OS 可以把很多錯誤限制在某個 process 或某個 I/O request 裡，不讓它擴散成整個 system failure。

I/O 錯誤大致分兩類。第一種是 **transient failure（暫時性錯誤）**，例如網路瞬間太忙、packet 掉了、disk read 偶發失敗。這種通常可以 retry，例如 disk read 失敗就再讀一次，network send 失敗就 resend。第二種是 **permanent failure（永久性錯誤）**，例如 disk controller 壞掉、硬體真的故障。這種 OS 通常很難完全恢復，只能回報錯誤或停止相關操作。

一般 I/O system call 會回傳成功或失敗。在 UNIX / Linux 中，如果 system call 失敗，會透過 errno 回報錯誤原因。
**errno = error number**，也就是錯誤代碼。例如參數錯誤、bad pointer、file not open 等。

課本也提到 SCSI 裝置可以回報更詳細的錯誤資訊。
**SCSI = Small Computer System Interface**，小型電腦系統介面。

SCSI 錯誤資訊有三層：
```
sense key：
大方向錯誤類型，例如 hardware error、illegal request

additional sense code：
更細的錯誤分類，例如 bad command parameter

additional sense-code qualifier：
更進一步補充細節，例如哪個 command parameter 錯
```
這頁核心：OS 要盡量從可恢復的 I/O 錯誤中補救，例如 retry / resend；不可恢復的錯誤則要回報給 application 或記錄系統錯誤。

## 12.4.6 I/O Protection：I/O 保護
![image](https://hackmd.io/_uploads/HkiNIEn-Mx.png)
這段重點是：不能讓 user process 直接亂碰 I/O device。
如果一般程式可以直接下 I/O instruction、直接改 device register、直接寫 disk controller，那系統安全性會爆炸，甚至可能破壞資料或讓 OS 失控。

因此 OS 會把所有 I/O instructions 定義成 privileged instructions（特權指令）。
只有 kernel mode 可以執行，一般 user mode process 不能直接執行。

正常流程是：
```
User process
↓
system call
↓
kernel 檢查請求是否合法
↓
kernel / device driver 執行 I/O
↓
結果回傳給 user process

也就是說，application 不能直接操作硬體，必須透過 OS 提供的 system call，例如 read()、write()、ioctl()。
```
此外，memory-mapped I/O 和 I/O port memory locations 也必須受到 memory protection 保護。
因為某些 device register 或 device memory 會被映射到 address space，如果 user process 能隨便讀寫，就等於能直接控制硬體。

但課本也提醒：kernel 不能完全禁止所有 user access。像 graphics games、video editing、playback software 這類程式，可能需要直接存取一部分 graphics memory 來提升效能。因此 OS 會用受控方式，把某一段 graphics memory 分配給特定 process 使用。

kernel 會執行真正的 I/O 流程，可能包含：
```
檢查 file descriptor 是否有效
檢查 buffer address 是否合法
檢查權限
查 file system
找出資料所在 block
呼叫 device driver
啟動 disk controller / DMA
等待 interrupt 回報完成
```
如果資料已經在 cache 裡，可能不需要真的碰 disk；如果資料不在 memory，才需要進行實際硬體 I/O。

這頁核心：I/O protection 的目的，是讓 user process 不能直接執行危險 I/O 操作；所有 I/O 必須經過 kernel 檢查與 device driver 執行。必要時，OS 可以安全地開放部分 device memory 給 application 使用。
## 12.4.7 Kernel Data Structures
### Concept
核心是在說：**kernel 為了管理 I/O，必須在核心記憶體中保存很多狀態資料**。
因為 I/O 不只是「呼叫 read/write 就結束」，kernel 必須知道每個檔案、socket、device、I/O request 現在的狀態、對應哪個 driver、要呼叫哪個處理函式。
### 1. Kernel 為什麼需要 I/O data structures？
Kernel data structures 就是 kernel 內部用來記錄系統狀態的資料結構。
在 I/O 系統中，它要記錄：
```
哪些檔案被打開
每個 process 的 file descriptor 對應到哪個 open file
每個 open file 對應到哪個 inode / socket / device
read(), write(), ioctl(), close() 要呼叫哪個實作函式
network connection 狀態
character device communication 狀態
其他 pending I/O activity
```
簡單說，application 只看到一個 `fd`，但 kernel 背後要一路查很多表，才能知道這個 `fd` 到底代表一般檔案、磁碟裝置、鍵盤、socket，還是其他 I/O object。

### 2. UNIX 把很多東西都包成「像檔案一樣」
UNIX / Linux 有一個很重要的設計精神：many things are files。
也就是很多不同實體都可以用類似 file-system access 的方式操作，例如：
```
一般 user file
raw device
process address space
socket
character device
block device
```
雖然它們都可能支援 `read()`，但背後語意不一樣。

例如讀一般檔案時，kernel 可能先查 buffer cache / page cache，如果資料已經在記憶體中，就不用真的讀 disk。

讀 raw disk 時，request size 可能必須是 disk sector size 的倍數，而且 address 要對齊 sector boundary。

讀 process image 時，可能只是從 memory copy 資料，不需要 disk I/O。

所以同樣是 `read()`，背後要做的事情依物件類型不同而不同。這就是 kernel 需要 dispatch table 的原因。

### 3. Dispatch table 是什麼？
**Dispatch table（分派表）** 是一張 function pointer table。
它裡面放很多函式指標，用來決定某個 I/O object 的操作要呼叫哪個實作。

例如某個 open-file record 可能有：
```
read function pointer
write function pointer
select function pointer
ioctl function pointer
close function pointer
```
當 application 呼叫：
```
read(fd, buffer, size);
```
kernel 不會只執行唯一一個固定的 read。它會先查 `fd` 對應到哪個 open-file record，然後根據 record 裡的 function pointer 呼叫正確的 read implementation。

例如：
```
fd 對應一般檔案
→ 呼叫 file-system read

fd 對應 socket
→ 呼叫 socket read

fd 對應 keyboard
→ 呼叫 character-device read

fd 對應 raw disk
→ 呼叫 block-device / raw-device read
```
這有點像物件導向裡的 polymorphism（多型）：同樣叫 `read()`，但不同物件有不同實作。
### 4. Figure 12.13：UNIX I/O kernel structure 圖解
![image](https://hackmd.io/_uploads/rkf3D4nWfx.png)
這張圖是在畫 UNIX 裡面一個 I/O request 如何從 user process 的 `file descriptor` 一路找到 kernel 裡真正的 I/O object。

左邊是 **user-process memory**。裡面有：
```
file descriptor
```
**File descriptor** 是一個整數，例如 `0, 1, 2, 3...`。
在 UNIX / Linux 中：
```
0 = standard input
1 = standard output
2 = standard error
3 以後通常是 open() 打開的檔案、socket、device 等
```
File descriptor 本身不是檔案，它只是 process 用來索引 open-file table 的編號。


### 5. Per-process open-file table
每個 process 都有自己的：
```
per-process open-file table
```
也就是「每個 process 私有的開檔表」。

例如 process A 的 fd = 3 和 process B 的 fd = 3 不一定代表同一個檔案。因為每個 process 的 file descriptor table 是獨立的。

流程是：
```
file descriptor
→ per-process open-file table
→ system-wide open-file table
```
### 6. System-wide open-file table
中間的大框是 kernel memory 裡的：
```
system-wide open-file table
```
這是整個系統共用的 open-file records。
它記錄實際被打開的 I/O object 狀態，例如 file offset、open mode、reference count、對應的底層 object 等。

如果兩個 process 開到同一個檔案，kernel 可能讓它們透過不同 fd 指向相同或相關的 open-file record。這樣 kernel 才能管理共享開檔狀態。
### 7. File-system record 與 active-inode table
圖中上半部是：
```
file-system record
```
這代表一般檔案系統物件。裡面有：
```
inode pointer
pointer to read and write functions
pointer to select function
pointer to ioctl function
pointer to close function
```
inode 是 UNIX / Linux file system 中描述檔案的核心資料結構。
它記錄檔案的 metadata，例如：
```
檔案大小
權限
擁有者
時間資訊
資料 block 位置
```
所以 inode pointer 會指向右邊的：
```
active-inode table
```
Active-inode table 是 kernel 目前正在使用的 inode 集合。只要某個檔案正在被開啟或使用，對應 inode 可能就會被放在 active-inode table 裡。
### 8. Networking socket record 與 network-information table

圖中下半部是：

networking (socket) record

這代表 fd 也可能不是一般檔案，而是一個 socket。

Socket 是網路通訊端點，例如 TCP connection、UDP socket。
所以 socket record 裡面不是 inode pointer，而是：
```
pointer to network info
pointer to read and write functions
pointer to select function
pointer to ioctl function
pointer to close function
```
右邊的：
```
network-information table
```
則記錄網路連線相關資訊，例如連線狀態、遠端位址、port number、protocol 狀態等。

這說明 UNIX 的抽象很漂亮：一般檔案和 socket 都能用 fd 表示，也都能呼叫 read/write/select/ioctl/close，但背後 dispatch 到不同資料結構與函式。

### 9. ioctl 和 select 在這裡的角色

ioctl = input/output control，I/O 控制。
它用來執行裝置或 I/O object 的特殊控制命令。一般 read() / write() 不夠表達的操作，就可能走 ioctl()。

select 不是縮寫。
它用來檢查某個 fd 是否 ready，例如是否可讀、可寫、是否有事件發生。常用於 socket、terminal、pipe 等 I/O multiplexing 場景。

### 10. Windows 的 message-passing model

課本最後提到，有些 OS 用更物件導向或 message-passing 的方法管理 I/O，例如 Windows。

Message passing（訊息傳遞） 的意思是：I/O request 會被包成一個 message，沿著 kernel I/O manager、device driver 等層級傳下去。每一層可以讀取或修改 message 內容。

例如 output request 的 message 會包含要寫出的資料；input request 的 message 會包含接收資料的 buffer。

這種方法的好處是架構彈性高、分層清楚；缺點是比直接共享資料結構的方式多一些 overhead。

### 這節重點整理

12.4.7 的重點是：**kernel 需要許多內部資料結構來追蹤 I/O 狀態。UNIX / Linux 用 file descriptor、per-process open-file table、system-wide open-file table、file-system record、socket record、inode table、network-information table 等結構，把不同 I/O object 統一包裝起來。**

最重要的觀念是：
```
同一個 system call，例如 read()
在 kernel 裡會依照 fd 對應的 object 類型
透過 dispatch table 呼叫不同的 read implementation
```
所以一般檔案、socket、raw device、character device 都能用類似介面操作，但底層處理邏輯完全不同。這就是 kernel I/O subsystem 用資料結構實現抽象化與多型的方式。
## 12.4.8 Power Management
![image](https://hackmd.io/_uploads/S1jAnE2bfl.png)
![image](https://hackmd.io/_uploads/B1beT4h-fl.png)

# 12.5 Transforming I/O Requests to Hardware Operations
## Concept
這節在回答一個很實際的問題：application 只是呼叫 `read()`、指定檔名或 file descriptor，最後 OS 到底怎麼把它變成「讀某顆磁碟的某個 sector」或「對某個 controller register 下命令」？

換句話說，這節是在把前面所有東西串起來：
```
Application request
→ System call
→ File system / kernel I/O subsystem
→ Device driver
→ Device controller
→ Actual hardware operation
```
## 1. 從檔名到硬體位置
OS 不是把「檔名」直接轉成一包資訊丟給硬體。
比較精準是：OS 會把 application 的高階 I/O request，一層一層轉換成硬體看得懂的低階操作。

例如讀檔：
```
read("/home/a.txt")
```
硬體完全不懂 /home/a.txt，硬體只懂：
```
哪個 device
哪個 block / sector
讀多少 bytes
資料放到哪個 memory buffer
開始執行
```
所以 OS 內部大概會做：
```
檔名 / pathname
→ 查 directory
→ 找到檔案的 inode / metadata
→ 知道檔案資料在哪些 disk blocks
→ 根據 mount table 知道是哪個 device
→ 根據 major/minor number 找到 device driver
→ device driver 轉成 controller command
→ controller / DMA 執行真正硬體傳輸
```
也就是說，OS 會先透過 file system 把「檔名」轉成「檔案 metadata 與 block 位置」，再透過 I/O subsystem / device driver 把「block 讀寫」轉成「硬體控制命令」。

可以想成三層翻譯：
```
Application 層：
我要讀 /home/a.txt

File system 層：
這個檔案對應到 disk 上的 block 100, 101, 102...

Device driver 層：
命令 disk controller 從那些 blocks 讀資料，放到 memory buffer

Hardware 層：
controller / DMA 實際搬資料
```
所以一句話答案是：

**是，OS 負責把檔名這種高階抽象，經由 file system、mount table、inode、device driver 等結構，逐步轉成硬體能執行的 I/O 操作；硬體本身不認得檔名，只認得裝置、位址、長度與命令。**
## 2. MS-DOS / FAT 的做法
**MS-DOS = Microsoft Disk Operating System。
FAT = File Allocation Table，檔案配置表。**

課本先用 MS-DOS for FAT 當例子。它的做法比較簡單，檔名前面會有一個裝置名稱或磁碟代號，例如：
```
C:\folder\file.txt
```
這裡的 `C:` 就代表某個特定裝置，例如 primary hard disk。OS 看到 `C:`，就可以查 device table，知道這個裝置對應到哪個 hardware port address 或 controller。

這種做法的特色是：**device name 和 file path 明顯分開**。
```
C:        → device portion
\xxx\yyy → file-system path
```
好處是簡單。缺點是裝置名稱和一般檔案名稱空間分離，比較不統一。
## 3. UNIX 的做法：把 device 放進一般 file-system namespace
UNIX 的做法比較統一。
UNIX 不是現代標準縮寫，歷史上源自 Unics / Uniplexed Information and Computing Service，但現在通常直接當系統家族名稱使用。

UNIX 不像 MS-DOS 那樣用 C:、D: 明顯標示裝置。UNIX 會把 device name 放進一般 file-system namespace 裡，例如：
```
/dev/sda
/dev/sda1
/dev/tty
/dev/null
```
也就是說，在 UNIX 裡，device 看起來也像一個檔案。這就是前面提過的精神：many things are files。
## 4. Mount table：把 path 對應到 device
UNIX 會用 mount table（掛載表） 來判斷某個 path 屬於哪個裝置。

例如：
```
/              → root filesystem on disk A
/home          → filesystem on disk B
/mnt/usb       → filesystem on USB device
```
當 OS 收到一個 pathname，例如：
```
/home/user/a.txt
```
它會去 mount table 找「最長符合的 prefix」。如果 /home 是一個 mount point，那 OS 就知道這個檔案其實位於掛載到 /home 的那個 device 上。

流程大概是：
```
pathname
→ mount table lookup
→ 找到對應 mounted device
→ 查 device number
→ 找到 device driver
```
## 5. Major / Minor device number
UNIX / Linux 常用 major number 和 minor number 識別裝置。

Major number（主裝置號）：用來識別應該交給哪個 device driver。
Minor number（次裝置號）：用來識別該 driver 管理下的哪一個具體裝置或分割區。

例如：
```
/dev/sda   major 8, minor 0
/dev/sda1  major 8, minor 1
/dev/sda2  major 8, minor 2
```
概念上是：
```
major number
→ 找到 disk driver

minor number
→ 找到第幾顆 disk / 第幾個 partition
```
接著 driver 會查自己的 device table，找到對應 controller 的 I/O port address 或 memory-mapped control-register address。
## 6. Modern OS：動態偵測與載入 driver
現代 OS 不一定在開機時就把所有 driver 全部固定好。系統可以在 boot time 掃描硬體 bus，找出目前有哪些 devices，然後載入需要的 drivers。

甚至有些 device 是後來才插上的，例如 USB device。OS 可能在裝置插入後偵測到新硬體，再動態載入對應 driver。

這裡的重點是：OS 透過多層 lookup table，把 application request 逐步連到真正的 physical device controller。
## Figure 12.14：The life cycle of an I/O request
![image](https://hackmd.io/_uploads/rysQWH2Wze.png)
### Step 1：Process 發出 blocking read()
Process 呼叫：
```
read(fd, buffer, size)
```
其中 `fd` 是 **file descriptor（檔案描述符）**，代表先前已經 open 的檔案或 device。

這個呼叫會透過 **system call（系統呼叫）** 進入 kernel。
### Step 2：Kernel 檢查參數，並嘗試直接滿足 request
Kernel 會先檢查參數是否合法，例如：
```
file descriptor 是否有效
buffer address 是否合法
是否有讀取權限
要求大小是否合理
```
接著 kernel 會問：這個 I/O request 可不可以立刻完成？

例如資料已經在 buffer cache / page cache 裡，就不用真的讀 disk。
這時 kernel 直接把 cache 裡的資料複製到 process address space，然後 system call 可以結束。
### Step 3：如果不能直接完成，就排入 device queue
如果資料不在 cache，必須真的做 physical I/O，process 會被移出 run queue，放進該 device 的 wait queue。

**Run queue：** 等待 CPU 排程執行的 process / thread queue。
**Wait queue：** 等待某事件完成的 process / thread queue，例如等待 disk I/O 完成。

這時 I/O request 會被排程，交給對應的 device driver。
### Step 4：Device driver 準備 kernel buffer，排程 I/O
**Device driver（裝置驅動程式）** 會配置 kernel buffer 或設定傳輸位置，準備接收資料。

接著 driver 會把這個 request 排入裝置的 I/O queue，並在適當時機送命令給 device controller。
### Step 5：Device driver 對 controller 下命令
Driver 會寫入 device controller 的 control registers，例如：
```
要讀還是寫
資料位置
資料長度
目標 block / sector
DMA buffer address
開始執行
```
**Device controller（裝置控制器）** 收到命令後，開始操作硬體。

如果使用 **DMA = Direct Memory Access（直接記憶體存取）**，driver 也會設定 DMA controller，讓資料可以直接在 device 和 memory 之間搬移。
### Step 6：Controller 執行資料傳輸，完成後產生 interrupt
Device controller 開始真正的硬體操作，例如 disk 讀取資料、network card 接收資料。

資料傳輸可以透過：
```
polling
interrupt-driven I/O
DMA transfer
```
課本這裡假設傳輸由 DMA controller 管理。當傳輸完成時，controller 會產生 interrupt（中斷） 通知 CPU。
### Step 7：Interrupt handler 收到中斷
CPU 收到 interrupt 後，會透過 interrupt vector 找到正確的 interrupt handler（中斷處理程式）。

Interrupt handler 會做必要處理，例如：
```
確認是哪個 device 完成 I/O
讀取 controller status
儲存必要資料
清除 interrupt
通知 device driver
```
Handler 處理完後返回。
### Step 8：Device driver 得知 I/O 完成
Device driver 收到完成訊號後，會判斷是哪一個 I/O request 完成，並檢查結果：
```
成功？
失敗？
是否需要 retry？
實際傳輸了多少 bytes？
```
若成功，driver 會通知 kernel I/O subsystem：這個 request 已經完成。
### Step 9：Kernel 把資料或結果回傳給 process
Kernel 會把資料複製到 requesting process 的 address space，或回傳 system call 結果。
例如 `read()` 成功時，回傳：
```
實際讀到的 bytes 數
```
若失敗，則回傳錯誤資訊，例如 UNIX / Linux 的 errno。

### Step 10：Process 從 wait queue 回到 ready queue
I/O 完成後，原本 blocked process 被移回 ready queue。
等 scheduler 之後選到它時，process 就能繼續執行。

最後 `read()` system call 回到 user program，看起來就像「`read()` 終於完成並回傳」。
## 這節最重要的完整流程
12.5 的重點是：application 的高階 I/O request 必須經過 OS 多層轉換，最後才會變成硬體操作。 檔名會透過 directory、mount table、inode、major/minor device number 找到真正 device；system call 會透過 kernel I/O subsystem、device driver、device controller、interrupt handler 完成 I/O。一次簡單的 read() 背後可能包含 cache lookup、queue scheduling、DMA setup、hardware transfer、interrupt handling、process blocking / waking 等大量步驟。
```
Application 呼叫 read()
→ trap into kernel
→ kernel 檢查參數
→ 查 cache 是否可直接滿足
→ 若不行，建立 I/O request
→ process block，進 wait queue
→ request 交給 device driver
→ driver 設定 controller / DMA
→ controller 執行硬體傳輸
→ I/O 完成產生 interrupt
→ interrupt handler 處理中斷
→ driver 確認 request 完成
→ kernel 回傳資料與狀態
→ process 回 ready queue
→ scheduler 讓 process 繼續跑
```
大方向是：**最後資料會進到 main memory 裡面**，但要小心分清楚兩個層次。

**Requesting process 的 address space 不是一塊實體 RAM 本身，而是 process 看到的 virtual address space。**
課本前面有講，process 的 virtual address space 是「邏輯上」看起來連續的記憶體，但實際上會由 **MMU = Memory Management Unit（記憶體管理單元）** 對應到 physical memory 的 page frames。也就是說，address space 是虛擬位址觀點，真正放資料的是 main memory 裡的 physical frames。

所以「kernel 把資料複製到 requesting process 的 address space」比較精準地說是：

```text
kernel 把資料複製到 user process 指定的 user buffer
↓
這個 user buffer 是 virtual address
↓
MMU / page table 把 virtual address 對應到 main memory 的 physical frame
↓
資料實際寫入 RAM 裡面的某些 frame
```

以 `read(fd, buffer, size)` 來看，`buffer` 是 application 傳給 kernel 的 **user-space virtual address**。Kernel 會檢查這個 address 合不合法、可不可以寫。如果合法，kernel 才會把讀到的資料 copy 到這個 buffer 對應的 physical memory 裡。

但 I/O 過程中常常會先經過 **kernel memory**。例如 DMA 讀磁碟時，課本提到最直接的做法是讓 DMA 的目標位址在 **kernel address space**，完成後若要讓 user thread 存取，還需要再從 **kernel memory copy 到 user memory**。這也是所謂 double buffering 的成本；後來有些 OS 會用 memory mapping 讓資料更直接地進入 user address space。

所以完整圖像大概是：

```text
Disk / device
↓
kernel buffer / page cache        ← 在 main memory 裡
↓ copy_to_user
user process buffer               ← 也是 main memory 裡，但屬於 user address space 對應的 frames
```

一句話：**是，資料實際會在 main memory 裡；但 process 看到的是 virtual address space，kernel 是把資料寫進該 virtual address 對應到的 physical frames。**

# 12.6 STREAMS
## Concept
它是 UNIX System V 裡的一種 **I/O framework（輸入/輸出框架）**，用來把 **user process** 和 **device** 之間的通訊做成一條可組裝、可插入模組的通道。

STREAMS 不是一般縮寫比較明確的那種名詞，通常就叫 UNIX STREAMS。它的概念是建立一條 **full-duplex communication channel（全雙工通訊通道）**。
Full-duplex 的意思是：資料可以同時雙向傳輸。例如一邊從 device 讀資料上來，一邊又把 user process 的資料寫到 device 下去。
## STREAMS 的組成
一個 STREAM 主要由三部分組成：
```
user process
↓
STREAM head
↓
zero or more STREAM modules
↓
driver end
↓
device
````
**STREAM head** 是靠近 user process 的那一端。Application 做 `read()`、`write()` 之類的 system call 時，會先接觸到 stream head。它可以看成 user process 和 kernel STREAMS system 的接口。

**driver end** 是靠近 device 的那一端。它負責和真正的 device driver / device controller 溝通，也就是把上層送來的 request 轉成裝置能處理的操作。

**STREAM modules** 是中間可以插入的模組。這是 STREAMS 的核心特色。中間可以放 0 個或多個 module，每個 module 可以對資料做處理，例如格式轉換、協定處理、資料過濾、編碼解碼等。很像一條資料管線，中間可以插功能方塊。
## read queue 與 write queue
每個 module 裡都有兩個 queue：
```
read queue
write queue
```
**read queue** 負責 device → user process 方向，也就是資料從硬體往上送。例如鍵盤輸入、網路封包收到、serial port 收到資料。

**write queue** 負責 user process → device 方向，也就是資料從 application 往硬體送。例如程式寫資料到 terminal、network device、serial device。

所以第二張圖中，左邊 read queue 的箭頭往上，代表資料從 device 往 user process 流；右邊 write queue 的箭頭往下，代表資料從 user process 往 device 流。
## STREAMS Structure
![image](https://hackmd.io/_uploads/Bk8qEr3bMx.png)
圖的上面是 user process，下面是 device。中間一層一層是 STREAMS 架構。

資料往下寫入時：
```
user process
→ stream head
→ write queue
→ module write queue
→ module write queue
→ driver end
→ device
```
資料往上讀取時：
```
device
→ driver end
→ read queue
→ module read queue
→ module read queue
→ stream head
→ user process
```
這代表 STREAMS 把 I/O 通道切成很多層，每一層都可以處理資料，再把資料往下一層或上一層傳。
## Message passing 與 flow control
**Message passing（訊息傳遞）** 是 STREAMS 內部各 queue 之間溝通的方式。資料不是單純被函式直接傳過去，而是包成 message，在不同 queue 之間傳遞。

**Flow control（流量控制）** 是用來避免 queue 爆掉的機制。
如果某一層處理太慢，queue 快滿了，就可以通知上游：「先不要再送資料」。這樣可以避免資料一直塞進來造成 buffer overflow。
## Asynchronous internally, synchronous with user process
這句是重點。

**內部 asynchronous（非同步）**：STREAMS 內部各 module、queue、driver 之間不一定要同步等待，每層可以透過 message queue 慢慢處理資料。

**對 user process 可能看起來 synchronous（同步）**：user process 呼叫 `read()` 或 `write()` 時，可能會等待結果，好像整個 I/O 是同步完成的。

也就是說，外面看起來像一般 system call，但 kernel 裡面其實用比較彈性的 queue / message passing 架構處理。
# 12.7 Performance
## 1. Performance
I/O 會嚴重影響系統效能，因為每次 I/O 不只是裝置自己在動，還需要 CPU 執行 device driver（裝置驅動程式） 和 kernel I/O code（核心 I/O 程式碼）。例如讀磁碟、收網路封包、印表機輸出，都不是 application 直接碰硬體，而是經過 kernel 和 driver。

另外，I/O 常常會產生 interrupt（中斷），中斷會造成 context switch（內容切換） 或至少讓 CPU 暫停目前工作去執行 interrupt handler。資料也常常要在 user space、kernel space、device buffer 之間複製，這些 data copying（資料複製） 都會消耗 CPU time 和 memory bandwidth。Network traffic 特別重，因為封包多、interrupt 多、copy 多，所以對系統壓力很大。
## 2. Intercomputer Communications
![image](https://hackmd.io/_uploads/S1l8PBhbfg.png)
這張圖是在說兩台電腦透過 network 溝通時，中間其實經過很多層。左邊是 sending system，右邊是 receiving system。

送資料時，user process 先進入 kernel，kernel 交給 device driver，再交給 network adapter。Network adapter 送出封包後可能產生 interrupt，kernel 再處理完成事件，最後 system call 才完成。

收資料時，network packet 到達 network adapter，adapter 產生 interrupt，device driver 和 kernel 會處理封包，最後可能喚醒 network daemon（網路背景服務行程） 或相關 process。

這張圖的重點是：一次網路傳輸會牽涉大量 interrupt、context switch、driver execution、kernel processing，所以 network I/O 很容易成為效能瓶頸。
## 3. Improving Performance
這張是在整理改善 I/O 效能的方法。

可以減少 context switches（內容切換），因為每次切換都要保存/恢復狀態，成本不低。也可以減少 data copying（資料複製），例如使用 zero-copy、memory mapping 或更好的 buffer 設計。

也可以減少 interrupt 數量，例如用較大的 transfer size、smart controller、polling。
Polling（輪詢） 是 CPU 主動檢查裝置狀態；在高頻 I/O 場景，有時比大量 interrupt 更有效。

DMA = Direct Memory Access（直接記憶體存取） 也很重要，因為它讓 device 和 memory 之間直接搬資料，不需要 CPU 一個 byte 一個 byte 搬。

最後一點是平衡 CPU、memory、bus、I/O device 的速度。系統瓶頸可能不在磁碟，也可能在 bus、memory bandwidth、CPU interrupt handling。高效能系統要整體平衡。
## 4. Device-Functionality Progression
![image](https://hackmd.io/_uploads/ryyOvrn-zg.png)
這張圖是在說：隨著硬體與系統世代演進，越來越多功能可以從 application / kernel / driver 往更底層硬體移動。

下面由左到右大概是：
```
device code / hardware
→ device-controller code / hardware
→ device-driver code
→ kernel code
→ application code
```
越往左，功能越靠近硬體，通常效率更高，但開發成本也更高、彈性較低。
越往右，功能越靠近 software / application，彈性較高、比較容易改，但效能可能較差。

圖上的意思是：新演算法或新功能一開始可能放在 application 或 kernel 裡，等技術成熟後，可能逐漸下放到 driver、controller，甚至直接做進 hardware 裡。這樣可以提升效率，但也會提高硬體設計與開發成本。
# 補充
## memory-mapped I/O（記憶體映射式輸入/輸出）
![image](https://hackmd.io/_uploads/HkB5Pr2-Gx.png)
這張圖比較接近在說 **memory-mapped I/O（記憶體映射式輸入/輸出）** 的硬體背景，但**光看這張圖本身，不能百分之百判斷一定是 memory-mapped I/O**；它主要畫的是 **CPU、memory controller、I/O controller、devices 之間怎麼連接**。

不過它**不是 direct I/O**。這裡要先分清楚：

**Memory-mapped I/O** 是 CPU 控制硬體的一種方式：把 device controller 的 registers 映射到某些 memory address。CPU 對那些 address 做 `load/store`，看起來像讀寫記憶體，實際上是在讀寫 I/O 裝置的控制暫存器。

**Direct I/O** 則是 application / database 做檔案 I/O 時，要求 OS 盡量不要經過 file-system buffer cache。它是在講「檔案資料傳輸要不要被 OS cache」，不是這張圖這種硬體連接方式。

---

圖中各元件意思如下：

**CPU = Central Processing Unit，中央處理器。**
負責執行程式指令。CPU 若要讀寫 memory 或控制 I/O device，通常都要透過下方的 controller / bus 架構。

**Memory Controller = 記憶體控制器。**
它負責管理 CPU 對 main memory 的存取，也可能負責協調 CPU、graphics processor、I/O controller 之間的資料路徑。圖中它像是中間樞紐。

**Memory = 主記憶體 / RAM。**
程式、資料、kernel buffer、user buffer 等執行中資料通常都會放在這裡。

**Graphics Processor = 圖形處理器，通常可理解成 GPU = Graphics Processing Unit。**
它會需要大量存取 memory，例如 frame buffer、texture、graphics data。某些 graphics memory 或 graphics controller registers 可以透過 memory-mapped I/O 讓 CPU 存取。

**I/O Controller = Input / Output Controller，輸入/輸出控制器。**
它下面接很多外部裝置或 bus，例如 SATA、PATA、USB、audio、ethernet、PCI。CPU 不會直接控制每個裝置，而是透過 I/O controller 或各裝置 controller 來下命令。

---

下面那些介面全稱：

**SATA = Serial ATA = Serial Advanced Technology Attachment，序列式 ATA。**
常用於硬碟、SSD 連接。

**PATA = Parallel ATA = Parallel Advanced Technology Attachment，並列式 ATA。**
較舊的硬碟連接介面，也常叫 IDE。

**USB = Universal Serial Bus，通用序列匯流排。**
用於鍵盤、滑鼠、隨身碟、外接硬碟等。

**PCI = Peripheral Component Interconnect，周邊元件互連匯流排。**
用來連接擴充裝置。後來更常見的是 PCIe。

**Ethernet = 乙太網路。**
有線網路介面。

---

如果這張圖要拿來解釋 **memory-mapped I/O**，重點會是：CPU 發出的某些 address 不一定真的對應到 RAM。某些 address 可能被 memory controller / I/O controller 判斷為「這是裝置控制暫存器的位址」，於是讀寫動作會被送到 I/O controller 或 graphics processor，而不是送到一般 memory。

概念像這樣：

```text
CPU 執行 store 指令
↓
送出一個 address + data
↓
Memory controller 判斷 address 屬於 I/O mapped range
↓
轉送到 I/O controller / graphics processor
↓
實際改變 device register 或 device buffer
```

所以在 memory-mapped I/O 中，CPU 用一般 memory instruction 讀寫裝置：

```text
一般 RAM address：
CPU read/write → main memory

I/O mapped address：
CPU read/write → device controller register
```

---

一句話總結：

**這張圖是在畫 CPU、memory controller、I/O controller 和各種裝置的硬體連接架構；若放在 memory-mapped I/O 的脈絡下，它表示 CPU 可以透過記憶體位址空間間接控制 I/O controller 或 graphics processor。它不是 direct I/O，因為 direct I/O 是檔案 I/O 繞過 OS buffering/cache 的概念，不是這種硬體位址映射架構。**

## Pentium Architecture
![image](https://hackmd.io/_uploads/ry5CwS2bMl.png)
