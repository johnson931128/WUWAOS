# WUWAOS 程式碼與架構紀錄

記錄目前所有手寫程式碼、標頭、連結及建置設定。現行設計由 SPEC.md 與後續 SPEC-06 至 SPEC-10 組成；記憶體以 SPEC-10 為準，SPEC-01／02／03／04 與後續各階段驗收保留當時紀錄，不代表現在的全部限制。依主題閱讀可從[文件首頁](./index.md)開始，開發階段見[歷史索引](./history.md)。
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
| kernel/io.c / io.h | 行輸入／raw console、固定管道、pending I/O（尚未完成的輸入輸出） |
| kernel/block.c / block.h | FDT 裝置探索、VirtIO-MMIO 區塊讀寫佇列 |
| kernel/fs.c / fs.h | WUFS 區塊配置、inode、目錄與路徑操作 |
| kernel/memory.c | freestanding（無標準執行環境）建置所需的 byte-wise memset／memcpy |
| kernel/page.c / page.h | 4 KiB 實體頁面空閒串列、配置／回收、配置 bitmap 與頁數查詢 |
| kernel/swap.c / swap.h | 獨立交換區標頭、slot bitmap、4 KiB 換入／換出讀寫 |
| kernel/syscall.c / syscall.h | 24 個 syscall（系統呼叫）的編號分派 |
| kernel/user_image.S | 把 user.bin、app.bin 與 fan.bin 三份獨立映像嵌入核心唯讀資料 |
| kernel/linker.ld | 核心入口、記憶體區段與權限邊界 |
| include/syscall_numbers.h | 共用系統呼叫編號、內建程式角色、錯誤碼 |
| include/process_limits.h | 行程與位址空間共用的上限 4 |
| include/vm_limits.h | heap 起點、頁面大小、heap 頁數與常駐頁預算 |
| user/entry.S | 使用者入口，將核心給的角色與可選路徑交給 user_main，返回後 exit |
| user/api.c / api.h | 使用者系統呼叫包裝函式 |
| user/shell.c / shell.h | 互動 shell、命令解析、fork／run／管線操作 |
| user/demo.c | shell 映像的 user_main，呼叫 shell_main |
| user/app.c | 獨立 app 映像的 user_main，分派 worker、quick、writer、reader、counter |
| user/fan.c | 獨立文字編輯器映像、緩衝區編輯、按鍵解碼、TUI、儲存與退出確認 |
| user/print.c / print.h | 三種映像共用的字串／數字輸出與短寫處理 |
| tools/make_disk.py | 建立／更新 WUFS 磁碟映像中的 kernel.elf，保留其他檔案 |
| tools/make_swap.py | 建立有格式標記的獨立交換磁碟，可調整 slot 數，拒絕重格式化既有檔案 |
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

kernel_main 依序 trap_init、boot_splash、輸出啟動文字、block_init_from_fdt、vm_init(fdt)、fs_init（有磁碟時）、process_init、process_start。先安裝例外入口，讓啟動畫面期間的錯誤也能診斷。vm_init 會初始化實體頁面配置器並啟用常駐核心分頁表。
UART 位址固定 0x10000000，LSR（線路狀態暫存器）offset 5：bit 5 可寫入字元，bit 0 有收到的字元。

- console_putc 輪詢可寫狀態，再寫 offset 0；換行前補 CR（回車）。
- console_puts 掃描零結尾字串逐字輸出；console_hex 固定 16 位十六進位；console_number 輸出十進位並處理負數。
- console_getc 非阻塞讀取，有字元回傳 0–255，否則 -1；不在此函式裡等待整行。
- kernel_idle 永久執行 WFI；只有全部回收、無事件可喚醒的阻塞或致命錯誤才進入。等待 shell 輸入不進 WFI，因為本版沒有 UART 中斷。

使用 volatile（要求編譯器保留存取）的 MMIO（記憶體映射裝置暫存器）指標；它不是鎖或通用記憶體屏障。輸出耗時 O(N) 加裝置等待，輸入每次檢查 O(1)。

### 開機圖案：kernel/splash.c / splash.h

