# Spec：第一個使用者行程與基本系統呼叫

## 目標與範圍

在現有 RV64 / QEMU virt / OpenSBI 的 S-mode（監督模式）啟動基礎上，建立一個具有隔離位址空間的 U-mode（使用者模式）行程，完成 write、getpid、exit 三個 syscall（系統呼叫）。
本規格彙整使用者要求與後續討論；本次完成實作及基本功能驗證後 commit / push 到目前 branch。
不加入排程、多核心、檔案系統、動態記憶體配置器、ELF loader（執行檔載入器）或測試框架。
未經同意不加入測試程式碼；user/demo.c 是本次實際執行的使用者示範程式，不加入負面測試程式。

## 設計

- trap（中斷／例外造成的控制權轉移）採 Direct mode（共同入口模式）。
- stvec（監督模式陷阱向量暫存器）指定 trap.S；組合語言保存一般暫存器及相關控制暫存器，再呼叫 C handler（處理函式）。
- sscratch（監督模式暫存用途暫存器）在 U-mode 存放核心堆疊頂端，在核心執行時為 0；用於切換堆疊。
- Sv39（三級分頁）以固定 4 KiB 頁面映射核心、UART（序列埠）、使用者程式碼、資料與堆疊。核心以相同虛實位址映射。
- 使用者程式碼位址 0x400000、資料 0x401000、堆疊 0x800000–0x801000；三頁各有獨立實體儲存區。
- 使用者程式碼為 RX（可讀／可執行），資料與堆疊為 RW（可讀寫）；核心及 UART 的頁面不帶 U（使用者可存取）權限。
- 一個 Process（行程資料結構），PID（行程識別碼）為 1，狀態為 Ready / Running / Exited（就緒／執行中／已結束），記錄頁表、堆疊與 TrapFrame（陷阱執行狀態）。
- user 程式獨立編譯成固定布局，轉成 binary（原始機器碼映像）後嵌入核心。最多兩頁，不解析 ELF。
- sret（監督模式陷阱返回指令）進入 U-mode，或返回被系統呼叫暫停的程式。
- 建置禁用 linker relaxation（連結指令改寫），不依賴 gp（全域資料指標）；使用者浮點／向量狀態停用，只執行整數指令。

## 系統呼叫介面

a7（參數暫存器）放呼叫編號，a0–a2 放參數，a0 放回傳值。這是 WUWAOS 自訂介面，不是 Linux ABI（呼叫介面）。

| 編號 | 介面 | 結果 |
| --- | --- | --- |
| 1 | write(fd, buffer, length) | 只支援 fd=1，成功回傳 byte 數；0 長度回傳 0 |
| 2 | getpid() | 回傳 1 |
| 3 | exit(status) | 記錄狀態、標記 Exited，輸出訊息後核心等待，不返回使用者 |

錯誤：-22 為不合法參數，-14 為不可讀的使用者記憶體，-38 為未知系統呼叫。
write 在輸出前檢查整段 buffer 的使用者頁面讀取權限、跨頁與整數溢位；由核心映射的實體頁讀取，避免直接信任使用者指標。
僅確認為 U-mode ecall（使用者環境呼叫指令，原因編號 8）時，返回位址 sepc 加 4；不對所有例外套用相同處理。
首次進入與每次 syscall 的來源模式、呼叫編號、回傳值記入 console（終端輸出），方便理解執行流程。
未處理的核心例外顯示原因、指令位址與額外資訊後停止；使用者例外終止該行程。

## 執行步驟

1. 抽出 UART 共用輸出，加入 trap 保存／返回流程與診斷。
2. 建立固定 Sv39 頁表與使用者記憶體讀取介面。
3. 建立單行程及 U-mode 初始狀態。
4. 加入 syscall 分派、使用者 wrapper（呼叫包裝函式）與示範程式。
5. 更新 Makefile、README、CODE_GUIDE 及本規格的實際驗收紀錄。
6. 編譯並在 QEMU 觀察完整流程；確認提交範圍後 commit / push。

## 驗收條件

- 建置無警告或未解析符號；正常啟動訊息仍存在。
- 真正收到 U-mode ecall，而非核心直接呼叫示範函式。
- getpid 回傳 1，write 回傳正確長度，示範程式使用回傳值決定 exit 狀態。
- exit(0) 將行程標記 Exited，後續不返回使用者程式。
- 檢查頁表配置確認核心／UART 不可被 U-mode 存取；使用者程式碼不可寫，資料／堆疊不可執行。
- 所有程式碼檔案在 CODE_GUIDE 中有功能、硬體關係與演算法說明，術語附中文用途。
- 不加入未授權測試程式碼。未執行的負面路徑必須明確標示。

## 實際驗收紀錄

2026-10-12 完成，使用 WSL Ubuntu、Clang 18.1.3、LLD、QEMU 8.2.2、OpenSBI 1.3。

- clean build（完整重新建置）成功，-Wall -Wextra -Werror 無警告；kernel.elf 與 user.elf 無未解析符號。
- 核心入口為 0x80200000，使用者入口為 0x400000。
- QEMU console 顯示 Ready → Running → Exited；getpid 回傳 1，write 輸出 `[user] Hello from PID 1` 並回傳 0x18（24 bytes），exit 狀態為 0。
- QEMU 的硬體陷阱紀錄有三次原因 8、desc=user_ecall，指令位置依序為 0x400022（getpid）、0x400014（write）、0x40002e（exit）；與使用者程式反組譯一致。
- QEMU monitor（虛擬機檢查介面）的 info mem 顯示：使用者 code 為 r-xu、data／stack 為 rw-u；UART 與核心頁沒有 u 權限，核心 code 為 r-x、唯讀資料為 r--。
- monitor 讀取實際 Process 記憶體，PID=1、state=2（Exited）、exit_status=0。CSR（控制與狀態暫存器）中 satp 的模式為 Sv39，scause=8，sscratch=0（已在核心）。
- 最後調整使用者浮點／向量狀態後，重新完整建置並執行 QEMU，三個呼叫與 exit(0) 結果仍成功。
- 驗證期間核心結束後保持等待；最後一次執行由主機 timeout 在 5 秒後停止，這是預定清理動作，不是核心失敗。
- 本機 build/qemu-console.log 與 build/qemu-traps.log 保存最後一次輸出，build/ 不納入提交。可用 README 的命令重跑。

驗收限制：頁面隔離已從執行中的實際頁表核對，未加入故意讀寫核心／UART 的負面測試程式；非法 buffer、未知 syscall、非 syscall 例外的 runtime 路徑尚未執行。這些分支完成程式碼檢查，不宣稱已完成動態驗證。
未加入測試框架或負面測試程式碼。下一個小步驟為第二個固定行程與 yield（主動讓出 CPU）的合作式排程。
