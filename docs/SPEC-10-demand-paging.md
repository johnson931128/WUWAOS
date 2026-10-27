# Spec 10：Demand paging、FIFO 頁面置換與 Swap

本文件保留當時的實作與驗收紀錄；後續共用磁碟請求已加入 5 秒逾時診斷並停止核心，見 [Spec 12](./SPEC-12-block-timeout.md)。下方「沒有裝置超時」是本階段歷史限制。

## 目標與範圍

完成基本教科書缺頁路徑：判斷合法位址與權限、取得頁框、載入映像／零頁或 swap、更新 PTE／TLB、重新執行原指令；頁框不足時以全域 FIFO 換出使用者頁面。保留單 hart、128 MiB、Sv39 4 KiB 頁面及既有 shell／fan／WUFS。未經同意不新增測試程式碼；以正常互動命令與 QEMU 檢查驗收，不自行 commit／push。

## 設計

- 補充分頁資料表記錄每個使用者頁的實體頁、FIFO 入駐次序、swap slot、dirty 與暫時 pin（禁止換出）狀態。合法區域為既有 16 KiB RX code、4 KiB RW data、4 KiB RW stack，以及 sbrk 取得的 RW heap；其餘或權限不符為非法存取。
- 每個行程最多 256 個 heap 頁（1 MiB），起點 0x1000000。sbrk 只保留位址範圍，實際頁面在第一次讀寫時清零配置。縮小 heap 釋放整頁；部分頁的剩餘內容不承諾重新清零。核心頁面與分頁表常駐，不參與置換。
- 建立／exec 不預先配置使用者實體頁。code／data 第一次存取從唯讀的核心內嵌映像填入；stack／heap 首次存取為零頁。從無到有建立 PTE 後 sfence.vma，必要時同步指令；缺頁成功不增加 sepc，重試原指令。
- 使用者頁全域常駐預算預設 64 頁（256 KiB），可用 vmstat limit 調整，最小 8 頁。這是可調整的頁框配置政策，QEMU 總 RAM 仍 128 MiB；一般實體配置器耗盡時也嘗試回收使用者頁。
- FIFO 以遞增入駐序號及掃描補充表選最老可換頁，命中不更新次序；暫時 pin 的頁面不換出。缺頁時也 pin 住重試指令可能占用的兩個 code 頁，避免 code／operand 互相淘汰而在 swap full 時無限重試。clean 且有原始映像／零頁可重建者直接丟棄；已存在 swap backing 的 clean 頁直接保留 slot；dirty 頁完整寫入 8 個磁區後才撤銷 PTE。若某候選頁無法安全寫出，嘗試其他可換頁，沒有候選者則回報資源或 I/O 錯誤。
- 第二顆 VirtIO block 預設為專用 16 MiB swap 映像，前 8 個磁區保留標頭，4095 個 4 KiB slots，以 bitmap 配置；make_swap.py 的 --slots 可建立 1–4096 個 slot 的交換區，既有檔案不重格式化。以 WUSWAP01 標記識別，不依賴 FDT 掃描或裝置插槽順序；WUFS 與交換區資料分離。交換區不保存重啟前行程，啟動時 slot 使用狀態從空開始。缺少 swap 仍可按需配置與丟棄 clean 頁，無法換出 dirty 頁時回報失敗。
- 換出寫入失敗保留原 RAM 頁與有效 PTE；換入讀取失敗不公布不完整頁、不清除 swap slot。swap full／無可換頁／RAM 不足與磁碟錯誤均可向上回報；非法缺頁 -14、資源不足 -12、磁碟錯誤 -5 結束行程。無裝置回應的超時機制仍不在此同步輪詢驅動範圍。
- kernel copyin／copyout 檢查的是合法使用者範圍，逐頁解析時可觸發按需載入／換入；失敗明確回傳，檔案寫入前完整取得來源資料。fork 複製私有內容，包括已換出的頁；來源實體頁在複製期間 pin，換出內容複製到子行程獨立 slot，不共享可寫 backing。exec 先準備可啟動的新空間與路徑參數，成功後才替換；exit／wait／孤兒回收釋放使用者頁、swap slots 與私有表。
- vmstat 顯示硬體缺頁、核心代載入、首次映像／零頁、換入換出、clean drop、resident、slot 使用量與錯誤；pcb 顯示個別位址空間狀態。mem alloc/write/read/free 是可互動的使用者記憶體配置／填寫／查看／釋放工具，不含自動測試或斷言。

## 執行步驟與驗收

1. 區塊驅動支援兩顆裝置與格式識別，建立獨立 swap 映像工具。
2. 實作 swap slot 與整頁讀寫；重構 VM 為補充表、延遲載入、FIFO 與可回報錯誤的核心複製。
3. 串接硬體缺頁、sbrk、fork／exec／回收與 I/O；新增 vmstat／mem 互動介面。
4. 編譯與 QEMU 實際開機。保留原有磁碟，使用獨立 WUFS／swap 檔操作。
5. 觀察未存取 heap 不占用實體頁；首次讀零、寫入、超過常駐預算產生換出，再讀回首／中／尾資料。縮小及行程回收後 slots／頁面歸還。
6. 置換壓力下驗證 fork 的獨立性、exec、fan、pipe 與磁碟重啟保存；檢查非法位址／權限錯誤、swap unavailable／full／I/O 失敗可實際驗證的範圍，未執行者明確記錄。

## 觀念來源與限制

