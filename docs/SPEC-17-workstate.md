# Spec 17：workstate 即時行程監測

## 目標與範圍

建立 workstate，全螢幕每秒更新所有有效 Process、CPU 與記憶體統計，按 q／Q 返回 shell。使用者確認即時全螢幕操作。維持單 hart、128 MiB、50 ms RR，不新增測試程式碼。完成 QEMU／網站驗收後依 AGENTS.md commit／push。

## 設計

- workstate 是 shell 內的使用者模式功能，使用 PID 1（或執行該命令的 shell PID），不新增 PCB 或內建程式 ID。監測者自己的時間與記憶體列在 shell；現有上限仍 4 個 PCB。
- kernel/workstate.c 管理 CPU 累計與唯讀資料快照；include/workstate_info.h 定義版本化、固定容量的使用者 ABI，包含所有非 Unused 行程，含 Waiting／Zombie。不輸出核心指標，不允許更改其他行程。
- 每次派發在切回行程前記錄 rdtime，切走前累加 elapsed 到 PCB cpu_ticks 及系統 busy_ticks。普通 syscall、缺頁與核心服務時間在派發期間歸給該行程；阻塞時間不算。已回收行程的時間仍留在系統總計，fork 子從零開始、exec 保留累計。
- snapshot 加入目前行程尚未結算的時間，但不改動原累計，避免重複計入。CPU%=兩次快照的 CPU ticks 差／牆鐘 ticks 差；以 PID 配對，避免 PCB 欄位重用誤配。第一張顯示 sampling，累計 CPU ms 則可直接讀取。比例用整數長除法避免乘 1000 溢位。
- CPU task 表示派發工作占用率，不是宿主 CPU 負載。idle/scheduler 為剩餘時間，包含排程開銷與 UART／期限輪詢；目前硬體可能仍在忙等，不宣稱量測到節電或真正 halt 的時間。整體 CPU 可能包含已退出行程，未必等於畫面各列的總和。
- RAM total=RAM_END-RAM_BASE，目前為 128 MiB；free 是配置池空閒頁，used=total-free，含 OpenSBI、FDT 保留區、核心靜態資料、頁表與使用者頁。Reserved/static=total-配置池總量，單獨標示，不能解讀成全都能回收。
- RSS 僅計入該行程常駐的使用者頁，不含核心堆疊／頁表；MEM%=RSS／128 MiB。SWAP KiB 僅列不在 RAM 的交換頁；上方 Swap slots 則包含常駐頁仍保留的 backing slots，兩者不必相等。heap_bytes 在快照中另保留，不把保留但未觸及的 heap 算成實體使用量。
- 新增 syscall 25 WORKSTATE（嚴格檢查結構長度與可寫範圍）、26 SLEEP_MS（1～60000 ms）、27 CONSOLE_POLL（raw console 所有者專用，0 無輸入、1～256 為字元加一）。快照先驗證位址，再收集並安全複製；未在複製期間開啟核心搶占。
- WAIT_TIMER 以硬體絕對 deadline 保存，排程迴圈檢查到期並喚醒；全員等待期限時沿用輪詢方式，不進沒有事件來源的永久 idle。它不是保證準點的即時服務，核心同步 I/O、其他行程時間片均可能延遲喚醒。
- 畫面取得 raw console，進 alternate screen、隱藏游標；每 50 ms 定時休眠與檢查輸入，每至少 1 秒重畫。q 後或可回報錯誤時恢復游標、原畫面與行輸入。其他行程的終端輸出仍可能交錯，下次更新重畫；沒有終端視窗管理或輸出隔離。

## 執行與驗收條件

1. 建立快照、CPU 計時、VM／RAM getter 與定時等待，接上 shell workstate／help。
2. 建置，QEMU 檢查每秒更新與 q 返回、Waiting／Zombie 列表、CPU 負載與記憶體／Swap 變化。使用既有程式與 QEMU 偵錯介面，不新增測試程式。
3. 確認既有 fork／wait、pipe、fan、RR 與資源回收不退步，更新 CODE_GUIDE、spec、操作網站與流程圖。

## 觀念來源

參考 notes/ch3 Process.md 的 PCB 與行程狀態、notes/ch5 CPU schedueling.md 的執行／等待與排程、notes/ch10 Virtual Momory.md 的常駐頁與交換觀念。此處 CPU 與記憶體口徑是 WUWAOS 明確定義的監測政策，不宣稱與 Linux top 完全相同。

