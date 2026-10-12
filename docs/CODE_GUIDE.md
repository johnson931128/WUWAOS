# WUWAOS 程式碼與架構紀錄

本文件記錄目前所有手寫程式碼、標頭、連結與建置設定。功能範圍及驗收紀錄以 SPEC.md 為準；新增檔案或行為時同步更新本文件。

## 1. 整體架構與硬體流程

平台為 RV64、QEMU virt、單核心、128 MiB RAM。CPU 執行的是記憶體中的機器碼，不知道 .c 或 .S 檔名。

```text
QEMU 載入 OpenSBI 與 kernel.elf
  → OpenSBI 在 M-mode（機器模式）初始化平台
  → entry.S：_start 在 S-mode（監督模式）準備 stack／BSS
  → main.c：kernel_main
      → trap_init：登記共同例外入口
      → vm_init：建立頁表與使用者映像
      → process_init：準備第一個行程
      → process_start：啟用分頁
      → user_enter → sret → U-mode（使用者模式）
          → user/entry.S → user_main → getpid / write / exit
          → ecall → CPU 記錄原因與指令位址，跳到 stvec 指定的入口
          → trap.S：換核心 stack、保存暫存器
          → trap.c → syscall.c
              ├─ getpid / write：還原狀態 → sret → 使用者繼續執行
              └─ exit：記錄行程結束 → 核心等待
```

OpenSBI 是外部 firmware（底層平台韌體），不是本 repository 的程式碼。
核心提供軟體入口位址與頁表；CPU 依暫存器設定進行跳轉、位址轉換與權限檢查。

## 2. 所有程式碼檔案

| 檔案 | 功能 |
| --- | --- |
| kernel/entry.S | 最初入口、關閉 supervisor interrupts（監督模式中斷）、設定 stack、清除 BSS |
| kernel/main.c | 初始化流程，交接給第一個行程 |
| kernel/console.c / console.h | UART（序列埠）輸出、十六進位格式、核心等待 |
| kernel/trap.S | 共同陷阱入口、保存／還原暫存器、進入使用者模式 |
| kernel/trap.c / trap.h | 陷阱初始化、原因判斷、TrapFrame（執行狀態結構）與共用位移 |
| kernel/vm.c / vm.h | 固定 Sv39（三級分頁）映射、使用者位址轉譯與讀取檢查 |
| kernel/process.c / process.h | 單行程資料、初始執行狀態、啟動與結束 |
| kernel/syscall.c / syscall.h | 系統呼叫分派、write / getpid / exit |
| kernel/user_image.S | 把使用者 binary（原始機器碼映像）嵌入核心唯讀資料 |
| kernel/linker.ld | 核心入口、區段、權限邊界與 stack 布局 |
| include/syscall_numbers.h | 核心與使用者共用的系統呼叫編號及錯誤碼 |
| user/entry.S | 使用者入口，呼叫 user_main，再用返回值呼叫 exit |
| user/api.c / api.h | 使用者系統呼叫 wrapper（包裝函式） |
| user/demo.c | 第一個使用者工作負載：取得 PID、輸出、結束 |
| user/linker.ld | 使用者程式碼與資料的固定位址、大小限制 |
| Makefile | 交叉編譯、映像嵌入、連結、啟動與清理 |

AGENTS.md 是協作規則，README.md 是執行方式；docs/SPEC.md 是規格，這份文件是程式碼紀錄；.gitignore 排除 build/。
標頭 .h 定義型別、常數與函式契約，本身不另產生執行流程。build/ 是建置及驗證產物，不是額外的手寫程式碼。

## 3. kernel/entry.S：準備 C 的執行環境

一般 C 應用程式的 main 前面已有 runtime（執行環境）準備；自己的核心必須先準備可用 stack（堆疊）與零初始化資料。
大寫 .S 表示先經過 C preprocessor（前置處理器），再組譯。

### 入口與中斷

`.section .text.entry` 將機器碼放在入口區段；`.globl _start` 讓 linker（連結器）看見入口符號。
`_start:` 是位置標籤，不是 CPU 指令。kernel/linker.ld 使用 `ENTRY(_start)` 指定它。

`csrw sie, zero` 清除 sie（監督模式中斷啟用暫存器）。
`csrci sstatus, 2` 清除 sstatus（監督模式狀態暫存器）的 SIE（中斷全域開關）bit。
這不會禁止同步例外，也不代表關閉所有 M-mode 中斷。本版本尚不啟用 supervisor 中斷。

