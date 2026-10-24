# Spec 02：合作式行程生命週期

## 目標與授權範圍

使用者要求完整的 process（行程）狀態流程，並已選擇合作式版本：spawn 建立、yield 讓出、sleep/wakeup 等待喚醒、exit 結束、wait 回收。
以前一版的 U-mode（使用者模式）、Sv39（三級分頁）與基本 syscall（系統呼叫）為基礎。
前一版規格與驗收保留在 SPEC-01-user-syscalls.md；這份是目前規格。
不加入 timer（計時器）搶占、多核心、外部 ELF（執行檔格式）載入、動態記憶體配置或測試框架。

## 架構與演算法

- 固定 4 個 process table（行程表）欄位，每次建立取得新的、遞增且不重複的 PID（行程識別碼）。
- 每個行程有獨立頁表、使用者 code/data/stack（程式碼／資料／堆疊）及核心 stack。
- 核心映射與 UART（序列埠）映射共用、僅 S-mode（監督模式）可用；使用者程式碼 RX（可讀／可執行）、資料與堆疊 RW（可讀寫）。
- Round Robin（輪流選取就緒行程）合作式排程：從上一次行程的下一個欄位起循環掃描 Ready。普通 write/getpid 不搶占。
- 只在系統呼叫／使用者例外邊界切換；保存 TrapFrame（暫存器狀態），切換 satp（頁表設定暫存器），返回所選行程的 frame。
- 不保存暫停中的核心 C 呼叫堆疊。wait 等待完成後的返回值與結果由核心在該行程被重新選中時填入，再以 sret（監督模式陷阱返回指令）返回。
- sleep 等待事件 channel（正整數通道），不是秒數或 tick（計時單位）。wakeup 喚醒同通道上的所有等待者，回傳人數。
- 單核心且核心執行時中斷關閉，登記等待與交出 CPU 在同一個 syscall 裡完成，不存在中途被另一行程插入的情況。事件不是信箱；先發生的 wakeup 不累積通知。

## 狀態與轉換

| 狀態 | 意義 | 轉換 |
| --- | --- | --- |
| Unused | 可用欄位，沒有活躍行程 | 建立時 → New |
| New | 已保留欄位，準備私有記憶體與 frame | 完成後 → Ready |
| Ready | 等待 CPU | 被選中 → Running |
| Running | 正在執行 | yield → Ready；sleep/wait → Waiting；exit/使用者錯誤 → Zombie |
| Waiting | 等待事件或子行程 | wakeup/指定子行程結束 → Ready |
| Zombie | 已結束，保留 PID、父行程與結束碼 | 父行程 wait 回收 → Unused |

New 是建立期間的短暫狀態，不會被排程。
Zombie 不可再執行；回收只釋放欄位，新的 spawn 再清除私有記憶體及核心 stack。
不能在退出行程仍使用核心 stack 時把該 stack 歸零，也不能先清空當下使用中的頁表。

## 系統呼叫

維持 a7（呼叫編號暫存器）、a0–a2（參數暫存器）、a0（回傳值）的原有約定。

| 編號 | 介面 | 行為 |
| --- | --- | --- |
| 1 | write(fd, buffer, length) | 沿用輸出與可讀範圍檢查 |
| 2 | getpid() | 回傳目前行程 PID |
| 3 | exit(status) | 保留結果、通知等待的父行程，切換到下一個 Ready |
| 4 | spawn(program_id) | 建立子行程；內建 1=等待事件的 worker（工作程式），2=立即完成的 quick worker；回傳新 PID |
| 5 | yield() | 將自己設為 Ready 並讓出 CPU；重新獲得 CPU 時回傳 0 |
| 6 | sleep(channel) | 在通道上等待；喚醒後回傳 0 |
| 7 | wakeup(channel) | 喚醒同通道所有事件等待者，回傳數量 |
| 8 | wait(pid, long *status) | 只等待指定的直接子行程；若已結束則立即回收，否則阻塞；成功回傳子 PID，並寫入結束碼 |

wait 的 status 可為空指標，表示不需要結束碼；否則在阻塞前及實際回寫前檢查完整可寫範圍，支援非對齊與跨頁。
回收後再次 wait 相同 PID 回傳 -10（沒有該子行程）；程序表滿或 PID 用盡回傳 -11；非法參數 -22、非法記憶體 -14、未知呼叫 -38。
內建 program_id 選擇同一個映像的不同入口角色，使用 a0 傳入，並非任意檔案路徑或 fork（複製行程）。

## 父子關係與無就緒行程

