# Spec 34：Thread 與共用行程資源

狀態：**多 Thread 核心/API 與共享資源生命週期已實作；僅完成編譯與程式碼檢視，runtime 驗收 NOT_RUN**。本文件於 2026-11-10 起草，階段 A 的回歸記錄見[階段 A](./thread-stage-a.md)，本次實作範圍與證據見[階段 B/C 記錄](./thread-stage-b.md)。QEMU、使用者 Thread 交錯、取消與 I/O 故障情境尚未驗證，不得標示為 runtime 通過。[教學背景](./topics/threads.md)。

## 目標與執行環境

讓同一個 Process 裡的多條使用者 Thread 共用位址空間與 FD table，各自保存暫存器、使用者堆疊、核心堆疊及等待狀態。排程器選擇 Thread；一條 Thread 等待 I/O 時，同 Process 的其他 Ready Thread 仍可執行。

第一版採由核心直接排程的 1:1 模型、QEMU `virt -smp 1`、RV64 LP64 整數 ABI。每條使用者 Thread 有一份核心可排程實體；這不表示每條 Thread 都配置一顆 CPU。保留使用者模式計時器搶占、核心內不強制搶占、SIE=0 下操作等待佇列的政策。核心可在指定阻塞點切到其他 Thread，持有者因此仍須正確同步。

本版不包含 SMP、SMT 管理、獨立的背景 kernel worker、POSIX pthread 相容層、detach、非同步 thread cancellation、TLS、浮點／向量 context。不得只把 QEMU 改成 `-smp 2` 就宣稱支援多核心。

## 設計基底與需要修改的邊界

下表保留起草時的基底對照；階段 A 已將執行、等待、gate／sync owner、raw TTY owner 移至 TCB；多 Thread 的 FD pin、額外 user stacks、join 與群組退出已接入，尚待 runtime 驗收。

| 現行位置 | 現行行為 | Thread 版必須完成的變更 |
| --- | --- | --- |
| `kernel/proc/process.h` | PCB 同時持有資源與執行狀態 | 拆成 Process 資源容器與 Thread 執行控制區塊 |
| `kernel/proc/scheduler.c` | Ready queue 串接 `Process *` | 改成 `Thread *`；同一 Thread 最多入列一次 |
| `arch/riscv/trap.c`、`trap.S` | frame、sscratch 對應目前 Process stack | 對應目前 Thread 的 TrapFrame／核心堆疊 |
| `kernel/proc/gate.*` | owner 為 PCB 指標與 PID | 改成 Thread 指標與 TID，保留 no-thread boot owner |
| `kernel/ipc/sync.c` | mutex owner／wait queue 是 Process | owner 與等待者都改成 Thread，保留既有 owner-died 規則 |
| `kernel/proc/event.c`、`process.c` | 掃描 Process 的等待紀錄 | 掃描 Thread，保留 source generation／sequence |
| `kernel/mm/vm.c` | 固定 code/data、一頁 stack 與 heap 的頁面索引 | 增加 Thread stack 區域與保留表，整合 fault／FIFO／swap／usercopy |
| `kernel/io/io.c` | FD lookup 與阻塞後重查以 Process 為單一執行者 | 共用 FD table 加鎖、暫時保留 OFD；一次 syscall 固定操作目標 |
| `kernel/io/tty.c` | raw owner 以 PID 辨識 | raw owner 改用 TID；session 生命週期仍屬 Process |
| `kernel/proc/workstate.c` | 以 Process 計算執行與記憶體 | CPU 合計 Thread；共用記憶體只計一次 |

`kernel/proc/kmutex.*` 是保留的本機原型，沒有接入 FS／VM runtime。本版以現行 KernelGate 為整合基礎，不並行維護第二套 FS／VM 鎖。

## 資源歸屬與識別

```text
Process (PID)
 +-- address space: text / data / heap / thread stacks
 +-- FD table / cwd / program / parent PID / terminal session
 +-- lifecycle: live threads / exit status / child wait claim
 |
 +-- Thread (TID A): frame / context / kernel stack / wait state
 +-- Thread (TID B): frame / context / kernel stack / wait state
 +-- Thread (TID C): frame / context / kernel stack / wait state
                         |
                         v
                Ready queue -> scheduler -> hart 0
```