### Stack 與 BSS

`la sp, __stack_top`：把 linker 定義的頂端位址放入 sp（堆疊指標），stack 向低位址成長，預留 16 KiB、16-byte 對齊。
`la t0, __bss_start`、`la t1, __bss_end`：取得 BSS（零初始化全域／靜態資料區）的起迄位址。
`la` 是載入位址的偽指令，會轉成實際機器指令；這些 __ 符號是 linker 的位址，不是 C 變數。
`.option push / norelax / pop` 保存組譯選項、禁止這段連結最佳化再還原；不操作 runtime stack。Makefile 也全域禁用 relaxation（連結指令改寫），目前不依賴 gp（全域資料指標）。

歸零演算法：

```text
t0 = BSS 起點
while t0 < BSS 終點（unsigned 比較）:
    將 t0 指向的一個 byte 寫成 0
    t0 += 1
```

對 B 個 byte 耗時 O(B)，額外空間 O(1)。範圍是左閉右開，不清除獨立的啟動 stack；目前頁表、使用者緩衝區、行程資料與行程核心 stack 都位於 BSS，會一起歸零。
`1b` 指向後方的數字標籤 1，`2f` 指向前方的標籤 2。

### 進入 C

`call kernel_main` 是程式主動發起的函式呼叫，設定 ra（返回位址暫存器）並跳到 main.c 的函式。
此時 stack 與 BSS 已準備。kernel_main 正常不返回；若返回，entry.S 的 WFI（等待中斷指令）迴圈避免繼續執行未知內容。
WFI 不是關機，也不保證永遠停止；外層迴圈處理它恢復執行的情況。

## 4. kernel/main.c 與 console.c

`kernel_main` 依序輸出啟動文字、安裝 trap 入口、建立頁表與使用者映像、建立行程、啟動行程。不再把全部功能放在同一個檔案。

console.h 的四個函式由 console.c 實作：

- `console_putc`：輪詢 UART 的 LSR（線路狀態暫存器）bit 5，待可寫入後將 byte 寫至 offset 0；遇到換行先輸出 CR（回車）。
- `console_puts`：直到字串的零字元，逐字輸出；N 個字元掃描 O(N)，另加裝置等待時間。
- `console_hex`：輸出 0x 與固定 16 位十六進位數字，不需要 printf 或標準函式庫。
- `kernel_idle`：永久 WFI 迴圈，用於行程結束及致命錯誤。

UART 基址為 0x10000000、LSR offset 為 5。使用 volatile（要求編譯器保留存取）指標讀寫 MMIO（記憶體映射裝置暫存器）。volatile 不是鎖，也不是通用記憶體屏障。
這是 polling（輪詢）演算法，額外空間 O(1)，等待沒有 timeout；UART 初始化仍借用 firmware 的設定。

## 5. kernel/trap.S、trap.c、trap.h：硬體事件到 C 處理

### 硬體暫存器

| 名稱 | 中文用途 |
| --- | --- |
| stvec | 監督模式陷阱向量暫存器，指定入口位址及模式 |
| scause | 監督模式陷阱原因暫存器，最高 bit 區分中斷／例外，其餘 bit 表示原因 |
| sepc | 監督模式例外程式計數器，記錄相關指令位址 |
| stval | 監督模式陷阱額外資訊暫存器，可能提供錯誤位址等資訊 |
| sstatus.SPP | 記錄進入陷阱前的模式；0 為 U-mode、1 為 S-mode |
| sscratch | 監督模式暫存用途暫存器，本核心用它保存陷阱所需的核心 stack 頂端 |

`trap_init` 登記 4-byte 對齊的 trap_entry，低位為 0，因此採 Direct mode（共同入口模式），不是 Vectored mode（不同中斷按原因位移的向量模式）。設定 sscratch=0，表示當下在核心。
送往 S-mode 的 trap 發生時，CPU 更新原因／指令位址／模式並跳至 stvec。CPU 不會自動保存所有一般暫存器，也不會自動安排核心 stack。
哪些事件交給 S-mode 仍受 OpenSBI 的 delegation（陷阱委派）設定影響。

### Stack 切換與保存

