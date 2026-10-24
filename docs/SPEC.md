# Spec 05：Fork → Exec → 合作式排程 → 回收

## 目標與範圍

使用者希望接續 fork（複製行程）／exec（替換程式），完成從建立 process（行程）到排程與回收的流程。
本階段串接既有合作式排程、sleep/wakeup、exit/wait 與管道，不加入計時器搶占、COW（寫入時複製）、任意 ELF 解析、磁碟檔案系統或通用描述號表。
保留前階段尚未提交的內容，不自行 commit / push。未經同意不新增測試程式碼，透過正常 shell 命令與內建工作程式驗收。
舊 spec 01–04 保留歷史，本文件記錄 fork／exec 里程碑；後續檔案系統見 SPEC-06，現行 FIFO 排程器與就緒佇列見 SPEC-07。

## 架構

- fork 完整複製使用者 code/data/stack（程式碼／資料／堆疊），為子行程建立新的 PCB（行程控制區塊）、PID、頁表、核心堆疊與 TrapFrame（暫存器保存狀態）。
- 不直接複製核心堆疊或整份 PCB；等待與 I/O 請求重新初始化，父 PID 指向呼叫者。
- 複製的是已越過 ecall（環境呼叫指令）的使用者狀態。父返回新 PID，子 a0（返回值暫存器）為 0，兩者從同一位置繼續。
- 複製現有輸入／輸出管道端點並增加 reader/writer（讀／寫端）參照數，exit/kill 時照原本機制減少。exec 保留端點。
- exec(program_id) 先在一份備用 AddressSpace（位址空間）完整準備嵌入映像，再交換位址空間所有權並切換 satp（頁表設定暫存器）。失敗時保留原程式與資料。
- 交換後的舊空間成為下一次 exec 的備用空間；所有空間的核心映射相同，切換中目前核心堆疊仍可存取。
- 成功 exec 保留 PID、父子關係與端點，清除舊使用者暫存器、資料／堆疊與等待欄位，設定新入口／參數，不返回舊程式；失敗回傳錯誤。
- shell 與 app（工作程式）分別連結成獨立 ELF，再轉 binary（原始映像）嵌入核心；不是只改 PCB 角色而繼續原 shell 映像。
- 位址布局維持 16 KiB code、4 KiB data、4 KiB user stack、每行程 16 KiB kernel stack，最多 4 個行程，另加一份備用位址空間。

## 介面與操作

新增系統呼叫 15=fork()、16=exec(program_id)。program_id 1=worker、2=quick、3=writer、4=reader、5=counter。

- fork：複製一次 shell 執行狀態；父取得子 PID，子得到 0，列出各自私有 sequence（操作序號）及 stack marker（堆疊資料）。父主動 yield 讓子先修改，再顯示自己的原值；子 exit(0)，由 wait 回收。
- run worker：fork 子行程後 exec app 的 worker，yield 後等事件通道 7；wakeup 7 後結束。
- run quick：fork 後 exec quick，返回結果 40。
- run counter：fork 後 exec counter，等待事件通道 9，再輸出倒數資料並逐次 yield，展示兩個行程的合作式交替執行。
- 子行程開始執行 exec 前輸出 fork 的子返回值，核心輸出保留的 PID、載入 app 映像與 program ID，便於觀察交接。
- spawn 舊命令維持可用；pipe 仍為固定 writer | reader，不擴成通用 shell。

## 最小步驟

1. 拆出 user/print.c/h 共用輸出，user/app.c 獨立工作映像；shell 不再內含工作程式本體。
2. vm 支援選擇映像、複製使用者頁、以備用空間原子替換。
3. process 加入 fork/exec，管道增加端點繼承，dispatcher 正確處理 exec 成功時不可覆寫新 a0 的規則。
4. shell 加 fork/run 指令及正常狀態輸出，更新 Makefile、README 與 CODE_GUIDE。
5. 完整建置，於 QEMU 互動觀察建立、狀態切換、獨立記憶體、exec、結束與欄位重用；填入驗收結果。

## 驗收條件

- 完整建置無警告／未解析符號，shell／app 為獨立 ELF 與嵌入映像。
- fork 父取得子 PID、子取得 0，私有 stack／data 修改不影響父；父子使用不同頁表與實體頁。
- exec 前後 PID 相同，執行獨立 app，初始化資料正確，舊 shell 不在子行程中繼續接受命令。
- 同時 run 兩個 counter，ps 可見兩個 Waiting；wakeup 9 後依 yield 交替執行，結果各自獨立。
- exit → Zombie → wait 回收正常，反覆建立／執行可重用欄位；ps 最終僅留 shell、空閒欄位 3。
- 既有固定管線仍傳送完整 80 bytes 並回收。
- 文件明確記錄哪些邊界尚未動態驗證；不把正常示範當成完整測試覆蓋。

