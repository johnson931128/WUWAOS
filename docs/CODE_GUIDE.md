# WUWAOS 程式碼與架構紀錄

記錄目前所有手寫程式碼、標頭、連結及建置設定。當前規格由 SPEC.md、SPEC-06 與 SPEC-07 組成；SPEC-07 更新排程政策，SPEC-01／02／03／04 保留舊里程碑，不代表現在的限制。
術語第一次出現附中文用途。原始碼與實際 QEMU 執行是行為依據，尚未執行的路徑不稱為已驗證。

## 1. 架構與控制權

```text
QEMU → OpenSBI（機器模式韌體）
  → kernel/entry.S：準備監督模式核心的執行環境
  → kernel/main.c：安裝例外入口、展開開機字樣、核心映射、PID 1
  → process_start → user_enter → sret
  → user/entry.S → user_main(0) → shell_main
      → 顯示 wuwaos>，以 read 等待輸入
      → kernel/io.c 輪詢 UART，完成一行並喚醒 shell
      → user/shell.c 解析指令
      → user/api.c 的 ecall → kernel/trap.S → trap.c → syscall.c
          ├─ process.c → scheduler.c：PCB 狀態／生命週期 → FIFO 就緒佇列
          └─ io.c：console／pipe 的輸入輸出與阻塞
          └─ fs.c → block.c：檔案路徑／WUFS inode → VirtIO → QEMU 磁碟
      → process_resume 選擇行程，切換頁表
      → trap.S 還原被選中行程 → sret 返回使用者模式
```

CPU 執行記憶體中的機器碼，不知道來源檔名。OpenSBI 負責早期平台準備，核心負責頁表、行程、例外與裝置操作，shell 在 U-mode（使用者模式）透過系統呼叫要求服務。

## 2. 所有程式碼檔案

| 檔案 | 責任 |
| --- | --- |
| kernel/entry.S | 核心初始入口、stack（堆疊）、BSS（零初始化資料） |
| kernel/main.c | 串接核心初始化與第一個行程 |
| kernel/splash.c / splash.h | WUWAOS 文字圖案、中央向兩側展開、開機短暫延遲 |
| kernel/console.c / console.h | UART（序列埠）輪詢、字元／數字輸出、非阻塞字元輸入、核心等待 |
| kernel/trap.S | 切換堆疊、保存／還原暫存器、返回使用者模式 |
| kernel/trap.c / trap.h | 共同例外入口設定、TrapFrame（保存狀態結構）、原因分派 |
| kernel/vm.c / vm.h | 各行程頁表、使用者映像、記憶體權限與安全複製 |
| kernel/process.c / process.h | PCB（行程控制區塊）、生命週期、合作式排程、查詢與終止 |
| kernel/scheduler.c / scheduler.h | 單向鏈結 FIFO 就緒佇列、入列／取隊首／移除與順序查閱 |
| kernel/io.c / io.h | 行輸入、固定管道、pending I/O（尚未完成的輸入輸出） |
| kernel/block.c / block.h | FDT 裝置探索、VirtIO-MMIO 區塊讀寫佇列 |
| kernel/fs.c / fs.h | WUFS 區塊配置、inode、目錄與路徑操作 |
| kernel/memory.c | freestanding（無標準執行環境）建置所需的 byte-wise memset／memcpy |
| kernel/syscall.c / syscall.h | 21 個 syscall（系統呼叫）的編號分派 |
| kernel/user_image.S | 把 shell 的 user.bin 與工作程式 app.bin 嵌入核心的唯讀資料 |
| kernel/linker.ld | 核心入口、記憶體區段與權限邊界 |
| include/syscall_numbers.h | 共用系統呼叫編號、內建程式角色、錯誤碼 |
| include/process_limits.h | 行程與位址空間共用的上限 4 |
| user/entry.S | 使用者入口，將核心給的角色交給 user_main，返回後 exit |
| user/api.c / api.h | 使用者系統呼叫包裝函式 |
| user/shell.c / shell.h | 互動 shell、命令解析、fork／run／管線操作 |
| user/demo.c | shell 映像的 user_main，呼叫 shell_main |
| user/app.c | 獨立 app 映像的 user_main，分派 worker、quick、writer、reader、counter |
| user/print.c / print.h | 兩種映像共用的字串／數字輸出與短寫處理 |
| tools/make_disk.py | 建立／更新 WUFS 磁碟映像中的 kernel.elf，保留其他檔案 |
| user/linker.ld | 使用者映像固定布局與大小限制 |
| Makefile | 交叉編譯、連結、映像嵌入、持久 QEMU 磁碟與啟動 |