`trap_entry` 第一條 `csrrw sp, sscratch, sp` 交換 sp 與 sscratch。

- 來自 U-mode：sscratch 原本是核心 stack 頂端；交換後 sp 成為核心 stack，sscratch 成為舊使用者 sp。
- 來自核心：sscratch 原本為 0；第一次交換後 sp=0，所以再交換一次，回到原核心 stack。

在 stack 預留 288 bytes，保存 x1–x31 一般暫存器、零值 x0、sepc、sstatus、scause、stval。
原始 sp 在切換後另行取得並保存。取得舊 sp 後清除 sscratch，讓 C handler 裡再發生陷阱時被識別為核心來源。
`mv a0, sp` 將 TrapFrame（保存結構）位址作為參數，`call trap_handler` 進入 trap.c。

trap.h 用共用的數字位移供組合語言使用，C 結構則定義相同布局。`_Static_assert`（編譯期布局契約）檢查大小及位移，避免兩邊不一致；不加入 runtime 測試程式。

### 分派、返回與初次進入使用者

trap_handler 只有在 SPP=0 且 scause=8（使用者 ecall）時呼叫 syscall_dispatch。
ecall 是固定 4-byte 指令，因此先將保存的 sepc 加 4，讓正常返回落在下一條指令。
其他 trap 輸出原因、指令位置及額外資訊；使用者例外終止行程，核心 trap 停止。尚未處理 timer 中斷。

`trap_restore` 把 sepc／sstatus 寫回 CSR（控制與狀態暫存器），依返回模式準備 sscratch，還原一般暫存器，最後用 `sret`（監督模式陷阱返回指令）交還 CPU。
`user_enter` 將初始 TrapFrame 位址放進 sp，再使用同一份還原流程；第一次的 frame 也位於行程核心 stack 的頂端下方，因此可以復用。
保存／還原固定數量資料，時間與空間皆 O(1)。本版本不保存浮點／向量狀態，初始使用者狀態停用相關能力。

## 6. kernel/vm.c、vm.h：頁表與指標檢查

Sv39（三級分頁）把虛擬位址拆成三個 9-bit 索引與 12-bit 頁內 offset（偏移）。每份表有 512 個 PTE（頁表項目），表對齊 4 KiB。
vm.c 靜態預留 root（根表）、兩份第二級表與四份第三級表；沒有動態配置演算法。

| 虛擬位址 | 實體儲存來源 | 權限 |
| --- | --- | --- |
| 0x400000–0x401000 | user_code，4 KiB | 使用者 RX（可讀／可執行） |
| 0x401000–0x402000 | user_data，4 KiB | 使用者 RW（可讀寫） |
| 0x800000–0x801000 | user_stack，4 KiB | 使用者 RW |
| 0x10000000 的一頁 | UART 同位址 | 僅核心 RW |
| 0x80200000 到 __kernel_end | 核心同位址 | 程式碼 RX、唯讀資料 R、其餘 RW；全部僅核心 |

所有範圍為左閉右開。使用者記憶體的實體儲存區還有僅核心可存取的 identity mapping（虛實位址相同映射），供核心讀取資料。

`table_entry` 生成下一級表的 PPN（實體頁編號）與 V（有效）bit；`leaf_entry` 加上 R/W/X/U（讀／寫／執行／使用者）權限及 A/D（已存取／已寫入）bit。
預設表全部為 0；未映射位置不可存取。使用者頁以外不設 U bit，因此使用者不能直接存取核心或 UART。

`vm_init` 先把嵌入的使用者映像複製到 code／data 實體頁，再建立固定映射。使用者資料的未初始化部分與 stack 已由 BSS 清除。
映像複製 O(I)，核心映射 O(K)，I 為映像 byte 數，K 為核心頁數；表與使用者記憶體大小固定。

`vm_enable` 將 Sv39 模式與根表 PPN 寫入 satp（位址轉換設定暫存器），使用 sfence.vma（位址轉換同步指令）清理轉換快取，並執行 fence.i（指令取得同步指令）讓 CPU 看見複製的程式碼。

### 使用者 buffer 的安全讀取