boot_splash 在進入使用者模式前，用 W／U／A／O／S 的 5×7 bitmaps（位元字形）及索引序列組成 WUWAOS。每個點占兩欄空格，利用 ANSI 256 色背景填出實心筆畫，不依賴 UTF-8 字型；七行由螢光黃綠漸變到薄荷綠。加上字母間兩欄間距，圖案 7×70、左側留白 5 欄。
以中點 35 為中心，radius（揭露半徑）由 1 增至 35，只填入 [35-radius, 35+radius) 內的有效字形像素，其餘輸出預設背景空格；每格增加左右各一欄。填色狀態改變時才輸出顏色控制碼，行尾與動畫結束重設，避免 shell 被染色。
每格固定 O(H×W)，共 35 格，沒有配置動態記憶體。這是逐欄揭露圖案，不是變更終端字型或縮放。
ANSI（終端控制碼）ESC[2J 清屏、ESC[H 回到起點重繪，ESC[?25l/h 隱藏／還原游標；最終字樣保留在 shell 上方。
hardware_time 使用 rdtime（讀取硬體時間計數器指令），pause_ms 依 QEMU virt 的 10 MHz 時基輪詢每格 32 ms、最後 200 ms，預定延遲合計約 1.32 秒，另加序列輸出時間。現行設計與驗收見 [Spec 13](./SPEC-13-block-splash.md)，Spec 04 保留最初 ASCII 圖案歷史。
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
使用者 page fault（缺頁例外）12／13／15 則先呼叫 vm_handle_fault；合法按需配置／換入成功後保留原 sepc 重試，非法或無法補頁才結束行程。這不是把所有缺頁當成一般錯誤，也不對缺頁指令套用 ecall 的 +4。
`mv sp, a0` 改用 C 回傳的 frame；trap_restore 寫回 CSR（控制與狀態暫存器）、還原一般暫存器，最後 sret（監督模式陷阱返回指令）。
第一次 user_enter 也復用這份還原流程。每次保存／還原為固定 O(1)，目前不保存浮點／向量狀態，初始使用者狀態停用相關能力。

## 6. vm.c / vm.h：按需分頁、FIFO 與位址空間

Sv39 將虛擬位址拆成 L2／L1／L0 三個 9-bit 索引與 12-bit 頁內位移；每張表有 512 個 8-byte PTE，對齊 4 KiB。kernel_root 共用 supervisor-only 的核心／RAM／UART／兩顆 VirtIO 裝置映射，使用者不能存取這些映射。

| 使用者範圍 | 權限與 backing（重建來源） |
| --- | --- |
| 0x400000–0x404000 | code，RXU，首次存取從 shell／app／fan 內嵌映像載入 |
| 0x404000–0x405000 | data，RWU，首次從映像及零填充初始化 |
| 0x800000–0x801000 | stack，RWU，首次配置零頁 |
| 0x1000000 起，最多 1 MiB | sbrk 保留的 heap，RWU，首次配置零頁 |

只有已保留的合法區域能補頁；heap 最後一個部分頁按頁對齊映射。新建立的行程只先配置私有根表與低位址 L1，不配置使用者頁；L0 表與使用者頁在需要時取得。kernel/root/page-table 頁面常駐、不參與使用者 FIFO 置換。

- VmSpace 保存 root、program、heap_end、缺頁數，以及 262 個 VmPage 補充分頁記錄。spaces[5] 是 metadata，並非預先配置的使用者 RAM；第五欄供 exec 準備新空間。
- VmPage 的 physical 非零表示常駐；slot 非零表示有獨立 swap backing；兩者皆零時由原始映像或零頁重建。dirty 與硬體 PTE_D 合併判斷修改狀態，pins 表示暫時不可換出，arrival 表示本次入駐次序。
- vm_walk 用 `(address >> (12 + 9 * level)) & 511` 逐層取索引，需要時配置下層表。核心 vm_map 僅安裝 4 KiB 映射，拒絕覆蓋既有 PTE。
- resident_page 取得頁框並完整讀取 swap 或填入映像／零頁，成功後才公布有效 PTE；kernel copyout 主動標 dirty，使用者直接寫入則由硬體 D bit 記錄。換入失敗保留原 slot，不公布部分讀取的頁面。
- evict_one 掃描全域補充表，選 arrival 最小、未 pin 的常駐頁。命中不更新 arrival，因此是 FIFO，不是 LRU。dirty 頁寫入八個磁區成功後才撤銷 PTE；clean 頁直接保留既有 backing 或回到可重建狀態。sfence.vma 撤銷舊轉換後才釋放實體頁。
- 使用者常駐預算預設 64 頁、最小可設 8 頁；超過預算或 page_alloc 無可用頁時嘗試置換。沒有可安全換出的頁面回 -12；磁碟失敗回 -5。QEMU RAM 總量仍是 128 MiB。
- vm_handle_fault 依 scause 12／13／15 檢查執行／讀／寫權限，合法者補頁；處理時 pin 住目前指令可能占用的 code 頁，避免 swap full 時 code 與 operand 互相淘汰。trap_handler 成功後重試同一 sepc，不像 ecall 加 4；非法位址／權限回 -14 終止該行程。
- vm_sbrk 只調整合法 heap 範圍；縮小時釋放不再涵蓋的整頁及 slot，空的分頁表暫留到位址空間銷毀。部分頁縮小再擴張不保證既有 bytes 清零。
- vm_clone 複製常駐內容時 pin 來源頁；已換出的內容透過核心常駐暫存頁複製至獨立 slot。未曾使用的頁不需載入，子行程保留相同映像／零頁來源。任何失敗由 process_fork 銷毀不完整子空間，父內容保留。
- vm_destroy 先釋放補充表中所有使用者實體頁與 swap slots，再遞迴回收私有分頁表，跳過共用核心分支。拒絕銷毀 satp 正在使用的根表。exec 成功切換後才銷毀舊空間；Zombie 保留資源直到 wait／孤兒回收。
- user_readable／user_writable 驗證合法範圍與權限，不要求常駐。user_copy_from／user_copy_to 逐頁載入後立即複製，不保留可能被下一次配置淘汰的實體指標；失敗明確回傳。user_read_byte／user_write_byte／user_write_long 是短資料包裝。

FIFO 選擇在最壞情況掃描 5×262 個 metadata；複製／換入換出 O(頁面 bytes)，單次轉譯固定三級。swap full 或失效候選者需繼續掃描其他頁，成本可能更高。沒有 COW、mmap、LRU、多 hart 或非同步磁碟 I/O。完整設計與實際限制見 SPEC-10。

### page.c：底層實體頁面配置

目前限 QEMU virt／128 MiB。以 `__kernel_end` 到 FDT 起點間整頁初始化 FreePage 空閒串列，排除韌體、核心、固定核心堆疊與 FDT／頂端保留 RAM。配置取串列頭並清零 4096 bytes，回收檢查對齊／範圍／allocated bitmap 後放回串列頭；串列操作 O(1)。它只配置空閒實體頁，缺頁與 victim 決策屬於 vm.c。

ps 的 free／total 是底層配置池頁數；vmstat 的 resident／limit 是使用者常駐頁政策，兩者不同。pcb 顯示個別空間 resident／swapped／heap_bytes／faults。使用者頁可以已換出但仍保留 swap slot，常駐頁也可以保留 backing slot；slot 在 heap 縮小或行程回收時歸還。

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

- create_process 掃描空欄位、分配遞增 PID、清除 PCB／stack、建立私有頁表、設定初始 frame，最後公布為 Ready。頁面不足回 -12 並恢復 Unused；失敗可能消耗 PID，但不占用 PCB 或頁面。
- 初始 a0（參數暫存器）存內建角色，sp 是使用者 stack 頂端，sepc 是 0x400000；sstatus 選 U-mode，停用中斷及浮點／向量能力。
- process_init 建立 PID 1 的 shell；process_spawn 只接受 worker／quick，父 PID 為目前行程。
- process_fork 建立尚未公布 Ready 的子 PCB，複製使用者頁及父 TrapFrame，設定子 a0=0、繼承端點後才改成 Ready；回傳新 PID 給父。trap_handler 已將 sepc 越過 ecall，父子都從 fork 呼叫後繼續。核心 stack 不複製，子使用新的保存框架。
- process_exec 先拒絕未知 ID，再準備／交換位址空間。清除舊使用者暫存器、設定 sp／入口／a0 角色，保留原本合法的 sstatus、PID／父子關係／核心 stack／端點，清除等待及 I/O 欄位。切換 satp 後，trap.S 直接恢復新入口，不會返回舊 shell。所有空間共用核心映射，所以切換時目前核心 stack 仍有效。
- transition 統一記錄與更新狀態，進入 Ready 時排到 FIFO 隊尾，離開 Ready 時移除；scheduler_next 已出列的節點不重複移除。trace_enabled 可關閉子行程的追蹤，但不會停用佇列維護。PID 1 與 fan 不輸出逐次狀態紀錄，以免打斷提示字元或編輯畫面，仍有真實狀態供 ps／pcb 查閱。fork 出來但還沒 exec 的 shell 子行程照常記錄。
- process_resume 先切換常駐核心根表，再輪詢 I/O；原行程仍 Running 時切回其根表繼續它，否則回收無父行程 Zombie，再呼叫 scheduler_next 取出最早入列的 Ready PCB。finish_wait 可在此時安全釋放剛退出行程的表。這是 FIFO 非搶占式排程；普通 syscall 不讓出 CPU，主動 yield 才重新排到隊尾。
- 返回使用者前更新 satp，將所選 frame 返回 trap.S；不保存暫停中的 C 呼叫堆疊，原 kernel stack 在返回時被放棄。
- 沒有 Ready 但有人等待 console 時持續輪詢 UART；沒有事件來源的全部阻塞顯示診斷；全部回收後進 kernel_idle。

process_sleep 等正整數 channel（事件通道），不是睡幾秒；process_wakeup 喚醒同通道所有事件等待者，回傳數量。通知不累積，先通知、後等待不會收到舊事件。
process_wait 僅等待直接子行程；已是 Zombie 就 collect_child，否則記錄目標與結果位址並進 WAIT_CHILD。
子行程結束會使父行程回 Ready；finish_wait 在父行程再被選中時填入原 frame 的 a0 與使用者 status，再回收子行程。
wait 的 status 可為空指標，否則阻塞前及回寫時檢查完整可寫範圍。驗證失敗時保留 Zombie 供重試。

terminate 由 exit／kill 共用：關閉端點、取消 pending I/O、清理等待資訊、保存結果、成為 Zombie，並通知等待自己的父行程。
process_kill 只接受目前行程的直接子行程，不接受自己、PID 1 或已是 Zombie，結果為 -9；不是完整訊號系統。
父行程退出時子行程交給仍活著的 PID 1，否則無父行程；無父行程的 Zombie 由核心回收。
reap 以 vm_destroy 釋放私有頁面與表、清除 PCB 的根表指標後將欄位改成 Unused；呼叫時必須已切離該根表。固定核心 stack 保留到下次建立時清零，避免清除仍在返回路徑上的 C 堆疊。

### 查詢與固定管線建立

process_list 列出有效 PCB、剩餘欄位與 scheduler_print_ready 的 FIFO 順序；process_inspect 顯示實際頁表、保存指令位置、stack、等待資訊、結束碼、端點、I/O 請求與 ready_queued。
process_pipeline 先驗證兩個 PID 的回寫範圍與資源，分配 pipe，再建立 writer／reader，設定 writer 的 stdout 與 reader 的 stdin；失敗時回收未公布的資源。
同一個核心系統呼叫內不會被其他行程插入，因此沒有建立到一半被排程的狀況。
固定 PCB 表大小為 P=4，尋找／喚醒為 O(P)；建立先配置私有表與補充 metadata，映像頁面延遲載入，fork 另複製已修改的使用者狀態。候選行程由 FIFO 隊首 O(1) 取出，但 process_resume 仍有 I/O 與 orphan 回收掃描，不宣稱整個呼叫都是 O(1)。

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

`console_mode(1)` 取得 raw console 的單一持有權。持有者 read(0, buffer, 1) 設定 IO_CONSOLE_RAW，console_read_raw 取得一個 byte 後直接寫回、不回顯，不要求 Enter。其他 console 讀取者（包括已掛起的請求）回 -16，避免讀走編輯器按鍵；fork 不繼承持有權，exit／kill／成功 exec 釋放它。切換時略過 shell 命令 CR 後尚未消耗的 LF。這是自訂的簡化介面，沒有完整 termios、信號與前景 process group。

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

現行驅動最多初始化兩顆 block 裝置，每顆 BlockDevice 各自保存 descriptor／available／used 佇列與完成計數；同步請求切換 active context，不共享佇列狀態。讀取 LBA 0 的 WUFS0001／WUSWAP01 標記區分檔案系統與交換區，交換磁碟先接或後接皆可。每顆找到的 MMIO 頁都加入核心映射；block_read/write 只走 WUFS 裝置，block_swap_read/write 只走交換裝置。

swap.c 驗證 4 KiB slot 標頭與磁碟容量，以 bitmap 配置 1-based slot；0 表示不可用／已滿。每頁讀寫八個連續 512-byte 磁區。make_swap.py 預設建立 4095 slots／16 MiB，--slots 可調整，既有映像只驗證、不覆寫格式；slot bitmap 是本次開機的使用狀態，不保存重啟前行程。

driver 協商 VERSION_1 feature，選 queue 0，建立一組 descriptor（描述項）鏈：request header → 512-byte data buffer → status byte。寫入 available ring，增加 index，敲 MMIO queue notify，輪詢 used index 等待完成。此版本一次只送一個請求，I/O 是同步輪詢；沒有中斷、佇列併發、DMA IOMMU 或 block cache。單筆正常操作 O(1)，檔案 N 個區塊則 O(N) 次同步讀寫。

單次請求以 `block.h` 的 BLOCK_IO_TIMEOUT_MS 設定門檻（預設 5000 ms），公布 available.index 前讀 rdtime（硬體時間計數器）保存 start。未完成時計算 unsigned 的 now-start，依 QEMU virt 10 MHz 時基比較門檻；到期再讀一次完成索引，仍未完成才印出裝置／角色／讀寫／磁區／經過時間與佇列計數。診斷後關閉監督模式中斷並停在 kernel_idle，不返回、不回收或重用尚可能被裝置 DMA 存取的緩衝區。這是單筆請求的 fail-stop，不是整檔／整頁總逾時，也沒有取消、重試或裝置 reset；初始化設定代數與 UART 等其他輪詢不在範圍。設計與操作紀錄見 [Spec 12](./SPEC-12-block-timeout.md)。

WUFS 在 LBA 0 存 superblock，接著 inode bitmap、block bitmap、固定 inode 表，最後才是資料區。inode 80 bytes，含類型、檔案長度、8 個直接區塊和 8 個單層索引區塊；每檔上限 (8 + 8×128)×512 = 528384 bytes。inode 表固定 128 個項目；16 MiB 磁碟有 32768 個 sector。bitmap 逐位標記 inode 與已用磁碟 sector。

`inode_read`／`inode_write` 依 inode 的 byte offset 逐磁區複製，每段長度取磁區剩餘空間與 inode 剩餘長度的較小值。例如 inode 6 從區內位移 480 開始，必須先處理 32 bytes，再處理下一磁區的 48 bytes。寫入各段前先讀取原磁區，避免覆蓋相鄰 inode；這不是跨磁區的原子交易，第二段失敗仍可能留下部分更新。

目錄內容是順序 DirectoryEntry（24-byte 名稱 + 4-byte inode 編號），路徑查找從 root inode 逐層掃描目錄。C 層支援跨 sector 的目錄項目複製。`fs_list` 回傳目錄名稱，`fs_mkdir` 建立目錄及 `.`／`..`，`fs_touch` 建立空檔，`fs_read` 依位移讀資料，`fs_write` 釋放舊區塊後覆寫並更新 inode 長度。

`tools/make_disk.py` 在首次使用時建立 WUFS 目錄與 `/boot/kernel.elf`；每次 make run 只替換這個 inode 的資料區，其他目錄項目和使用者檔案會保留。核心本身也能在空白磁碟格式化並建立目錄。`/boot/kernel.elf` 是一般核心 ELF 檔案副本；QEMU 目前仍以 `-kernel` 載入另一份 ELF，磁碟檔案沒有參與開機。

目前路徑型 syscall 是教學用介面，不是 POSIX VFS：沒有 `open/close`、檔案 descriptor table、cwd（目前目錄）、相對路徑或通用 `read/write` 檔案 FD。shell 的 `write` 只接一個文字 token，`cat` 顯示前 2048 bytes；核心複製可跨合法使用者頁面，包含按需載入與 swap。磁碟資料寫入同步完成，未實作掉電交易／journal（寫入日誌）與損毀復原。

## 10. syscall.c、共用編號與使用者 API

WUWAOS 自訂 ABI（呼叫約定）：a7（呼叫編號暫存器）、a0–a2（參數暫存器）、a0（返回值暫存器）。不是 Linux 編號。

| 編號 | 介面 | 核心服務 |
| --- | --- | --- |
| 1–3 | write、getpid、exit | I/O、目前 PID、退出 |
| 4–8 | spawn、yield、sleep、wakeup、wait | 行程生命週期 |
| 9 | ps | 有效 PCB 列表 |
| 10 | pcb(pid) | 特定 PCB 保存資料 |
| 11 | kill(pid) | 終止直接子行程 |
| 12 | read(fd, buffer, length) | console 行輸入／raw byte／pipe byte 讀取 |
| 13 | pipeline(pids) | 固定 writer／reader 建立 |
| 14 | trace(enabled) | 開關子行程追蹤 |
| 15 | fork() | 複製使用者執行狀態，父返回子 PID、子返回 0 |
| 16 | exec(program_id) | 以內建 app 替換當前程式，成功不返回舊映像 |
| 17 | fs_list(path, buffer, capacity) | 將目錄項目文字複製到使用者緩衝區 |
| 18 | fs_mkdir(path) | 建立目錄 |
| 19 | fs_touch(path) | 建立空檔案或確認既有檔案 |
| 20 | fs_readfile(path, buffer, capacity) | 從檔案開頭讀取最多 capacity bytes |
| 21 | fs_writefile(path, buffer, length) | 建立或覆寫檔案 |
| 22 | console_mode(raw) | 取得／釋放逐字元、不回顯的終端輸入模式 |
| 23 | sbrk(increment) | 增減 heap 合法範圍，回傳舊 break；按需配置實體頁 |
| 24 | vmstat(frame_limit) | 0 查閱 VM／swap 統計，正數設定使用者常駐頁預算 |

syscall_dispatch 用 switch 分派；正常立即結果填 frame->a0，阻塞時先存 placeholder（暫存結果），由排程中的 I/O 或 wait 完成處理替換後才真正返回。
exec 成功時 dispatcher 立即返回，不能把正常返回值 0 再寫進 frame->a0，否則會破壞新程式所需的初始角色參數。失敗才按一般規則回傳錯誤。
filesystem syscall 先複製 NUL 結尾路徑，資料透過可回報錯誤的 user_copy_from／user_copy_to 逐頁處理；過程可能按需配置或從 swap 換入。寫檔前完整取得來源資料，讀取失敗不繼續用不完整 staging 覆寫磁碟。user/api.c 的 syscall3 使用固定暫存器與 memory clobber 執行 ecall，提供各系統呼叫的使用者包裝函式。
錯誤碼包括 -5 磁碟 I/O、-10 沒有直接子行程、-11 PCB／pipe 資源不足、-12 記憶體／交換資源不足、-14 非法記憶體／權限、-16 終端忙碌、-22 非法參數、-32 管道無讀者、-38 未知呼叫。

## 11. user/shell.c、demo.c、app.c、fan.c、print.c

shell 的 memory_command 提供 mem alloc／write／read／free：sbrk 保留頁數；以 volatile 指標填寫整頁及查看首／中／尾 bytes，讓一般互動能觸發真實硬體缺頁。vmstat 與 vmstat limit 呼叫核心診斷介面。這些是使用者可操作工具，不含自動斷言或測試程式。

shell_main 顯示提示、read 一行、tokenize（切分命令）、command（分派），全程在 U-mode 執行。
每次提示前 yield 兩輪，足以讓本版 worker（自身先 yield 一次）到達等待點，避免背景文字蓋過新提示；不是通用工作控制或時間片。
tokenize 原地將空白／tab／換行及 | 分開，最多 4 個 token（詞元）。只有 writer、|、reader 的三詞組合啟動固定管線，其他組合不會被當作主機命令。
positive_number 檢查每個十進位數字與 LONG_MAX 溢位；wait_child 顯示結果，run_pipeline 前景等待兩個端點行程。
help、ps、pcb、spawn、fork、run、yield、wakeup、wait、kill、trace、固定管線、ls、mkdir、touch、cat、write 與 exit 的操作詳見 README。`cat` 顯示最多 2048 bytes；若非空輸出最後沒有 LF（換行字元），shell 會補一個換行，避免提示字元緊接檔案內容；不修改檔案本身。

fork_shell 先增加全域 fork_sequence，再 fork。子行程修改自己的全域資料及區域 stack marker，印出子返回值 0，exit(0)；父顯示子 PID，主動 yield 讓子先修改，再顯示自己仍保留的原資料／堆疊值，留下子行程給 wait 回收。這是正常互動命令，不含測試斷言。
run_program 將 worker／quick／counter 轉成 ID，先 fork，只有子分支呼叫 exec；成功不回來，失敗印錯誤並 exit(127)。父立即返回命令列，由使用者觀察、喚醒、終止或回收。

`run_fan(path)` 採前景 fork → exec_arg(PROGRAM_FAN, path) → wait；父 shell 等編輯器退出後自動回收，恢復終端顏色、游標與替代畫面。exec 的 a1 可傳 fan 的路徑指標；dispatcher 在替換前複製並驗證舊地址字串。process_exec 把路徑放在新使用者 stack 的最高 128 bytes，a1 指向它，sp 從此位置向下生長；不是完整 argc／argv 或任意程式載入。

fan.c 的 user_main(program, path) 開檔時讀最多 2049 bytes，超過 2048 或含非 ASCII／LF／Tab bytes 時直接拒絕，不寫回。不存在的檔案先在 RAM 編輯，儲存時才建立。buffer 固定 2049 bytes，插入／刪除搬移後段 O(N)，cursor 是 byte offset，line_start／line_end 與 vertical 掃描換行決定移動，上下移動保留目標欄。

`decode` 用狀態機逐 byte 處理 CSI／SS3 按鍵序列；`handle` 實作一般編輯與未儲存退出確認。`render` 計算垂直／水平視窗，保留 78×20 編輯區，逐行比對畫面快取後只重畫有變動的行，Ctrl+L 強制完整重畫。`save` 對 WUFS 整檔覆寫，完整成功才清除 dirty；失敗保留緩衝區及錯誤訊息。實作與限制見 SPEC-09。

print.c 的 user_write_all 迴圈處理短寫；user_puts 掃描字串；user_number 以除 10 與反轉輸出十進位，支援超過 9 的 PID。print.h 宣告共用輸出，shell.h 只宣告 shell 入口。
命令解析 O(L)，L 最多 126；不支援引號、一般參數、多段管線或通用檔案描述號。

demo.c 的 user_main 只進入 shell；app.c 是另一份獨立映像，依初始 a0 角色分派：1 worker（yield 後 sleep(7)，醒後 exit 0）、2 quick（exit 40）、3 writer、4 reader、5 counter。
counter 先等待通道 9，再讀寫自己初始化為 3 的 volatile 全域 remaining，每次輸出後減一並 yield，至 0 結束。兩份 counter 各自從 3 開始，展示初始化資料與交替執行；沒有硬體時間片。
writer 正常輸出 80 bytes，以 user_write_all 處理管道短寫；reader 每次最多讀 12 bytes，等 EOF 後將完整訊息輸出到 console。接收緩衝區有界，不能無限制累積資料。
user/entry.S 保留初始 a0／a1 呼叫 user_main，返回值再交給 exit；exit 如果意外返回就在使用者模式迴圈。

## 12. Linker 與 Makefile

kernel/linker.ld 由 0x80200000 配置 .text（程式碼）、.rodata（唯讀資料）、.data（有初值資料）、.bss（零初始化資料）、啟動 .stack；前 2 MiB 保留給 firmware。
程式碼與唯讀區終點對齊 4 KiB，__text_end／__rodata_end 供 vm 區分 RX／R／RW。__bss_start／end 供啟動歸零，__stack_top 供啟動 stack，__kernel_end 為映像保留終點。
NOLOAD 不攜帶初始化內容但保留記憶體，KEEP 保護入口，ALIGN 決定對齊；限制核心不超過 0x80400000，保留低位址核心開機大小預算。RAM 映射現由多張 L0 動態建立，不再受單張 L0 的 2 MiB 覆蓋範圍限制。

user/linker.ld 的入口固定 0x400000，code／唯讀資料最多 16 KiB，data／BSS 從 0x404000 開始最多 4 KiB；ASSERT（連結期限制）防止超出。stack 由核心映射，不在 user.bin 中。
Makefile 將 user/entry.S、api.c、print.c、demo.c、shell.c 連結為 user.elf；entry.S、api.c、print.c、app.c 連結為 app.elf；entry.S、api.c、print.c、fan.c 連結為 fan.elf。三者各有自己的 user_main，但不互相連結。llvm-objcopy 轉成 user.bin／app.bin／fan.bin，kernel/user_image.S 的 incbin 嵌入核心唯讀資料，vm 依程式 ID 複製映像。不是 ELF parser（執行檔解析器）。
`make run` 先呼叫 tools/make_disk.py 建立或更新 `build/wuwaos.img`，再以 `-drive` 與 `virtio-blk-device` 接上 QEMU。Python 工具用相同 WUFS metadata layout 建立空卷、Linux 風格目錄與 `/boot/kernel.elf`；辨識已格式化映像後只更新 kernel inode，保留其他檔案。`make clean` 特意保留磁碟映像。
binary 以 0x400000 為起點，若有初始化 .data，會保留到 0x404000 的位址間距；vm 將前 16 KiB 放 code，其後放 data。BSS 不攜帶映像內容，page_alloc 配置零頁保證其初值。

Clang 交叉編譯為 rv64imac_zicsr_zifencei，ABI 為 lp64，LLD 連結；不依賴標準函式庫、浮點呼叫慣例或動態載入器。
-mcmodel=medany 適用高位址核心；-mno-relax／-msmall-data-limit=0 避免 gp 依賴；-ffreestanding／-fno-builtin／-nostdlib 用於裸機執行環境；-fno-pic／-fno-pie 固定位址。
-O2 最佳化、-g 除錯資訊、-Wall -Wextra -Werror 將警告視為錯誤；-MMD -MP 追蹤標頭相依。未來 C 寫法若產生 memcpy 等呼叫，仍需提供實作。
all 建置、run 啟動、clean 清理 build 產物。更改 flags（編譯選項）後需 clean 再建置；$< 是第一個相依項，$@ 是目標，| build 先確保目錄存在。

## 13. 驗證與閱讀練習

早期行程驗收與 QEMU monitor 證據在 SPEC.md；VirtIO／WUFS 磁碟重啟持久性在 SPEC-06；FIFO 與 Ready 行程移除驗收在 SPEC-07；動態頁面／三級表的初期驗收在 SPEC-08，fan 互動操作在 SPEC-09，現行按需分頁／Swap／回收及失敗路徑在 SPEC-10。ps／pcb 是讀取真實 PCB 與就緒佇列；pipe 的 80 bytes 必須通過 16-byte 緩衝區與多次狀態切換。
未完成 runtime 驗證的邊界不可等同已測試；沒有新增未經同意的測試程式碼。
可先照 README 操作 worker，對照 saved sepc（保存指令位置）說明恢復後從哪裡繼續；再追蹤 writer 滿管道時的 buffer 位址如何留在 PCB。

仍無計時器搶占、多核心、通用檔案描述號、任意執行檔與前景 Ctrl+C 取消。若 shell wait 一個永遠不會收到事件的子行程，也無其他行程能通知，會阻塞；先 wakeup 再 wait。

## 官方參考

- [RISC-V Supervisor 規格：特權暫存器與 Sv39](https://docs.riscv.org/reference/isa/priv/supervisor.html)
- [QEMU virt 與 OpenSBI](https://www.qemu.org/docs/master/system/riscv/virt.html)
- [LLVM RISC-V target](https://llvm.org/docs/RISCVUsage.html)
- [Linker script](https://www.sourceware.org/binutils/docs/ld/Scripts.html)