| 歸屬 | 欄位或資源 | 生命週期 |
| --- | --- | --- |
| Process | PID、parent PID、program、page table／VmSpace、cwd／previous_cwd、FD table、terminal session | 所有 Thread 結束且共用資源清理完成後，才進入 Process Zombie |
| Thread | TID、`Process *`、state、ready links、TrapFrame、KernelContext、kernel stack、user stack slot | 結束後成為 Thread Zombie；join 或 Process 整體清理後回收 |
| Thread | event/channel/child/join/sync 等待、取消旗標、cancel defer、gate depth、console route、CPU accounting | 不得讓同 Process 的另一條 Thread 覆蓋 |
| 共用物件 | OFD offset、pipe buffer、FS／VM gate、sync object | 由參照與鎖保護，不隨任一 Thread 單獨退出而失效 |

容量建議固定為 `PROCESS_LIMIT=16`、全系統 `THREAD_LIMIT=32`、每 Process 最多 8 個已占用 Thread slots，包含初始 Thread、建立中與尚未 join 的 Zombie。Thread slot 耗盡回 `ERR_NO_SLOT`，不能增加 Process slot 冒充 Thread。

PID 與 TID 為不同命名空間；`getpid()` 對同 Process 的所有 Thread 相同，`gettid()` 各自不同。TID 使用正值單調遞增，不重用；到 `LONG_MAX` 前拒絕新增，避免舊 TID 指向重用的 TCB。Boot／scheduler 無目前 Thread 時，`thread_current_or_null()` 回傳 null，不能借用剛才執行者的身分。

Process 狀態改為 `NEW -> LIVE -> DYING -> ZOMBIE -> UNUSED`；Ready／Running／Waiting 屬於 Thread。`process_current()` 回傳 `thread_current()->process`。`Process.thread` 是仍占用 Thread 中的代表指標，Thread 退出／join 時更新；群組清理回收所有 TCB 後為 null。此時 Process Zombie 的 `ps` 顯示 `TID=0` 與 Process lifecycle state，`pcb` 顯示沒有占用 Thread slots；PID／exit status 保留到 parent wait 回收。

## 使用者 API 與 syscall ABI

放在 `user/lib/api.h`、`user/lib/api.c` 與 `user/lib/thread.S`。以下為 WUWAOS 自訂 ABI，不保證 pthread 相容。

```c
typedef long thread_id_t;
typedef long (*thread_entry_t)(void *argument);

long thread_create(thread_entry_t entry, void *argument);
long thread_join(thread_id_t tid, long *status);
_Noreturn void thread_exit(long status);
long gettid(void);
```

| 操作 | 成功結果 | 第一版語意 |
| --- | --- | --- |
| `thread_create` | 新 Thread 的正 TID | 核心配置 stack；可能在建立者回到 U-mode 前執行 |
| `thread_join` | 被回收的正 TID | 僅限同 Process；等待並回收一次；status 可為 null |
| `thread_exit` | 不返回 | 只結束呼叫者，最後一條 Thread 結束時啟動 Process 清理 |
| `gettid` | 呼叫者 TID | 不等於 hart ID；不要求數值與 PID 不同 |
| 既有 `exit` | 不返回 | 任一 Thread 呼叫即要求整個 Process 結束 |

現行 syscall 編號為 1–41。本設計預留 `SYS_THREAD_CREATE=42`、`SYS_THREAD_JOIN=43`、`SYS_THREAD_EXIT=44`、`SYS_GETTID=45`，實作前須再次檢查是否已被使用；舊編號不得變動。

RV64 syscall 沿用 `a7` 傳編號、`a0..a2` 傳參數、`a0` 傳結果。`SYS_THREAD_CREATE` 的原始參數為 `(entry, argument, trampoline)`；C wrapper 額外傳入受同一 ELF／內建映像控制的 user trampoline 位址，使用者 API 仍只有兩個參數。Join 使用 `(tid, status_address)`，exit 使用 `(status)`，gettid 無參數。

錯誤約定：不存在／不同 Process／已回收／join 自己回 `ERR_INVALID`；目標已被其他 joiner claim、形成 join cycle、fork/exec 條件不符、Process 正在換映像或退出回 `ERR_BUSY`；無槽位回 `ERR_NO_SLOT`；實體頁不足回 `ERR_NO_MEMORY`；無效 user pointer／entry／trampoline 回 `ERR_ADDRESS`；等待因 Process 結束被取消回 `ERR_INTERRUPTED`，再經既有取消邊界結束。`thread_exit` 不把錯誤回傳到 user。