- `user_physical`：步行三層頁表，檢查有效、4 KiB leaf（末級映射）、U 和 R 權限，再組合實體位址。固定布局只接受低半部使用者位址與 4 KiB 頁面。
- `user_readable`：0 長度立即成功；先檢查 address + length 的溢位，再逐頁確認整段範圍可讀。P 頁耗時 O(P)。
- `user_read_byte`：將已驗證位址轉成實體位址，經核心 identity mapping 讀取 byte。只供完成整段驗證的呼叫者使用。

核心不直接解參考使用者的虛擬指標，也不開啟 SUM（允許監督模式直接讀寫使用者頁的狀態 bit）。本版本頁表不可被使用者改動，也沒有並行執行，驗證後映射保持不變。

## 7. kernel/process.c、process.h：單行程生命週期

Process（行程）記錄 PID、state（狀態）、exit_status（結束碼）、根頁表位址、兩種 stack 頂端及 TrapFrame 指標。
`current_process` 是唯一行程；`kernel_stack` 靜態配置 16 KiB，使用者 stack 在 vm.c 預留一頁。

- `process_init`：PID=1、Ready（就緒）；將 frame 放在核心 stack 頂端下方，設定使用者 sp 與入口 0x400000。
- 初始 sstatus 保留合理的既有欄位，但清除 SPP（選擇使用者返回模式）、SIE/SPIE（中斷開關）、SUM/MXR（額外記憶體存取能力）、FS/VS（浮點／向量啟用狀態）。
- `process_start`：改為 Running（執行中），啟用頁表，再以 user_enter 進入使用者模式。
- `process_exit`：保存結束碼，改為 Exited（已結束），輸出後永久等待，不返回使用者。

初始化與狀態更新為 O(1)。沒有 process queue（行程佇列）、context switch（行程切換）、回收頁面或建立第二個行程。

## 8. kernel/syscall.c、syscall.h 與共用編號

include/syscall_numbers.h 定義 WUWAOS 自訂 ABI（核心與使用者的呼叫約定），不等同 Linux 的 syscall 編號。

| 編號 | 呼叫 | 結果 |
| --- | --- | --- |
| 1 | write(fd, buffer, length) | fd=1 輸出，回傳 byte 數 |
| 2 | getpid() | 回傳 1 |
| 3 | exit(status) | 終止行程，不返回 |

a7（參數暫存器）存呼叫編號，a0–a2 存參數；正常返回時把結果寫入 frame 的 a0。
syscall_dispatch 用 switch 分派，數量固定；印出 U-mode 來源、編號與返回值，展示兩邊交接。

sys_write 先檢查 fd 及 length 能否用有號 long 表示，再檢查完整使用者範圍，最後逐 byte 輸出。
回傳值計算原始 byte 數，即使 console 為換行額外輸出 CR，也不改變該數字。N bytes 加 P 頁檢查為 O(N+P)，另加 UART 等待時間。
錯誤 -22 為不合法參數，-14 為不可讀位址，-38 為未知 syscall；zero-length（零長度）write 不讀取 buffer。

## 9. user/：獨立的使用者程式

### entry.S 與 api.c / api.h

使用者入口 `_user_start` 呼叫 user_main，C 返回值在 a0，再以它呼叫 exit。sp 已由核心設定，不由使用者入口建立核心 stack。

api.c 的 syscall3 用固定暫存器變數將編號和三個參數放入 a7、a0–a2，執行 ecall，取回 a0。
memory clobber（記憶體可能改變的編譯器宣告）避免編譯器跨越呼叫任意搬動記憶體存取。
write / getpid / exit 封裝這個呼叫流程；exit 若意外返回會在使用者模式持續迴圈，不呼叫 WFI 這種需要權限的指令。

### demo.c

user_main 在使用者 stack 建立文字，getpid 取得 1，填入 PID 字元，write 輸出 24 bytes。
它依實際回傳值決定結束狀態：正常 0、不支援的 PID 1、輸出結果不符 2，再返回 user/entry.S 由 exit 結束。
這是正常的第一個使用者工作負載，不加入故意非法存取、未知 syscall 等負面測試程式碼。

### user/linker.ld 與 kernel/user_image.S

使用者 linker 將入口固定為 0x400000，程式碼與唯讀資料合併在一頁，資料／BSS 位於 0x401000 的下一頁。ASSERT（連結期限制）阻止任何區域超過一頁。
Makefile 將 user.elf 轉成 user.bin；映像不含 runtime stack，BSS 不攜帶初始化內容。
核心 user_image.S 的 incbin（嵌入二進位資料指示）把檔案放進核心 .rodata，並定義 __user_image_start／end 邊界，vm_init 才能複製。
這是固定映像載入，不是 ELF parser（執行檔格式解析器）。