- `notes/ch10 Virtual Momory.md` 的缺頁處理 Step 1–6、§10.4.2 FIFO（約 2200 行）及 dirty／頁框不足處理。教材概念對應為合法性檢查、free frame／victim、backing store、更新表與重試。
- [RISC-V privileged specification](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-privileged.pdf)：scause 12／13／15、stval、PTE A/D 與 sfence.vma。
- 不加入 COW、共享 mmap、檔案映射、ELF loader、動態核心 heap、多 hart、非同步磁碟排程或所有置換演算法；FIFO 是第一個可使用的頁面置換策略。

## 驗收紀錄

2026-10-26，WSL Ubuntu、Clang 18、QEMU 8.2.2、OpenSBI 1.3；virt／單 hart／128 MiB。使用既有 shell／app／fan、mem 互動工具及 QEMU 偵錯介面，沒有新增測試程式或自動斷言。

- 最終 `make clean && make` 通過 -Wall/-Wextra/-Werror；kernel.elf／user.elf／app.elf／fan.elf 的 llvm-nm -u 無未解析符號，git diff --check 通過。make -n run 確認會建立獨立 swap 並接入第二顆 VirtIO block。
- 原有 build/wuwaos.img 未使用或修改；驗證使用 build/paging-check.img、paging-swap.img 與另建的 4-slot paging-small-swap.img。兩顆磁碟都辨識成功；最終輪反轉 QEMU 裝置接入順序，仍依標頭識別 WUFS／swap，檔案可正常讀取。
- 開機 kernel 常駐映射占 67 頁，底層配置池 total=31560、初始 free=31493。shell 首次執行後產生真實取指令／堆疊／資料缺頁；vmstat 初始 resident=4、faults=4、image=3、zero=1。mem alloc 32 後 heap_bytes=131072，未配置 32 個實體頁；額外載入的 shell 工具程式碼使 resident 從 4 到 5。
- mem read 0 首／中／尾回 0；再 write 65 後讀回 65。該頁首次由讀取載入後才由 CPU 寫入，之後換出換回仍為 65，涵蓋硬體 PTE_D 修改狀態的使用。
- 8 頁常駐預算下依序填寫 32 頁，第 0／15／31 頁首／中／尾分別讀回 65／80／96；統計顯示實際 swap_in／swap_out 與 clean_drop。修正版再填寫 24 頁，讀第 0／12／23 頁為 100／112／123，並執行 fork／wait，父資料保留、子 data／stack 101／101、父 1／100；觀察到 swap_fork_copies=23，已換出頁複製到獨立 child slots。
- 8 頁壓力下 run quick／wait 得 40、writer | reader 傳完 80 bytes／EOF 並回收，fan 能開啟／修改／儲存含空白的多行文字。正常兩個 counter 被喚醒後交替 3／2／1，wait 均為 0。
- fan 初次儲存 /home/user/paging.txt 為 39 bytes，關閉 QEMU 後再開讀回；修正版加驚嘆號後為 40 bytes。最後完整重編譯、以 make_disk.py 更新該獨立磁碟的核心副本，再用不接 swap 的 QEMU 啟動，仍讀回 `Demand paging works!` 與 `Swap keeps my text`（兩行皆 LF 結尾）。
- 4-slot 交換區與 limit 8 下填滿 8 個 heap 頁，fork 回 -12，原第 0／7 頁仍為 40／47；失敗子空間回收。進一步耗盡資源時初輪發現 code／operand 互換的無限缺頁，加入重試指令 pin 後重跑：明確輸出 fault cause=15、error=-12，結束 PID 1 並 All processes reaped，不再無限重試。
- swap 磁碟設 readonly=on：fork 回 -5，原頁仍可讀 55／55／55，重新寫入 77 也可讀回，slot 使用量維持 0。資源壓力再提高後 fault 回 -5 並正常終止／回收。這驗證寫出失敗仍保留 RAM 副本；未注入八磁區中途的部分讀取失敗。
- 使用 QEMU GDB remote protocol 暫停 CPU，依 pcb 印出的 frame 位址修改既有 worker 的保存 sepc／sp，再喚醒。未加入任何故障程式碼：跳至未映射 0x600000 觸發 cause 12；使既有 user_main 的堆疊寫入 RX code 觸發 cause 15、stval=0x4000f8；跳至既有 `ld ra, 0x28(sp)` 並將 sp 設為 0x600000，觸發 cause 13、stval=0x600028。均返回 -14、只結束該子行程，shell 能 wait 回收並繼續操作。另一次非法 stack store 同樣 -14，最終 invalid_faults=4 均為刻意偵錯操作。
- 最終一般 shell exit 後，QEMU monitor 確認 resident=0、swap used=0、free_count=0x7b05（31493），回到只剩核心常駐映射的基準。唯讀 swap 的 fatal path 亦確認 resident／used 歸零。不接 swap 時仍能開機、按需配置零頁、sbrk 縮小、fork／exec／wait；正常操作 invalid_faults=0、io_errors=0。
- 紀錄位於忽略的 build/paging-console.log、paging-full-fixed-console.log、paging-readonly-fixed-console.log、paging-final-console.log、paging-noswap-console.log；monitor／GDB 操作見本對話工具輸出。所有 QEMU 已結束。

限制與未驗證：置換／資源不足使用可調常駐預算與小交換區驗證，沒有真的耗盡整個 128 MiB 實體配置池；未注入 swap read 的部分磁區失敗、磁碟無回應或多核心競爭。驅動仍是同步輪詢，缺頁磁碟 I/O 期間不切換到其他行程，沒有裝置超時。使用者 heap 有 256 頁上限，沒有 mmap、COW、共享頁與任意磁碟 ELF 載入。空 heap L0 表保留到整個位址空間銷毀，已換回頁可保留 backing slot，不能把這些仍屬行程的資源誤判為洩漏。