AGENTS.md 是協作規則，README.md 是操作說明，docs/ 下為規格與紀錄，.gitignore 排除 build/。標頭定義型別與契約，不是額外可執行程式。沒有新增測試框架或負面測試程式。

## 3. entry.S：為什麼先組合語言，再執行 C？

一般應用程式的 main 前有 runtime（執行環境）準備，自己的核心必須自行準備。
`.section .text.entry` 放置入口機器碼，`.globl _start` 匯出入口符號；kernel/linker.ld 的 ENTRY(_start) 指定核心從這裡開始。

1. `csrw sie, zero` 清除 sie（監督模式中斷啟用暫存器）。
2. `csrci sstatus, 2` 清除 sstatus（監督模式狀態暫存器）的 SIE（監督模式中斷全域開關）。同步例外仍可能發生。
3. `la sp, __stack_top` 設定 sp（堆疊指標）；啟動 stack 向低位址成長，16 KiB、16-byte 對齊。
4. 取得 __bss_start／end，逐 byte 清除左閉右開範圍。B 個 byte 時間 O(B)、額外空間 O(1)。所有靜態頁表、PCB 與緩衝區在開機時因此歸零。
5. `call kernel_main` 設定 ra（返回位址暫存器），開始執行 C 核心函式。
6. 若 C 意外返回，進入 WFI（等待中斷指令）迴圈，不落入未知內容。

la 是載入位址的偽指令；__ 開頭符號由 linker（連結器）決定，不是 C 變數。`1b` 指向前面出現的數字標籤 1，`2f` 指向後面將出現的標籤 2。
`.option push/norelax/pop` 保存、設定、還原組譯選項，不操作 stack。Makefile 禁用 relaxation（連結指令改寫），不依賴尚未初始化的 gp（全域資料指標）。

## 4. main.c 與 console.c

kernel_main 依序 trap_init、boot_splash、輸出啟動文字、vm_init、process_init、process_start。先安裝例外入口，讓啟動畫面期間的錯誤也能診斷。
UART 位址固定 0x10000000，LSR（線路狀態暫存器）offset 5：bit 5 可寫入字元，bit 0 有收到的字元。

- console_putc 輪詢可寫狀態，再寫 offset 0；換行前補 CR（回車）。
- console_puts 掃描零結尾字串逐字輸出；console_hex 固定 16 位十六進位；console_number 輸出十進位並處理負數。
- console_getc 非阻塞讀取，有字元回傳 0–255，否則 -1；不在此函式裡等待整行。
- kernel_idle 永久執行 WFI；只有全部回收、無事件可喚醒的阻塞或致命錯誤才進入。等待 shell 輸入不進 WFI，因為本版沒有 UART 中斷。

使用 volatile（要求編譯器保留存取）的 MMIO（記憶體映射裝置暫存器）指標；它不是鎖或通用記憶體屏障。輸出耗時 O(N) 加裝置等待，輸入每次檢查 O(1)。

### 開機圖案：kernel/splash.c / splash.h