- 開機先建立 PID 1，parent_pid=0（無父行程）。
- 父行程結束時，尚存子行程交給仍活著的 PID 1；若 PID 1 不存在或正在結束，設為無父行程。
- 無父行程的 Zombie 由核心回收，避免無人 wait 而永久占用欄位。
- 全部回收後顯示完成訊息並等待。
- 若沒有 Ready、但仍有 Waiting，列出等待狀態並顯示阻塞診斷後等待。本版無 timer／外部喚醒來源，不能自行解除這種死結。

## 變更範圍與步驟

1. 修改 vm.c/h 與共用容量定義，讓每個欄位有獨立位址空間；指標檢查明確傳入目標頁表。
2. 修改 process.c/h，加入狀態轉換、父子關係、排程、等待與回收。
3. 修改 trap.S/c/h，讓 handler 返回所選行程的 frame；系統呼叫分派及 user API 加入五個新介面。
4. 修改正常使用者示範工作負載：建立兩個 worker、yield、sleep、wakeup、wait，再建立 quick worker 示範欄位重用。
5. 更新 README、CODE_GUIDE 與本文件的驗收紀錄，在 QEMU 觀察實際狀態轉換。
6. 不加入未經同意的測試程式碼。故意非法指標、行程表溢出等負面測試不在本次範圍。

## 驗收條件

- 完整建置成功且無未解析符號，原有 write/getpid/exit 在多行程下運作。
- 示範中真實發生 New → Ready → Running、Running → Ready、Running → Waiting、Waiting → Ready、Running → Zombie、Zombie → Unused。
- 至少兩個子行程共存，事件 wakeup 回傳 2，wait 能取得各自 PID 與結束碼。
- wait 同時呈現先阻塞後喚醒，以及子行程已是 Zombie 時立即回收。
- 新 PID 使用已回收欄位，私有記憶體及暫存器重新初始化。
- 全部行程完成且沒有遺留 Zombie；QEMU 陷阱紀錄來源為使用者模式。
- 以實際頁表與執行資料核對隔離配置，不把單純文字標籤當作隔離證據。
- 文件明確區分此次正常流程的實際驗證與未執行的邊界路徑。

## 實際驗收紀錄

2026-10-16，使用 WSL Ubuntu、Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3。

- 完整重新建置成功；kernel.elf 與 user.elf 無未解析符號，編譯啟用 -Wall -Wextra -Werror。
- 正常工作負載實際完成所有列出的狀態轉換。最初存在 PID 1、2、3；wakeup(7) 回傳 2，兩個子行程由 Waiting 回 Ready。
- 父行程等待 PID 2 時進入 Waiting，PID 2 結束後父行程由 Waiting 回 Ready；取得 PID 2／3 的結束碼 20／30。對 PID 3 的 wait 立即回收已存在的 Zombie。
- PID 4 重用原 PID 2 的欄位，私有資料為 0，結束碼 40；父行程阻塞等待並成功回收。
- 父行程最後以 0 結束，所有行程回收，console 出現 `[scheduler] All processes reaped; idle`。
- 最終示範映像的 QEMU 硬體紀錄包含 41 次 user_ecall（使用者環境呼叫），沒有核心／使用者頁面錯誤。firmware 初始化期間的探測例外位於 OpenSBI 範圍，與核心執行階段區分。
- QEMU monitor（虛擬機檢查介面）的實際頁表顯示目前使用者 code RXU、data/stack RWU，核心與 UART 不含 U 權限。
- 讀取三個已使用欄位的末級頁表：同一使用者 code/data 虛擬位址分別指向不同實體頁。此次映像的資料實體位址為 0x8020c000（父行程）、0x80213000（重用欄位的 PID 4）、0x8021a000（PID 3），值分別為 100、0、3。
- monitor 讀取實際 Process 表：四個欄位的 state 都為 0（Unused），保留供診斷的 PID 為 1、4、3、0；PID 4 的舊欄位已完成重建而不是沿用 PID 2 的資料。
- QEMU 經 monitor 的 quit 正常離開。另一次非互動執行由主機 timeout 停止，是核心完成後的預定清理，不是執行失敗。
- build/lifecycle-traps.log 保留最終互動執行的硬體紀錄；本機日誌及產物不納入 Git。

未動態驗證的邊界：孤兒接管／無父行程子行程回收、全部行程阻塞、表滿、PID 用盡、非法 channel／program／指標、跨頁／未對齊 status、重複 wait、非直接子行程、未知 syscall、故意觸發使用者錯誤。這些分支完成程式碼檢查，不宣稱已完成負面測試，也未新增相關測試程式。