參數 `argument` 當作不透明數值傳遞，核心不預讀它指向的內容；呼叫者要確保其資料在子 Thread 使用期間仍有效。Entry 的正常 `return value` 必須由 trampoline 轉成 `thread_exit(value)`，不能落入程式起始入口的 Process `exit`。

建立與成功 join 也定義資料可見性：建立前發布的初始化資料可供新 Thread 使用；join 成功後可觀察 target 退出前完成的寫入。User syscall wrapper 與同步函式需有正確 compiler memory clobber，核心 context／發布邊界需符合平台的記憶體順序；不得靠 `volatile`、偶然的函式呼叫或把本版單 hart 規則直接套到未來 SMP 來宣稱保證成立。

## 堆疊、位址空間與 CPU 初始狀態

主 Thread 保留既有使用者 ABI：`0x800000..0x801000` 一頁 stack，sp 起始為 `0x801000`；在 `0x7ff000..0x800000` 明確保留不可按需配置的 user guard。新 Thread 使用下列獨立區域，與 `0x400000` 程式區及 `0x1000000` 起的 heap 不重疊。

```text
Each extra user stack slot: 0x9000 bytes (36 KiB)

slot_base = 0x900000 + slot_index * 0x9000
slot_index = 0..6                 (7 extra threads)

low address
 +--------------------------+ slot_base
 | guard: unmapped, 4 KiB    | fault must not allocate it
 +--------------------------+ slot_base + 0x1000
 | user stack: RW, 32 KiB    |
 |                          |
 | grows downward           |
 +--------------------------+ slot_base + 0x9000 = initial sp
high address
```

核心管理額外 stack 的保留／回收，第一版不接受 caller 傳入的任意 stack buffer。User stack 沒有執行權限，sp 維持 16-byte 對齊。每條 Thread 另有 16 KiB kernel stack 與一頁 supervisor guard；kernel stack guard 上限要從 Process 數改為 Thread 數，scheduler stack 另計，不能映射到 user。

使用者堆疊屬於共用位址空間；「各自使用」不代表其他 Thread 不能透過指標存取。Guard 只保護對該頁的存取，不能保證任意大幅跨越 guard 的越界都被攔下。

VM 必須擴充 `SPACE_PAGES`、`page_address`、`page_index`、permissions、accessible、fault、clone／destroy、swap 與 usage，並以 stack slot 保留表拒絕 guard／未配置 slot 的存取。每個額外 slot 有 8 個可按需配置的 stack pages；7 個 slot 共增加 56 筆頁面中繼資料，guard 不占可換頁項目。發布 Ready 前實際準備最高一頁，以便初次進入 trampoline；其他頁依 demand paging 配置。整個 VmSpace 只銷毀一次。

新 Thread 從 user trampoline 進入：`sepc=trampoline`、`a0=entry`、`a1=argument`、`sp=slot_top`、`ra=0`，`sstatus` 由核心建立為可返回 U-mode 的狀態。`gp` 沿用同映像的呼叫者值；其餘一般暫存器清零，包含未提供 TLS 的 `tp`。保留整數暫存器的能力不代表已支援 TLS／浮點／向量 ABI。

Trampoline 保存函式入口，再把 argument 移到 `a0`、呼叫 entry，返回後以 `a0` 作為 Thread exit status。所有分支都符合 LP64 呼叫規約；不信任使用者傳來的 sstatus、核心位址或 stack top。Entry／trampoline 必須為同 VmSpace 內可執行且 2-byte 對齊的 user 位址；所有範圍計算先檢查溢位。VM 驗證以實際區域權限／ELF allowance 為準，不能只檢查「小於 USER_LIMIT」。

## 建立、排程與阻塞

建立順序：

1. 在單 hart、SIE=0 與 Process 成員狀態保護下，檢查 LIVE、未換映像與容量，保留 NEW TCB 與 stack slot；記錄保留但不發布 Ready。
2. 在可取消清理規則下取得 VM scope，驗證 entry／trampoline、準備最高 stack page、初始化 kernel stack、TrapFrame 與首次執行 context。必要的 VM I/O 可阻塞，NEW slot 仍須阻止競爭的 fork/exec。
3. 重新檢查 Process 未要求退出，才增加 live count 並一次發布 Ready。失敗／取消則完整撤銷新增 user pages、swap slots、stack reservation 與 TCB；無法立即釋放時保留清理紀錄重試，不留下可執行的半成品。