## 10. kernel/linker.ld：核心布局

核心從 0x80200000 開始，前 2 MiB 留給 firmware；這是布局選擇，不代表 OpenSBI 剛好占 2 MiB。
依序配置 .text（程式碼）、.rodata（唯讀資料）、.data（有初值資料）、.bss（零初始化資料）、.stack（啟動堆疊）。

.text 和 .rodata 的終點對齊 4 KiB，定義 __text_end／__rodata_end，讓 vm.c 能按頁區分 RX／R／RW 權限。
__bss_start／end 供 entry.S 歸零；__stack_top 是啟動 stack 頂端；__kernel_end 是整個映像保留空間的對齊終點。
NOLOAD（不攜帶初始化內容）仍保留執行時記憶體；KEEP 保護入口區段；ALIGN 決定對齊；`.` 是連結器位置計數器。
限制核心不超過 0x80400000，是因為本版本只使用一份末級表映射這個 2 MiB 區間。
linker 的 rwx 布局宣告不是硬體隔離本身；CPU 的保護來自 vm.c 建立的 PTE 權限。

## 11. Makefile：建置流程

```text
user/entry.S + api.c + demo.c + user/linker.ld
  → build/user.elf → llvm-objcopy → build/user.bin
  → kernel/user_image.S → build/user_image.o
kernel/*.c + entry.S + trap.S + kernel/linker.ld
  → build/kernel.elf → QEMU / OpenSBI
```

使用 Clang 交叉編譯與 LLD（LLVM 連結器）；ISA（指令集）為 rv64imac_zicsr_zifencei，明確支援 CSR 與指令取得同步操作，ABI 為 lp64，不使用浮點呼叫慣例。
-mcmodel=medany 適用高位址核心；-msmall-data-limit=0 與 -mno-relax 避免依賴尚未初始化的 gp。
-ffreestanding、-fno-builtin、-nostdlib 表示不依賴主機 runtime 或標準函式庫；未來 C 程式若產生 memcpy 等呼叫，仍需提供對應實作。
-fno-pic／-fno-pie 是固定位址；-fno-stack-protector 是尚無保護器 runtime；-O2、-g 提供最佳化與除錯資訊；-Wall -Wextra -Werror 把警告視為錯誤。
-MMD -MP 產生 .d 標頭相依紀錄；更改編譯 flags（選項）仍需 clean 再建置。

all 建立核心；run 先建置再啟動單核心 QEMU；clean 移除 build/ 中指定類型的建置產物。
`$<` 是第一個來源相依項，`$@` 是目前目標，`| build` 先確保資料夾存在，.PHONY 表示動作名稱。

## 12. 驗收與下一步

實際執行證據及未驗證路徑記錄於 SPEC.md。QEMU 硬體紀錄有三次 user_ecall，console 顯示 PID=1、write=24 bytes、Exited status=0；QEMU monitor（虛擬機檢查介面）可顯示實際 CSR、行程資料與頁表權限。
本版本不宣稱完成未知呼叫、非法指標、故意觸發例外的 runtime 驗證；未新增相關測試程式。

尚未實作多行程、timer（計時器）中斷、排程、通用實體頁配置器、動態 spawn（建立行程）、檔案系統或 shell。
下一個小切片可以是第二個固定行程與 yield（主動讓出 CPU）的合作式排程。

閱讀練習：沿著 getpid 追蹤 a7=2、CPU scause=8、frame 的 a0=1、sret 返回；再對照 write 的 stack buffer 如何經頁表轉成實體位址。

## 13. 官方參考

- [RISC-V Supervisor 規格：特權暫存器與 Sv39](https://docs.riscv.org/reference/isa/priv/supervisor.html)
- [QEMU virt 與 OpenSBI 啟動](https://www.qemu.org/docs/master/system/riscv/virt.html)
- [LLVM RISC-V target](https://llvm.org/docs/RISCVUsage.html)
- [Linker script 的區段與記憶體布局](https://www.sourceware.org/binutils/docs/ld/Scripts.html)

官方文件說明通用規則；本專案實際行為以原始碼與執行證據為準。