## 驗收紀錄與限制

2026-10-28，WSL Ubuntu／Clang 18／QEMU 8.2.2／OpenSBI 1.3，獨立 workstate-check.img 與 workstate-swap.img。未動原使用者磁碟、未新增測試程式碼。

- 最終 make clean／make／make build/kernel-disk.elf 通過 -Wall/-Wextra/-Werror，四個 ELF 無未解析符號。shell text=12944 bytes，仍在 16 KiB 映像預算內；磁碟核心副本 317752 bytes。第一次使用結構直接賦值遇到 freestanding user ELF 缺 memcpy，改為明確逐 byte 複製，無新增標準函式庫依賴。
- spawn worker 2、quick 3，workstate 首張顯示 sampling；後續每秒更新，worker Waiting 與 quick Zombie CPU=0.0%，shell 監測開銷觀察約 2～3%，沒有使用者忙等占滿 CPU。按 q 恢復原畫面與 shell 行輸入。
- QEMU GDB 以實體記憶體模式把既有 worker 2 保存 sepc 改至 app.elf 原有 0x400010 自跳迴圈（沒有新增／注入指令）。wakeup 後重開 workstate，worker 約 97.3～97.6%，shell 約 2.3～2.4%，整體 CPU task=99.9%，累計 CPU ms 持續成長，q 仍可離開。kill／wait 2=-9，wait 3=40。
- pipe 子 4／5 傳 80 bytes、EOF、wait=0；fork 子 6 data／stack=101／101、父=1／100，wait=0。
- vmstat limit 8、保留並填入 12 個 heap 頁後讀頁 0 首／中／尾均 65。workstate 顯示 RSS 32 KiB、SWAP 40 KiB、Swap slots 13；RAM used=5180／131072 KiB，Reserved/static=4860 KiB。vmstat 的 swap_in=9、swap_out=16，invalid_faults=0、io_errors=0。釋放 heap、恢復常駐預算。
- fan PID 7 儲存 workstate verified 加換行共 19 bytes、Ctrl+X／wait=0、cat 讀回正確。最後只剩 shell、3 個空 PCB、timer_interrupts=1534。shell exit 後 monitor：free_count=0x7afe（31486，開機基準）、resident=0、swap used=0、timer armed=0，quit 結束 QEMU。紀錄在忽略的 build/workstate-console.log；偵錯位址只適用該建置。
- npm docs:build 通過，維持既有 Mermaid 大型 chunk 提醒；瀏覽器確認新章節欄位表格、導覽與 CPU 流程圖。

未注入錯誤快照長度／位址、計數器長期溢位或終端 I/O 失敗；未驗證優先權排程、多核心與真實硬體。不含每執行緒統計、user/system 分離、I/O wait 分類、歷史儲存、排序／kill 操作、核心同步鎖與遠端監測。網站不內嵌 OS 程式碼，僅連結 GitHub。

## 2026-10-28：行程表格排版調整

需求：workstate 的標題與行程資料欄位不易對齊，改為易讀的表格。保留一秒更新、q 退出、所有欄位和統計口徑，不新增測試程式碼。

設計：先將快照各欄格式化，計算標題及所有行程所需最大欄寬；以 ASCII `+`、`-`、`|` 畫邊框，數字靠右、名稱及狀態靠左。預設表格寬 77 欄，RSS(K)／SWAP(K) 的 K 表示 KiB。長數值會擴展欄寬，不截斷；超過終端寬度時需加寬視窗。每行一次輸出並清除行尾，避免上一輪較寬時殘留。固定 4 個 PCB、9 欄，欄寬掃描及輸出成本與顯示欄位數成正比。

驗收：WSL Ubuntu／Clang／QEMU 8.2.2 使用獨立 workstate-table-check.img 與 workstate-table-swap.img。建置通過；workstate 同時顯示 shell Running、worker Waiting、quick Zombie、counter Waiting 四列，標題與數字對齊、第一輪為 --、後續每秒更新，q 返回 shell。wakeup／wait 回收子行程後再次開啟，單列顯示與 q 返回正常。觀察初版 shell text 已達 16368 bytes、距 16 KiB 上限僅 16 bytes；限制重複的數字格式化函式內聯後降至 14624 bytes，重新建置並重新完成以上 QEMU 操作。最終互動紀錄在 build/workstate-table-final.log。未實際驗證超出終端寬度的數值。