Ready queue 改為串接 Thread，保留現有 RR quantum。Thread 首次執行走 first-run／user-enter；之後 syscall／fault／timer 以它自己的 TrapFrame 返回。核心 context switch 仍保存 `ra/sp/s0..s11`，完整 user registers 則由 TrapFrame 保存。`sscratch` 在 U-mode 指向該 Thread kernel stack top，在 S-mode 依既有規則為零。

同 Process 的 Thread 共用 page table root，但第一版可繼續沿用現有 `vm_use_kernel`／`vm_enable` 和 fence 路徑。**不把「相同 Process 必定不寫 satp／不 flush TLB」列為本版保證。** 跨 Process 需選擇新 root；每次離開 VM scope 前完成既有映射更新與 fence，不能以 Thread 共用記憶體為由省略。

```text
UNUSED -> NEW -> READY -> RUNNING
                   ^       |
                   |       +-- yield / user timer --> READY
                   |       |
                   |       +-- event / sync / join --> WAITING
                   |                                  |
                   +-------------- wake --------------+

RUNNING -> exit / safe cancellation -> ZOMBIE -> join -> UNUSED
```

Event 仍採「取得 source generation／sequence → 檢查條件 → 登記等待 → 切到 scheduler」；同一 Thread 同時最多一筆等待。IRQ callback 只 ACK／收集／通知並使等待者 Ready，不在 callback 中睡眠或切換 stack。Thread A 等待不改變 Thread B 的 state；所有掃描 timeout、cancel 與 wakeup 的迴圈都改用 Thread 容量。

## Join 與 Thread 退出

每個 target 最多一名 joiner，以 TID 身分 claim。Join 自己無效；跨 Process 無效；沿 join target chain 檢查 cycle，檢查最多 `THREAD_LIMIT` 步，形成循環回 `ERR_BUSY`。宣告 claim、檢查 Zombie、擷取 event sequence、進入等待要遵守不漏喚醒的單 hart 規則。Join 是等待特定 Thread；既有 `wait(pid)` 仍只回收子 Process。

Target 正常退出須釋放自己持有的 user sync 所有權、清掉 raw owner 與等待註冊，確認無內部 gate／cancel-defer 遺漏，記錄 status、移出 Ready queue、標記 Zombie 並通知 joiner。不得在仍執行的 kernel stack 上釋放該 stack；先切到 scheduler，確認 stack 已離開，再允許回收。

Joiner 喚醒後重驗 TID 與 claim。非 null 的 status 必須成功寫入 caller 的 user memory，才消耗 Zombie。status 寫入不得落在即將回收的 target 額外 stack slot；若位址無效或 copyout 失敗，保留 Zombie 並釋放 join claim，讓其他有效呼叫重試。Target user stack／swap slots 與 kernel stack 回收成功後，才釋放 TCB 和每 Process 容量。

Join claim 在 copyout／VM 清理可阻塞的整段期間都有效。Joiner 因 Process exit 取消時，在自身 stack unwind 中釋放 claim；Process 整體退出期間停止新的 join/create，由群組清理接管尚未回收的 Zombie。第一版沒有 detach；不 join 的 Zombie 會占用 Thread 容量。

初始 Thread 的 user stack 為既有映像的一部分，join 可回收其 TCB／kernel stack，但第一版保留該一頁 user stack 到整個 Process 回收，避免改變既有 fork/exec stack ABI。額外 stack slots 依上述 join 規則回收；所有 Thread 的自動區域變數在 Thread 結束後都不應再使用。

## Process exit、kill、fault、fork 與 exec

任一 Thread 呼叫既有 `exit(status)`，Process 由 LIVE 進 DYING，第一次群組退出請求決定 status。停止 create／換映像，向其餘 live Thread 發取消請求；可取消等待醒來回原 kernel stack 清理。外部 `kill(pid)` 沿用原有權限限制，對整個目標 Process 生效；沒有公開 `thread_kill`。未處理的 user fault 終止整個 Process，避免其他 Thread 在未知共享資料狀態下繼續。

已提交 Block DMA、FS transaction、VM swap revoke／restore 等 cancel-defer 範圍必須先到安全點；不可強制釋放 gate、銷毀被引用的 buffer 或直接把 suspended stack 清零。因等待外部輸入而無法 unwind 的路徑必須有取消通知，不能讓 Process 永久卡在 DYING。無法確認裝置 reset 的既有 fail-stop 政策保留。