boot_splash 在進入使用者模式前，利用 console 輸出 7×52 的固定 ASCII（文字字元）圖案，左側留白 14 欄。
以中點 26 為中心，radius（揭露半徑）由 1 增至 26，畫面只顯示 [26-radius, 26+radius) 內的圖案字元，其餘輸出空白；每格增加左右各一欄。
每格固定 O(H×W)，共 26 格，沒有配置動態記憶體。這是逐欄揭露圖案，不是變更字型或縮放。
ANSI（終端控制碼）ESC[2J 清屏、ESC[H 回到起點重繪，ESC[?25l/h 隱藏／還原游標；最終字樣保留在 shell 上方。
hardware_time 使用 rdtime（讀取硬體時間計數器指令），pause_ms 依 QEMU virt 的 10 MHz 時基輪詢每格 45 ms、最後 200 ms，預定延遲合計約 1.37 秒，另加序列輸出時間。
此處沒有啟用 timer interrupt（計時器中斷），也不新增延遲 syscall；忙等只發生於開機期間。尚未從 DTB（硬體描述資料）解析時基，限目前 QEMU virt 平台。
不支援 ANSI 的輸出環境可能顯示控制碼；小於 80 欄可能換行影響動畫，非本次驗收範圍。

## 5. trap.S / trap.c / trap.h：硬體事件到 C

| 名稱 | 中文用途 |
| --- | --- |
| stvec | 監督模式陷阱向量暫存器：入口位址及模式 |
| scause | 監督模式陷阱原因暫存器：中斷／例外的原因 |
| sepc | 監督模式例外程式計數器：相關指令位址 |
| stval | 監督模式陷阱額外資訊：依原因提供錯誤位址等資訊 |
| sstatus.SPP | 進入陷阱前的模式，0 表示使用者、1 表示監督模式 |
| sscratch | 監督模式暫存用途暫存器：本核心用它保存使用者陷阱所需的核心 stack 頂端 |

trap_init 安裝 4-byte 對齊的 trap_entry，採 Direct mode（共同入口模式）。哪些事件交給 S-mode（監督模式）由 OpenSBI 的 delegation（陷阱委派）設定決定。
CPU 更新原因／位置／模式並跳至入口，但不自動保存全部一般暫存器，也不會自動安排核心 stack。

trap_entry 用 `csrrw sp, sscratch, sp` 交換堆疊。來自使用者時取得核心 stack；來自核心時 sscratch 原本為 0，交換後再換回，繼續使用原核心 stack。
配置 288-byte TrapFrame，保存一般暫存器、原 sp、sepc／sstatus／scause／stval，清除 sscratch 後呼叫 trap_handler。
trap.h 的數字位移供組合語言使用，C 結構用相同布局；_Static_assert（編譯期布局契約）避免兩邊不一致，並非 runtime 測試。

trap_handler 保存目前行程的 frame，僅在 SPP=0、scause=8 時分派使用者 ecall（環境呼叫指令）。ecall 固定 4 bytes，因此先將 sepc 加 4；其他例外不套用此位移。
使用者其他例外終止該行程，核心錯誤顯示資訊後停止。一般呼叫後透過 process_resume 選出返回的 frame。
`mv sp, a0` 改用 C 回傳的 frame；trap_restore 寫回 CSR（控制與狀態暫存器）、還原一般暫存器，最後 sret（監督模式陷阱返回指令）。
第一次 user_enter 也復用這份還原流程。每次保存／還原為固定 O(1)，目前不保存浮點／向量狀態，初始使用者狀態停用相關能力。

## 6. vm.c / vm.h：每個行程的位址空間

Sv39（三級分頁）把位址拆成三個 9-bit 索引與 12-bit 頁內 offset（偏移）。每表 512 個 PTE（頁表項目）、對齊 4 KiB。
共用 kernel_l1／kernel_l0／uart_l0 三份表；每個 AddressSpace（位址空間）預留四份表（root／low_l1／user_l0／stack_l0）及四個 code 頁、一個 data 頁、一個 stack 頁，共 40 KiB。
storage[5] 基底對齊 4 KiB，結構內部與長度皆整頁，確保所有頁表對齊。spaces[4] 保存各行程目前擁有的空間指標，spare 指向剩下的一份，供 exec 預先準備。

| 使用者虛擬範圍 | 用途／權限 |
| --- | --- |
| 0x400000–0x404000 | code：16 KiB，RXU（可讀／可執行／使用者可存取） |
| 0x404000–0x405000 | data：4 KiB，RWU（可讀寫／使用者可存取） |
| 0x800000–0x801000 | stack：4 KiB，RWU |
| 0x10000000 的一頁 | UART，僅核心 RW |
| 0x80200000–__kernel_end | 核心，code RX、唯讀資料 R、其餘 RW，全部不設 U |

所有範圍左閉右開。相同虛擬位址在不同 PCB 的根表指向不同實體頁。核心另有 identity mapping（虛實位址相同映射）用於存取私有頁的實體儲存區。

- table_entry 建立下一級 PPN（實體頁編號）及 V（有效）bit；leaf_entry 加入 R/W/X/U 權限與 A/D（已存取／已寫入）bit。
- vm_init 建立共用核心／UART 映射，檢查嵌入映像不超過 code+data 限制。
- prepare_space 先清除 40 KiB 的表與私有空間，再依 program 選 shell 或 app 映像複製、建立私有映射；vm_create(slot, program) 用它初始化指定欄位。
- vm_clone(slot, source_root) 複製父行程的 16 KiB code、4 KiB data、4 KiB stack；不複製父頁表，保留子行程指向自己實體頁的映射。root 是 AddressSpace 第一個成員，因此可由根表位址取得來源儲存區。
- vm_exec(slot, program) 在 spare 先準備完整新映像，再交換 spaces[slot] 與 spare；舊空間成為下一次替換的備用空間。程式 ID 已由 process_exec 驗證，全部資源預先配置，沒有交換後才配置失敗的步驟。
- vm_enable 寫入 satp（位址轉換設定暫存器），執行 sfence.vma（轉換快取同步）與 fence.i（指令取得同步）。
- user_physical 從明確指定的根表步行三級，要求有效、4 KiB leaf（末級映射）、U 及要求的讀寫權限。
- user_readable／user_writable 驗證完整範圍、跨頁與整數溢位；user_read_byte／user_write_byte 經核心映射存取已驗證的 byte。
- user_write_long 逐 byte 回寫 long，支援非對齊與跨頁；呼叫者必須先驗證完整範圍。

建立 O(S+I)，S 為固定空間大小、I 為映像長度；P 個頁面的權限檢查 O(P)，單 byte 轉譯固定三級 O(1)。
fork 另有 O(U) 使用者頁複製，U 固定 24 KiB；exec 清除並複製新映像，空間交換 O(1)。五份 AddressSpace 合計 200 KiB，不含共用頁表、PCB 與核心堆疊。
不直接解參考使用者虛擬指標，也不開啟 SUM（允許監督模式直接存取使用者頁面的狀態 bit）。阻塞期間頁表不會被使用者修改；取消／終止會先移除 pending I/O，避免向已回收行程複製。

## 7. process.c / process.h：PCB 與生命週期

原本的 Process 結構現在以 ProcessControlBlock 標記，仍只有一份真實狀態。processes[4] 是行程表，kernel_stacks 每欄預留 16 KiB。
PCB 保存 PID／父 PID／角色／狀態／結束碼、頁表與兩種 stack、frame、等待原因／事件通道／子 PID／結果位址、stdin/stdout pipe（輸入／輸出端點）、待完成 I/O 的種類／位址／長度／進度。
`ready_next` 指向下一個就緒 PCB，`ready_queued` 標示是否已入列；只有 Ready 行程位於佇列，Running／Waiting／Zombie／Unused 不佔據佇列節點。行程表仍保存所有狀態，佇列不複製 PCB。

```text
Unused（可用）→ New（建立中）→ Ready（就緒）→ Running（執行中）
                                 ↑               │
                                 ├── yield ──────┤
                                 │               ↓ sleep / wait / read / write
                                 └── 喚醒 ── Waiting（等待中）
Running／Waiting／Ready → exit／kill → Zombie（待回收） → wait → Unused
```

- create_process 掃描空欄位、分配遞增 PID、清除 PCB／stack、建立私有頁表、設定初始 frame，最後公布為 Ready。
- 初始 a0（參數暫存器）存內建角色，sp 是使用者 stack 頂端，sepc 是 0x400000；sstatus 選 U-mode，停用中斷及浮點／向量能力。
- process_init 建立 PID 1 的 shell；process_spawn 只接受 worker／quick，父 PID 為目前行程。
- process_fork 建立尚未公布 Ready 的子 PCB，複製使用者頁及父 TrapFrame，設定子 a0=0、繼承端點後才改成 Ready；回傳新 PID 給父。trap_handler 已將 sepc 越過 ecall，父子都從 fork 呼叫後繼續。核心 stack 不複製，子使用新的保存框架。
- process_exec 先拒絕未知 ID，再準備／交換位址空間。清除舊使用者暫存器、設定 sp／入口／a0 角色，保留原本合法的 sstatus、PID／父子關係／核心 stack／端點，清除等待及 I/O 欄位。切換 satp 後，trap.S 直接恢復新入口，不會返回舊 shell。所有空間共用核心映射，所以切換時目前核心 stack 仍有效。
- transition 統一記錄與更新狀態，進入 Ready 時排到 FIFO 隊尾，離開 Ready 時移除；scheduler_next 已出列的節點不重複移除。trace_enabled 可關閉子行程的追蹤，但不會停用佇列維護。只有 PID 1 不輸出狀態紀錄，以免打斷提示字元，仍有真實狀態供 ps／pcb 查閱。fork 出來但還沒 exec 的 shell 子行程照常記錄。
- process_resume 先輪詢 I/O；原行程仍 Running 時繼續它，否則回收無父行程 Zombie，再呼叫 scheduler_next 取出最早入列的 Ready PCB。這是 FIFO 非搶占式排程；普通 syscall 不讓出 CPU，主動 yield 才重新排到隊尾。
- 返回使用者前更新 satp，將所選 frame 返回 trap.S；不保存暫停中的 C 呼叫堆疊，原 kernel stack 在返回時被放棄。
- 沒有 Ready 但有人等待 console 時持續輪詢 UART；沒有事件來源的全部阻塞顯示診斷；全部回收後進 kernel_idle。

process_sleep 等正整數 channel（事件通道），不是睡幾秒；process_wakeup 喚醒同通道所有事件等待者，回傳數量。通知不累積，先通知、後等待不會收到舊事件。
process_wait 僅等待直接子行程；已是 Zombie 就 collect_child，否則記錄目標與結果位址並進 WAIT_CHILD。
子行程結束會使父行程回 Ready；finish_wait 在父行程再被選中時填入原 frame 的 a0 與使用者 status，再回收子行程。
wait 的 status 可為空指標，否則阻塞前及回寫時檢查完整可寫範圍。驗證失敗時保留 Zombie 供重試。

terminate 由 exit／kill 共用：關閉端點、取消 pending I/O、清理等待資訊、保存結果、成為 Zombie，並通知等待自己的父行程。
process_kill 只接受目前行程的直接子行程，不接受自己、PID 1 或已是 Zombie，結果為 -9；不是完整訊號系統。
父行程退出時子行程交給仍活著的 PID 1，否則無父行程；無父行程的 Zombie 由核心回收。
reap 只將欄位改為 Unused，不立即清除仍可能使用中的 stack／頁表；下次建立時才清除。

### 查詢與固定管線建立

process_list 列出有效 PCB、剩餘欄位與 scheduler_print_ready 的 FIFO 順序；process_inspect 顯示實際頁表、保存指令位置、stack、等待資訊、結束碼、端點、I/O 請求與 ready_queued。
process_pipeline 先驗證兩個 PID 的回寫範圍與資源，分配 pipe，再建立 writer／reader，設定 writer 的 stdout 與 reader 的 stdin；失敗時回收未公布的資源。
同一個核心系統呼叫內不會被其他行程插入，因此沒有建立到一半被排程的狀況。
固定表大小為 P=4，尋找／喚醒為 O(P)；建立另加固定空間清理／映像複製。候選行程由 FIFO 隊首 O(1) 取出，但 process_resume 仍有 I/O 與 orphan 回收掃描，不宣稱整個呼叫都是 O(1)。

### scheduler.c / scheduler.h：保存 Ready 行程的 FIFO

`ReadyQueue` 保存 `Process *head`、`Process *tail` 與 count。節點鏈結直接放在 PCB，無需 malloc 或額外複製行程資料。
`scheduler_enqueue` 先檢查 ready_queued 防止重複入列，再接到 tail；`scheduler_next` 取出 head、清除該 PCB 的鏈結與入列標記，取走最後一個節點時也清空 tail。兩者 O(1)。
`scheduler_remove` 尋找指定 PCB 的前一個節點，再重接鏈結；同時處理隊首、中間、隊尾與唯一節點，耗時 O(P)。kill Ready 行程與管線建立失敗回收時，透過 transition 移除節點，避免欄位重用後仍被舊鏈結排程。
排程順序依實際入列時間，同一輪 wakeup／I/O 完成仍按既有 PCB 掃描順序入列，不按 PID 排序。單核心且核心中斷關閉，無佇列並行修改；未實作多核心鎖或時間片。
參考筆記：`notes/ch5 CPU schedueling.md`「FCFS 理論觀念與數學剖析」。原始 FCFS 的非搶占特性保留，專案額外保留主動 yield 後排到隊尾的介面；counter 交替是主動讓出造成的，不是 timer 搶占。

## 8. io.c / io.h：行輸入與真實管道

### 終端行輸入

io_read 先驗證 fd（描述號）、長度與完整可寫範圍。fd0 未接管道時接受 2–128-byte 行輸入請求。
process_block_io 將緩衝區／長度存入 PCB、設定 IO_CONSOLE_READ，狀態改為 Waiting。不是在使用者程式持續讀硬體。
console_read_line 非阻塞輪詢字元、回顯、處理 CR／LF 與 Backspace。CR 後的 LF 會忽略；超長行丟棄到換行後回傳錯誤，不執行截斷命令。
完成時回寫換行與長度，process_complete_io 把結果填入保存 a0，再將行程改成 Ready。

### Pipe 環形緩衝區

pipes[2] 每條有 used、readers／writers（端點數）、16-byte 資料陣列、read_at／write_at（索引）與 count（資料量）。
寫一個 byte：放入 write_at，索引加一對 16 取餘數，count 加一。讀取相反；傳輸 N 個 byte 耗時 O(N)，儲存固定 O(16)。

- pipe_read：有資料就回傳最多請求量；空且還有 writer 回 IO_PENDING（尚未可完成）；空且沒有 writer 回 0，也就是 EOF。
- pipe_write：有空間就寫入並回傳實際量，可短寫；滿且有 reader 就 pending；沒有 reader 回 -32（broken pipe，無讀者）。
- io_write：fd1 未接管道時輸出至 console，否則走 pipe_write。所有來源範圍先檢查可讀權限。
- io_poll：掃描 Waiting／WAIT_IO 的 PCB，重試待完成請求；完成一次傳輸／EOF／錯誤後填返回值並喚醒。若一次完成請求促成另一請求就緒，繼續掃描直到無進展。
- io_close_process：exit／kill 時減少該行程端點數；兩端都關閉就釋放 used。其他端的待完成請求會由下一次 io_poll 判斷 EOF／錯誤。
- io_inherit：fork 時複製輸入／輸出端點並增加 readers／writers 參照數。exec 不關閉端點，exit／kill 逐份減少，避免其中一個持有者結束就提早出現 EOF。

本版只有固定 fd0 輸入與 fd1 輸出，沒有一般描述號表、pipe()/dup2()/close() API 或多段管線。固定管線初始讀者／寫者各一個，fork 繼承可增加持有者。建立／繼承／關閉在核心單核心區間內完成，不需要額外鎖。
只有 shell 會讀 console，內建 reader 綁定 pipe；不宣稱支援多行程同時編輯一個終端。

## 9. block.c 與 fs.c：I/O 層和檔案系統

這兩層的關係是：檔案系統把路徑和檔案位移轉成磁碟區塊編號；block driver（區塊裝置驅動）負責把指定 512-byte sector 讀入記憶體或寫回 QEMU 磁碟。檔案內容、inode、目錄等格式由 WUFS 管理，VirtIO driver 不理解檔名。

kernel_main 從 OpenSBI 傳入 a1 的 FDT（硬體裝置樹），block_init_from_fdt 搜尋 `virtio,mmio` 節點與 reg（裝置暫存器位址），再檢查 magic、Version 2 與 Device ID 2（block）。virtio-mmio 有多個空插槽，所以逐一略過未接裝置與非 block 裝置，不假設磁碟固定在某個位址。`vm_map_mmio` 將找到的那一頁加入核心監督模式映射。

driver 協商 VERSION_1 feature，選 queue 0，建立一組 descriptor（描述項）鏈：request header → 512-byte data buffer → status byte。寫入 available ring，增加 index，敲 MMIO queue notify，輪詢 used index 等待完成。此版本一次只送一個請求，I/O 是同步輪詢；沒有中斷、佇列併發、DMA IOMMU 或 block cache。單筆正常操作 O(1)，檔案 N 個區塊則 O(N) 次同步讀寫。

WUFS 在 LBA 0 存 superblock，接著 inode bitmap、block bitmap、固定 inode 表，最後才是資料區。inode 80 bytes，含類型、檔案長度、8 個直接區塊和 8 個單層索引區塊；每檔上限 (8 + 8×128)×512 = 528384 bytes。inode 表固定 128 個項目；16 MiB 磁碟有 32768 個 sector。bitmap 逐位標記 inode 與已用磁碟 sector。

目錄內容是順序 DirectoryEntry（24-byte 名稱 + 4-byte inode 編號），路徑查找從 root inode 逐層掃描目錄。C 層支援跨 sector 的目錄項目複製。`fs_list` 回傳目錄名稱，`fs_mkdir` 建立目錄及 `.`／`..`，`fs_touch` 建立空檔，`fs_read` 依位移讀資料，`fs_write` 釋放舊區塊後覆寫並更新 inode 長度。

`tools/make_disk.py` 在首次使用時建立 WUFS 目錄與 `/boot/kernel.elf`；每次 make run 只替換這個 inode 的資料區，其他目錄項目和使用者檔案會保留。核心本身也能在空白磁碟格式化並建立目錄。`/boot/kernel.elf` 是一般核心 ELF 檔案副本；QEMU 目前仍以 `-kernel` 載入另一份 ELF，磁碟檔案沒有參與開機。

目前路徑型 syscall 是教學用介面，不是 POSIX VFS：沒有 `open/close`、檔案 descriptor table、cwd（目前目錄）、相對路徑或通用 `read/write` 檔案 FD。shell 的 `write` 目前只接一個文字 token，`cat` 顯示前 2048 bytes；使用者地址驗證限制一次檔案 syscall 位於單一 4 KiB 使用者資料頁。磁碟資料寫入同步完成，未實作掉電交易／journal（寫入日誌）與損毀復原。

## 10. syscall.c、共用編號與使用者 API

WUWAOS 自訂 ABI（呼叫約定）：a7（呼叫編號暫存器）、a0–a2（參數暫存器）、a0（返回值暫存器）。不是 Linux 編號。

| 編號 | 介面 | 核心服務 |
| --- | --- | --- |
| 1–3 | write、getpid、exit | I/O、目前 PID、退出 |
| 4–8 | spawn、yield、sleep、wakeup、wait | 行程生命週期 |
| 9 | ps | 有效 PCB 列表 |
| 10 | pcb(pid) | 特定 PCB 保存資料 |
| 11 | kill(pid) | 終止直接子行程 |
| 12 | read(fd, buffer, length) | console 行輸入／pipe byte 讀取 |
| 13 | pipeline(pids) | 固定 writer／reader 建立 |
| 14 | trace(enabled) | 開關子行程追蹤 |
| 15 | fork() | 複製使用者執行狀態，父返回子 PID、子返回 0 |
| 16 | exec(program_id) | 以內建 app 替換當前程式，成功不返回舊映像 |
| 17 | fs_list(path, buffer, capacity) | 將目錄項目文字複製到使用者緩衝區 |
| 18 | fs_mkdir(path) | 建立目錄 |
| 19 | fs_touch(path) | 建立空檔案或確認既有檔案 |
| 20 | fs_readfile(path, buffer, capacity) | 從檔案開頭讀取最多 capacity bytes |
| 21 | fs_writefile(path, buffer, length) | 建立或覆寫檔案 |

syscall_dispatch 用 switch 分派；正常立即結果填 frame->a0，阻塞時先存 placeholder（暫存結果），由排程中的 I/O 或 wait 完成處理替換後才真正返回。
exec 成功時 dispatcher 立即返回，不能把正常返回值 0 再寫進 frame->a0，否則會破壞新程式所需的初始角色參數。失敗才按一般規則回傳錯誤。
filesystem syscall 先從呼叫者頁表逐 byte 驗證並複製 NUL 結尾路徑；資料 buffer 驗證完整使用者範圍後才透過核心暫存區交給 fs.c。user/api.c 的 syscall3 使用固定暫存器與 memory clobber（記憶體可能改變宣告）執行 ecall，二十一個包裝函式供使用者程式呼叫。
錯誤碼：-10 非直接子行程／沒有該子行程、-11 資源不足、-14 記憶體不可存取、-22 非法參數、-32 管道無讀者、-38 未知呼叫。

## 11. user/shell.c、demo.c、app.c、print.c

shell_main 顯示提示、read 一行、tokenize（切分命令）、command（分派），全程在 U-mode 執行。
每次提示前 yield 兩輪，足以讓本版 worker（自身先 yield 一次）到達等待點，避免背景文字蓋過新提示；不是通用工作控制或時間片。
tokenize 原地將空白／tab／換行及 | 分開，最多 4 個 token（詞元）。只有 writer、|、reader 的三詞組合啟動固定管線，其他組合不會被當作主機命令。
positive_number 檢查每個十進位數字與 LONG_MAX 溢位；wait_child 顯示結果，run_pipeline 前景等待兩個端點行程。
help、ps、pcb、spawn、fork、run、yield、wakeup、wait、kill、trace、固定管線、ls、mkdir、touch、cat、write 與 exit 的操作詳見 README。`cat` 顯示最多 2048 bytes；若非空輸出最後沒有 LF（換行字元），shell 會補一個換行，避免提示字元緊接檔案內容；不修改檔案本身。

fork_shell 先增加全域 fork_sequence，再 fork。子行程修改自己的全域資料及區域 stack marker，印出子返回值 0，exit(0)；父顯示子 PID，主動 yield 讓子先修改，再顯示自己仍保留的原資料／堆疊值，留下子行程給 wait 回收。這是正常互動命令，不含測試斷言。
run_program 將 worker／quick／counter 轉成 ID，先 fork，只有子分支呼叫 exec；成功不回來，失敗印錯誤並 exit(127)。父立即返回命令列，由使用者觀察、喚醒、終止或回收。

print.c 的 user_write_all 迴圈處理短寫；user_puts 掃描字串；user_number 以除 10 與反轉輸出十進位，支援超過 9 的 PID。print.h 宣告共用輸出，shell.h 只宣告 shell 入口。
命令解析 O(L)，L 最多 126；不支援引號、一般參數、多段管線或通用檔案描述號。

demo.c 的 user_main 只進入 shell；app.c 是另一份獨立映像，依初始 a0 角色分派：1 worker（yield 後 sleep(7)，醒後 exit 0）、2 quick（exit 40）、3 writer、4 reader、5 counter。
counter 先等待通道 9，再讀寫自己初始化為 3 的 volatile 全域 remaining，每次輸出後減一並 yield，至 0 結束。兩份 counter 各自從 3 開始，展示初始化資料與交替執行；沒有硬體時間片。
writer 正常輸出 80 bytes，以 user_write_all 處理管道短寫；reader 每次最多讀 12 bytes，等 EOF 後將完整訊息輸出到 console。接收緩衝區有界，不能無限制累積資料。
user/entry.S 保留初始 a0 呼叫 user_main，返回值再交給 exit；exit 如果意外返回就在使用者模式迴圈。

## 12. Linker 與 Makefile

kernel/linker.ld 由 0x80200000 配置 .text（程式碼）、.rodata（唯讀資料）、.data（有初值資料）、.bss（零初始化資料）、啟動 .stack；前 2 MiB 保留給 firmware。
程式碼與唯讀區終點對齊 4 KiB，__text_end／__rodata_end 供 vm 區分 RX／R／RW。__bss_start／end 供啟動歸零，__stack_top 供啟動 stack，__kernel_end 為映像保留終點。
NOLOAD 不攜帶初始化內容但保留記憶體，KEEP 保護入口，ALIGN 決定對齊；限制核心不超過 0x80400000，以配合單份末級表的 2 MiB 核心映射。

user/linker.ld 的入口固定 0x400000，code／唯讀資料最多 16 KiB，data／BSS 從 0x404000 開始最多 4 KiB；ASSERT（連結期限制）防止超出。stack 由核心映射，不在 user.bin 中。
Makefile 將 user/entry.S、api.c、print.c、demo.c、shell.c 連結為 user.elf；另一組 entry.S、api.c、print.c、app.c 連結為 app.elf。兩者各有自己的 user_main，但不互相連結。llvm-objcopy 轉成 user.bin／app.bin，kernel/user_image.S 的 incbin 嵌入核心唯讀資料，vm 依程式 ID 複製映像。不是 ELF parser（執行檔解析器）。
`make run` 先呼叫 tools/make_disk.py 建立或更新 `build/wuwaos.img`，再以 `-drive` 與 `virtio-blk-device` 接上 QEMU。Python 工具用相同 WUFS metadata layout 建立空卷、Linux 風格目錄與 `/boot/kernel.elf`；辨識已格式化映像後只更新 kernel inode，保留其他檔案。`make clean` 特意保留磁碟映像。
binary 以 0x400000 為起點，若有初始化 .data，會保留到 0x404000 的位址間距；vm 將前 16 KiB 放 code，其後放 data。BSS 不攜帶映像內容，prepare_space 歸零保證其初值。

Clang 交叉編譯為 rv64imac_zicsr_zifencei，ABI 為 lp64，LLD 連結；不依賴標準函式庫、浮點呼叫慣例或動態載入器。
-mcmodel=medany 適用高位址核心；-mno-relax／-msmall-data-limit=0 避免 gp 依賴；-ffreestanding／-fno-builtin／-nostdlib 用於裸機執行環境；-fno-pic／-fno-pie 固定位址。
-O2 最佳化、-g 除錯資訊、-Wall -Wextra -Werror 將警告視為錯誤；-MMD -MP 追蹤標頭相依。未來 C 寫法若產生 memcpy 等呼叫，仍需提供實作。
all 建置、run 啟動、clean 清理 build 產物。更改 flags（編譯選項）後需 clean 再建置；$< 是第一個相依項，$@ 是目標，| build 先確保目錄存在。

## 13. 驗證與閱讀練習

行程驗收與 QEMU monitor 證據在 SPEC.md；VirtIO／WUFS 磁碟重啟持久性在 SPEC-06；FIFO 與 Ready 行程移除驗收在 SPEC-07。ps／pcb 是讀取真實 PCB 與就緒佇列；pipe 的 80 bytes 必須通過 16-byte 緩衝區與多次狀態切換。
未完成 runtime 驗證的邊界不可等同已測試；沒有新增未經同意的測試程式碼。
可先照 README 操作 worker，對照 saved sepc（保存指令位置）說明恢復後從哪裡繼續；再追蹤 writer 滿管道時的 buffer 位址如何留在 PCB。

仍無計時器搶占、多核心、通用檔案描述號、任意執行檔與前景 Ctrl+C 取消。若 shell wait 一個永遠不會收到事件的子行程，也無其他行程能通知，會阻塞；先 wakeup 再 wait。

## 官方參考

- [RISC-V Supervisor 規格：特權暫存器與 Sv39](https://docs.riscv.org/reference/isa/priv/supervisor.html)
- [QEMU virt 與 OpenSBI](https://www.qemu.org/docs/master/system/riscv/virt.html)
- [LLVM RISC-V target](https://llvm.org/docs/RISCVUsage.html)
- [Linker script](https://www.sourceware.org/binutils/docs/ld/Scripts.html)