## 實際驗收紀錄

2026-10-16，WSL Ubuntu、Clang 18.1.3、QEMU 8.2.2、OpenSBI 1.3；virt／單核心／128 MiB。

- `make clean && make` 通過 -Wall/-Wextra/-Werror。最後調整 fork 命令的父行程 yield 次序後再次 make，kernel.elf／user.elf／app.elf 的 `llvm-nm -u` 都沒有未解析符號。
- shell 與 app 分開連結、轉 binary、嵌入核心；最終 user.bin 為 4672 bytes、app.bin 為 16392 bytes，後者包含 16 KiB 位址間距後的初始 remaining=3 資料。
- 最終映像 QEMU 正常操作 `fork; wait 2; run counter; run counter; ps; wakeup 9; wait 3; wait 4; run quick; wait 5; ps`（實際每條按 Enter）。PID 2 子返回 0、data=101／stack=101；子結束後父顯示 data=1／stack=100。狀態含 New→Ready→Running→Zombie→Unused。
- PID 3／4 fork 後各自 exec，PID 不變，進 Waiting channel 9。wakeup 回傳 2；倒數輸出順序為 3:3、4:3、3:2、4:2、3:1、4:1，兩者狀態反覆 Running→Ready→Running，再 exit(0)／wait 回收。PID 5 quick 結果為 40。最後僅 PID 1、空閒欄位 3。
- 前一輪同核心實作的完整互動：run worker PID 6 於通道 7 等待，kill／wait 取得 -9；PID 7 正常 wakeup／wait 得 0。固定管線 PID 8／9、10／11 連續兩次完整收到 80 bytes／EOF，均回收。PID 12 再次 run quick／wait 得 40，證明多次 exec 與欄位重用。
- 同輪建立 PID 13／14／15 填滿四欄後，run quick 的 fork 回傳 -11，既有三個 Waiting 行程仍存在；依序 kill／wait 得 -9。重複 wait 15 回傳 -10，ps 恢復空閒 3，下一次 fork 成功取得 PID 16。
- 該輪 QEMU monitor 的 info mem：shell code 0x400000→0x8020e000、data 0x404000→0x80212000、stack 0x800000→0x80213000，權限各為 RXU／RWU／RWU；UART、核心映射沒有 U 權限。
- PCB 根表：shell 0x8020a000、fork 子 PID 2 0x80214000；exec 後 counter PID 4／5 為 0x80214000／0x80232000。位址空間儲存區會輪替，不能把 PID 永久綁定實體位置。
- 後續 PID 16 fork 的子根表 0x80214000；其 user_l0 的 data PTE=0x200870d7，指向實體 0x8021c000。monitor `xp /1gx` 讀得父 data 實體 0x80212000 值 2、子 data 實體 0x8021c000 值 0x66（102），與第二次 fork 的正常輸出相符。子 code PTE 同樣指向自己的私有頁。
- 本機紀錄：build/lifecycle-console.log、build/lifecycle-traps.log 為完整流程；build/lifecycle-final-console.log、build/lifecycle-final-traps.log 為最終 fork 次序調整後的操作。build/ 被 Git 忽略，未加入測試檔案。
- 最終 QEMU 記錄含 181 次 user_ecall（使用者環境呼叫），console 沒有 User trap／Kernel trap 的非預期例外診斷；git diff --check 通過。

### 尚未動態驗證與目前限制

- 未對未知 exec ID／無效使用者指標注入 syscall，也未執行持有管道端點後 fork 的工作程式；這些僅完成程式碼檢查，不宣稱動態覆蓋。固定管線通過不等於端點繼承路徑已動態驗證。
- exec 只有固定內建映像與預配置資源，錯誤 ID 在修改前拒絕；沒有一般 ELF 格式錯誤／磁碟讀取失敗等路徑。
- 回收將 PCB 欄位變成 Unused，頁表與記憶體預留儲存區留待下次建立重新清除，不是一般動態實體頁分配器。
- 合作式排程要求程式 yield／阻塞／退出；無限運算的使用者程式仍可獨占 CPU。sleep 是事件通道，並非計時睡眠。沒有計時器搶占、完整訊號或 Ctrl+C 取消。
- 未建立測試框架或測試程式碼；上述為正常 shell／工作程式的實機模擬互動驗收。