呼叫 exit 的 Thread 本身也退出；由 scheduler 安全的回收流程持續檢查所有 Thread 是否離開 kernel stack、等待佇列、join claim 與 DMA。最後一條自然 `thread_exit` 若未有群組退出請求，使用它的 status 啟動相同清理。FD、TTY session、VmSpace、swap slots 僅清一次；最後完成後才把 Process 公布為 Zombie，通知父 Process 的 waiter。需要睡眠的回收不得硬塞進 nonblocking scheduler poll；沿用／擴充 deferred cleanup 與 `vm_try_destroy`，條件未滿保留 DYING 重試。

第一版 `fork`／`exec` 僅允許呼叫者是 Process 唯一占用的 Thread，**包含 NEW／Zombie 在內占用超過一個 slot 都回 `ERR_BUSY`**。這比只看 runnable 數更嚴格，避免複製未回收 stack 與等待身分。若唯一 Thread 是額外 stack slot，fork 必須複製該活動 stack 的 reservation／內容；exec 改建新映像，回到標準主 stack。

Fork／exec 在檢查到發布完成期間設 `image_changing`，Thread create 遇到此旗標回 `ERR_BUSY`，旗標必須在所有錯誤／取消路徑還原。Fork 子 Process 只產生一條 Thread，child PID／TID 都重新取得，子 `a0=0`；exec 保留 PID／TID、替換 address space，清除舊額外 stack reservation。不得複製 kernel C stack、joiner、event wait、gate owner 或取消深度。

同 Process 多個 Thread 同時 `wait(pid)` 時，子 Process 最多一名回收者，使用 caller TID claim；第二名回 `ERR_BUSY`。Status copyout 失敗／waiter 取消要釋放 claim 並保留子 Process 結果，不能回收兩次或寫入已退出 Thread 的 stack。

## 同步、FD 與終端的一致性

KernelGate owner 改為 `(Thread *, TID)`，遞迴只允許同一 Thread；同 PID 的不同 Thread 必須互斥。No-thread boot／scheduler owner 要與 unlocked 狀態區分，且不得等待。User mutex owner、sync queue links、sync_wait_handle、owner-died cleanup 與 destroy reference scan 全改為 Thread。Thread 持有 user mutex 退出時保留 poisoned 語意；不把共享資料自動視為已修復。

Thread create **不增加** Process FD table 內每個 descriptor 的引用數，Thread exit **不關閉**共用 FD。FD table 的 lookup／open 安裝／close／dup／dup2 由每 Process gate 保護；呼叫開始後保留 OFD 與所屬 pipe／TTY 的暫時引用，釋放 table gate，再進入會睡眠的 usercopy／I/O。不要持有 table gate 等待 pipe 資料或磁碟完成。

另一 Thread close 並重新使用相同 FD number 時，已開始的 read/write 必須繼續針對保留的原 OFD，不能在醒來後重查 FD number 而悄悄改讀另一個檔案。OFD／pipe generation 防止槽位重用；offset 仍由 OFD gate 序列化，最後一個暫時引用釋放後才可回收。EOF／broken pipe 依實際引用生命週期觀察。Open／dup2 失敗須撤銷預留 descriptor；不能把可睡眠操作中的半成品交給另一 Thread。

Cwd／previous_cwd 屬 Process，共用更新受保護；相對路徑在 syscall 開始時取得 cwd 快照，不能在 I/O 等待後用另一 Thread 改過的 cwd 重解釋。TTY session 引用留到 Process cleanup；raw input owner 改用 TID，Thread 退出釋放自己的 raw owner。多個 canonical reader 競爭同一輸入串流，不廣播或保證每條 Thread 都收到相同字元。Console routing 暫存與診斷歸屬仍須存每 Thread，避免阻塞後輸出到別人的路由。

鎖順序沿用現行 FS／VM／Block scope，先列出所有可睡眠呼叫鏈再移植。特別禁止持有成員／FD table gate 去等待 Thread exit／join／pipe／裝置完成；生命週期 claim／reservation 應在短 scope 內建立，再以明確引用跨越等待。Single hart 仍可能因兩條 kernel stack 互等形成死結。

## 實作分階段與驗收

| 階段 | 工作與完成條件 | 驗證 |
| --- | --- | --- |
| A：拆 Process／Thread | 每 Process 暫時只有一條 Thread；scheduler／trap／wait／gate／sync／TTY 全以正確身分運作 | 既有開機、fork/exec/wait、pipe、sync、檔案與取消行為回歸 |
| B：共用位址與 stack | 新 VM stack 區域、TCB limits、create／exit／join、trampoline、安全回收 | 同 PID 不同 TID；stack 不重疊；return status；容量與失敗回滾 |
| C：共享資源生命週期 | FD pin、cwd snapshot、group exit、child wait claim、fork/exec 排他 | 阻塞中 close/reopen、Process exit、DMA 等待取消與雙重 join／wait |
| D：示範與觀測 | 預定 `demo threads`、`threads`，`ps`／workstate 以 Process 聚合並可看 Thread | CPU accounting 不重複；教學例與真實 QEMU trace 分開記錄 |

Thread 執行狀態由 `kernel/proc/thread.h`／`thread.c` 管理；使用者入口位於 `user/lib/api.h`／`api.c`／`thread.S`，容量定義在 `include/thread_limits.h`。本次沒有新增 `demo threads` 或獨立 Thread 列表命令；既有 `ps` 顯示代表 TID，`pcb` 顯示代表 TCB。

最低驗收清單：

1. 開機、GUI shell（若執行）、fork/exec/wait、pipe、FS、sync、guard、取消行為保留；未跑 GUI 明記 NOT_RUN。
2. 同 Process 兩條 CPU-bound Thread 不呼叫 yield，也能被 user timer 輪流派發；一條阻塞時另一條前進。
3. 共用計數器以同一 mutex 更新得到精確結果；用受控交錯重現無鎖遺失更新，不依賴偶然 race。
4. 相同 PID 的另一 Thread 無法冒充 mutex／recursive gate owner；一條 Thread 遞迴 gate 仍正常。
5. Thread return／thread_exit status、join 已退出者、join 自己／他 Process／重複 join／cycle、非法 status 與取消後重試均符合約定。
6. User/kernel stack 不重疊；guard fault 不按需補頁；stack 可換出／換回、join 釋放 swap；維持對 kernel guard 既有 emergency-stack 限制的誠實說明。
7. 全域／每 Process 容量、NEW 建立失敗、記憶體不足、Thread Zombie 占槽、TID 不重用都可觀察，無新增資源洩漏。
8. 一條 Thread 在 pipe／disk／gate 等待，另一條 close/reopen／exit：原 OFD 不被替換、等待能取消、DMA buffer 不提前釋放、Process 不提前 Zombie。
9. 主 Thread 先 thread_exit，其他 Thread 仍能執行；整個 Process exit／kill／fault 後全部退出，共用資源只清一次。
10. 多 Thread fork/exec 拒絕；join 到唯一 Thread 後恢復；唯一額外 stack Thread 的 fork／exec 與失敗回滾正確。
11. 兩條 Thread 同時 wait 同一子 Process 僅一名成功 claim；copyout 失敗不丟結果。
12. `ps`／Thread 清單的 PID/TID、state、wait target 正確；workstate 共用 RSS 不乘以 Thread 數。CLI disk boot 無 swap 時，無法涵蓋的 swap 測試須改用獨立 swap 模式執行。

## 驗證狀態與參考

| 項目 | 狀態 |
| --- | --- |
| 規格與現行欄位／呼叫點比對 | 已完成文件層級調查 |
| 階段 A：單 Thread／Process 的 TCB runtime | 已實作，主機與 QEMU 基本回歸通過；見[紀錄](./thread-stage-a.md) |
| 多 Thread syscall、額外 user stack／join／群組生命週期 | CODE_PRESENT；編譯通過，runtime NOT_RUN |
| 共享 FD pin、cwd snapshot、child wait claim、fork／exec 限制 | CODE_PRESENT；編譯通過，runtime NOT_RUN |
| `demo threads`／專用 Thread 列表介面 | NOT_IMPLEMENTED |
| 多 Thread QEMU／新增故障注入／效能測量 | NOT_RUN |
| 網站文件建置 | 2026-11-10 VitePress 建置通過；不代表 Thread 驗收 |

硬體觀念與來源集中於[教學章節](./topics/threads.md#資料來源與實作對照)。本規格的容量、位址、錯誤與生命週期是 WUWAOS 的設計決策，不是 RISC-V ISA 或 POSIX 的強制規定。
