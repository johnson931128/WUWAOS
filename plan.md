# 後續實作計畫

目前整體狀態：第 1 階段、2A 與 2B1 完成；2B2 暫停於 Block pointer refactor，非同步 request 尚未完成；2C 與第 3–6 階段未開始。Spec 33 的原始碼分層工作已完成。程式實作暫停，先更新文件與網站。

## 1. 多 TTY 與 session 物件化

- 進度：完成；目前 UI 仍是單一 Terminal view。
- 實作內容：`kernel/io/tty.c` 管理 16 個 endpoint slot：slot 0 是固定 UART，slot 1–15 可建立 GUI session。每個 endpoint 獨立保存 input/output ring、active 狀態、raw owner、CR/LF 狀態、consumer/context 與引用數。handle 的低 32 位編碼 slot+1，高 32 位為 generation；generation 不回繞，達上限的 slot 不再配置。`OFD_TTY` 保存 handle；同一 OFD 的 dup/fork 增加 OFD refs，只有 OFD 首次建立時 retain TTY，最終 close 時 release TTY。GUI 的 fd 0 使用可讀 OFD，fd 1/2 共用可寫 OFD；核心診斷只按 fd 1 的 TTY endpoint 路由。process/session service 位於 kernel，UI 只提供 output consumer 和視圖 context。
- 解決觀念與演算法：把可變 endpoint 身分與儲存 slot 分開，handle 每次重用都帶新世代，避免舊 FD/consumer 對新 session 生效。endpoint 結束時先標 inactive、解除 consumer 並清除輸入/raw 狀態；既有 OFD refs 仍可安全釋放，但讀寫會回錯，slot 直到最後一個 ref 離開才回收。callback 呼叫後重新驗證 handle、active 與 consumer/context，避免 callback 關閉或重用 slot 後寫到新 endpoint。輸入、輸出 ring 都以每端點 read/write counters 索引；輸出先進 endpoint ring，再由已綁定 consumer 分段批次 drain，滿載回短寫／錯誤，不跨端點 fallback。raw owner 與 canonical CR/LF 過濾狀態也依 endpoint 查找。
- 核心流程虛擬碼：

  ```text
  resolve(id):
      slot = low32(id) - 1; generation = high32(id)
      if slot 越界或 endpoint 未使用或 generation 不符: 回 stale/error
      return endpoint[slot]

  create_session(consumer, context):
      找未使用且 generation 未達上限的 slot
      generation += 1; 初始化獨立 queues/mode/consumer; refs = UI owner 1
      回傳 (generation << 32) | (slot + 1)

  attach_process(id):
      建立 input OFD、output OFD，各 retain(id) 一次
      全部成功後再替換 fd 0/1/2；fd 1 和 fd 2 指向同一 output OFD
      失敗則釋放新 OFD，舊 fd table 保持不變

  close_session(id):
      先 drain 可交付 output
      endpoint.active = false; consumer = null; 清 input/raw owner
      阻塞讀者輪詢後以 ERR_IO 喚醒；UI 發現 inactive 後 release owner ref
      UI owner 與所有 OFD refs 都釋放時才清 slot；下次重用 generation 再加一
  ```

- 驗收結果：增量 `make -q` 完成，無待重建目標；QEMU GDB 以兩個真實 shell endpoint 驗證 generation ID、各自 input ring、raw owner/CRLF 隔離、fd 1 跨端點路由、dup/fork OFD ref 計數。兩個 shell 均進入 `WAIT_IO` 後關閉 A，GDB 觀察 A 以 `-5` 喚醒、B 仍為 `WAIT_IO`；舊 handle 失效，slot 最後釋放後重用時 generation 遞增。GUI QMP 驗收確認 pipe、檔案重導向與讀回、Fan 儲存/退出、workstate 返回、視窗隱藏後重開保留 session，以及 shell exit 後新 session。UART-only QEMU 確認 shell、pipe 與檔案重導向/讀回。停機後 host 唯讀驗證兩個 scratch WUFS metadata 均有效，`tty.txt` 與 `uart.txt` 內容為 `persist\n`。原始 `build/wuwaos.img` 與 swap hash 未變。
- 驗收記錄：`build/tty-phase1-runtime-gdb.log`、`build/tty-phase1-gui-session.log`、`build/tty-phase1-gui-apps.log`、`build/tty-phase1-session-lifecycle.log`、`build/tty-phase1-pipe.log`、`build/tty-phase1-uart-serial.log`；互動畫面為 `build/tty-phase1-hidden.png`、`build/tty-phase1-reopen-same.png`、`build/tty-phase1-new-session.png`、`build/tty-phase1-pipe-redirect.png`、`build/terminal-fan-active.png`、`build/terminal-fan-saved.png`、`build/terminal-workstate-running.png`、`build/terminal-query-routing.png` 及 `build/wuwaos-terminal-workstate-return.png`。
- 限制：GDB fork/ref 檢查確認 fork 增加 OFD references，但該子行程在父 session close 測試前被清除，因此未驗證「leader 結束、存活 child 繼續持有同一 endpoint OFD」整段生命週期。UI 尚未支援同時顯示多視窗；本階段只完成核心多 endpoint 物件和單一 view 綁定。

## 2. 統一事件等待與非同步 I/O

- 整體進度：進行中；此階段分三個子步，2A 完成不可代表整個階段完成。

### 2A. 事件框架、TTY/pipe 等待與 idle

- 進度：完成實作與本階段驗收；第 2 階段整體仍進行中，2B1 已完成，2B2 暫停，2C 未開始。
- 實作內容：`kernel/proc/event.c` 提供帶 generation/sequence 的 event source 與 per-process WaitRecord；完成、deadline timeout、cancel 結果均存於 PCB。TTY/pipe 改由原 syscall caller stack try/wait/retry；scheduler `io_poll` 僅 harvest UART 並通知事件，不做 usercopy／VM／swap。kill 對仍持有 kernel call stack 的行程延後至安全 unwind。移除舊 `process_block_io`／`process_complete_io`、`WAIT_IO`／`WAIT_TIMER` 及不再使用的 PCB io buffer 欄位；`pcb` 改顯示實際 WaitRecord 與 kill 狀態。Idle 在 SIE=0、sie.STIE=1 下以有限 SBI 絕對 deadline 加 WFI；醒來 stop/ack timer 後再 poll。
- 解決觀念與演算法：source 在固定 storage 中保存 generation、sequence、active/retired；init 只允許 inactive source 以更大 generation 重用，generation／sequence 不允許繞回。waiter 註冊前先 snapshot sequence，再檢查 readiness；原核心不可搶佔區內若 source 已關閉或 generation 不合回 `ERR_IO`，若 sequence 已改變立即回 0 讓 caller 重試，否則寫入 PCB WaitRecord、改 WAIT_EVENT 並切換。notify 先遞增 sequence，再以 source pointer+generation 找相符 waiter，將結果存入 PCB 並喚醒；close 在 source 失效前以 `ERR_IO` 完成既有 waiter。恢復後只讀 PCB result 並清 WaitRecord，不再解參考 source，故即使 slot 已重用亦不 alias。deadline 掃描以硬體 time 做 wrap-safe 到期比較；只有無 Ready process 且仍有 event/UI poll work 才以固定短絕對 deadline WFI，spurious wake 不重算延後原 deadline。
- Pipe read/write 演算法：

  ```text
  repeat:
      (generation, sequence) = snapshot(pipe.event)
      if read has no data and writers remain, or write has no space:
          result = wait_event(pipe.event, generation, sequence)
          if result < 0: return result
          continue
      amount = min(request, current readable data / free ring space, 256)
      if read:
          staging = peek ring bytes
          result = user_copy_to(caller, staging)
      else:
          result = user_copy_from(caller, staging)
      if result < 0: return error without changing ring counters
      if write: copy staging -> ring
      advance ring counters; notify(pipe.event); return amount
  ```

  staging 是 ready 後在 caller stack 建立，並不跨 wait 保留 payload；usercopy 成功後才 commit 讀寫 counters 並通知另一端，因此當前單 hart 無 async VM 下錯誤不會遺失資料或漏喚醒。2B 若讓 usercopy 可 yield，仍需 pipe gate／pinning，不能把此演算法視為已解決跨行程 race。
- Kill unwind 演算法：user trap 入口標記 `kernel_call_active`；kill 對 active stack 設 `kill_pending`，event wait 寫 `ERR_INTERRUPTED`，channel／child wait 清理 wait fields，sync wait 從等待鏈移除並喚醒。原 stack 恢復並正常返回 syscall/fault/timer trap 的 unwind 邊界時才清 active 並 terminate，釋放 OFD、sync、TTY 等資源；VM 則在之後安全的 process reap 階段回收，不由 kill/unwind 直接銷毀。event waiter 即使已完成並 READY 但尚未 unwind，仍保持 active/stack 生命週期並可安全取消。
- 驗收結果：`make -j2 all build/user.bin build/app.bin build/fan.bin build/workstate.bin build/kernel-disk.elf` 成功，隨後 `make -q` 無待建目標；未新增測試程式碼。fresh V2 scratch QEMU (`build/phase2a/`) 的 GUI QMP 實際執行 `echo pipe | cat`、整檔 redirect 與 `cat /home/user/phase2a.txt`，畫面讀回 `persist`。另執行 `cat /boot/kernel.elf | cat > /home/user/pipe4.elf`：GDB 在真實 caller syscall stack 觀察 writer `pipes[0].count=256, readers=1, writers=1` 時進入 pipe event wait；亦觀察 reader `count=0, readers=1, writers=1` 時進入 event wait，兩個 SP 均位於各自 `kernel_stack_top` 以下的行程核心堆疊。兩行程最後皆 exit 0。停機後 host 唯讀驗證 scratch WUFS metadata 有效，來源與目的檔皆 156,328 bytes 且逐位元組相同，SHA-256 均為 `6af9f7641202c8beb6bab0d44a7cf6d155687f71c0517aba789d73c6425b41f4`。GDB 在真實 syscall stack 確認 TTY `process_wait_event` caller 的 SP 落在 `kernel_stacks`；TTY input notify 使 WAITING waiter READY/completed；在 snapshot 與 register 中間注入 newline 後實際返回 0 且未註冊 wait，caller 取走換行後繼續下一 read；以 active gen=1 呼叫 gen=2 wait 回 `-5` 且未建立 WaitRecord。`demo sync` 的真實 `sleep_ms` deadline wait 正常 timeout/completion；`WAIT_SYNC` child 與 `WAIT_CHANNEL` worker 經既有 shell/QEMU operation 被 kill 後皆安全 unwind、exit/reap，workstate event waiter 被 kill 時 PCB 先 READY+completed/result=`-4`，再於返回邊界清理。GUI TTY 在真實 PID 2 WAIT_EVENT 時由 scheduler stack 呼叫 `tty_session_close`，waiter 被喚醒並存 `-5`；read syscall frame a0 為 `-5`，之後僅為清除該測試 shell，在 read 已 unwind 的 kernel-call-return 邊界標記 kill_pending，並非聲稱 closed shell 自行正常退出。UI 釋放舊 endpoint 後點圖示建立 PID 3 新 Terminal session。WFI GDB：前置 `sstatus&2=0`、`sie&0x20=0x20`、`sip=0`、`current_index=-1`、ready_count=0；WFI 醒來後 time 前進且 `sip&0x20=0x20`，`timer_stop` 後 `sip&0x20=0`，仍能再 arm 與接受 GUI input。UART input 事件/timeout亦以實際 shell堆疊與QEMU serial trace觀察。
- 驗收記錄：`build/phase2a/final-gdb.log`（真實 stack、sequence retry、stale generation、WFI前後CSR/time）、`build/phase2a/gdb-runtime.log`（deadline wait、WAIT_SYNC/WAIT_CHANNEL/event kill 互動）、`build/phase2a/close-event-gdb.log`（TTY close result）、`build/phase2a/pipe-block-gdb.log`（大檔 pipeline 的 writer-full、reader-empty 阻塞與 caller stack）、`build/phase2a/pipe-uart.log`（pipeline completion）、`build/phase2a/close-uart.log`、`build/phase2a/final-uart.log`、`build/phase2a-final-readback.png`、`build/phase2a-close-reopened.png`。scratch WUFS／swap 均與原始 `build/wuwaos.img`／swap 分開。原始disk SHA256=`6ba61b2e9e6c5af4931dd39d338619bf66bf25c5639bb916adc59b6b62fd4989`、swap SHA256=`f6ddb73f54cf434916200d752a0596b14da7d6534fe0f8cf454e6b1c02078355`，均保持原值。4 個 user binary 的 SHA-256 與第 1 階段基準相同。
- 限制與未執行項目：本子步不改 VirtIO block/GPU、WUFS transaction 或 VM eviction 為非同步；boot/pre-scheduler 同步 block 初始化保留單執行緒同步路徑。沒有對 `WAIT_CHILD` cancel 分支單獨做 runtime kill case；event cancel、WAIT_SYNC、WAIT_CHANNEL 已有代表性 runtime 驗收。沒有真斷電或 DMA timeout/cancel 驗收；若 DMA 未能安全取消，不能丟棄 request/buffer，timeout 仍 fail-stop，復原留待第 3 階段。WFI 驗收是 QEMU 單 hart/OpenSBI 環境，不代表所有 SBI/QEMU 平台皆同；UART 仍是 timer-polling，不是 UART interrupt。

### 2B. Owner-aware gate 與 Block/FS/VM 非同步

- 整體進度：進行中；2B 分為 2B1（共享狀態 gate 與呼叫脈絡安全）及 2B2（Block 非同步），完成 2B1 不代表 2B 整體完成。

### 2B1. 可重入 gate 與共享狀態保護

- 進度：完成；2B2 Block 非同步仍未開始，不能視為第 2 階段整體完成。
- 實作內容：`KernelGate` 以目前 PCB/PID 作 owner、遞迴深度和 event source；每個 OFD offset/data、每個 Pipe copy/commit 各自序列化。FS scope 保護 metadata、redo transaction staging、syscall 的 `fs_staging` 全段及 `io_open` 的查找/截斷/發布；`elf_load` 整段持有 FS scope，避免分段讀取混合不同 ELF 版本。VM scope 保護 address-space 建立、複製、fault、eviction、查詢等共享頁表與 resident metadata。console route 改存每個 Process。`fs_init` 仍是 boot-only 一次性 mount/format/replay，並非 runtime remount API；Block I/O 在本步仍同步。
- 取得與釋放演算法：

  ```text
  gate_enter(g):
      若目前 process 已是 owner: depth++，返回
      若 no-wait context: gate 空閒則取得，否則 BUSY
      重複:
          snapshot event sequence
          若 gate 空閒: 設 owner/depth，計入 PCB gate ownership，返回
          wait(event, snapshot)       // 不持有目標 gate
          若被取消: 返回 interrupted

  gate_leave(g):
      驗證 owner，depth--
      若 depth==0: 清 owner/accounting，notify 等待者

  owner 操作結束: 依巢狀順序逐層 leave
  ```

  一般鎖序為 OFD/Pipe → FS → VM → 單一 Block device。FS 的 `io_inode_is_open` 僅作 SIE=0 下有界 metadata scan，不等待 OFD gate，避免 FS 反向等待 OFD。Pipe 不持 gate 等 data/space event；ready 後 bounded staging copy，再 commit ring/counter 並 notify。這個 copy/commit 保護只涵蓋目前同步 VM 行為，2B2 前仍須保留 Pipe gate；未宣稱可安全跨非同步 VM wait 修改 ring。
- no-wait 與生命週期：boot pre-scheduler 可在立即取得 Gate 時做同步工作，但不允許睡眠；scheduler/UI poll runtime context 不論有無 current process 都不可等待。VM allocation 在此 context 若需 eviction，會在改 PTE 或提交 I/O 前回 BUSY。scheduler reap 用 `vm_try_destroy` 延後重試。`cancel_defer_depth` 保護 cleanup 和未來 DMA critical section；kill/cancel 只標記 pending，deferred cleanup 等待 Gate、釋放資源後才由 trap return boundary 處理；trap return 檢查 defer/gate ownership 必須平衡。user page fault 在 VM wait 回錯後也先走該 boundary，pending kill 保留 -9 語意，普通 page fault 保留原錯誤。
- 驗收結果：clean/full rebuild 未執行；本輪增量命令 `make -j2 all build/user.bin build/app.bin build/fan.bin build/workstate.bin build/kernel-disk.elf` 成功。以獨立 scratch WUFS image 啟動 GUI QEMU，shell 執行 `echo gate > /home/user/gate.txt`、`cat /home/user/gate.txt`、`cat /boot/kernel.elf | cat > /home/user/pipe.elf`、讀取該檔及 `workstate q`；UART 顯示 child PID 7 正常以 status 0 結束，parent 收集並回收 child。此 smoke 確認主要 syscall/FS/pipe/VM 路徑可運作，不等同大檔 byte-for-byte host 比較。
- 未執行：依使用者「確認成功即可、不要花時間測試一堆」指示，未跑 Gate contention/cancel、cleanup defer、NEW-process inspect、poll no-wait eviction 等 GDB 個別邊界案例；也未跑 clean build 或完整 Fan/exec/fork 矩陣。這些不作為已驗證項目。原始 disk/swap 未掛載或修改；本階段未修改任何測試程式碼。

### 2B2. Block 非同步 request

- 進度：暫停；僅完成 `drivers/virtio/block.c` 的 pointer refactor，async request 尚未開始。使用者要求先處理文件與網站工作。
- 已完成方法與編譯：移除 `active` selector 與欄位 alias macros；`init_device`、`transfer`、flush helper 明確使用 `BlockDevice *`，各裝置 queue index/header/status 由自身物件持有。`make build/obj/drivers/virtio/block.o` 成功。
- 未完成／NOT_RUN：per-device async Gate、completion event、request 狀態與 bounce buffer、scheduler poll callback、runtime no-wait submit guard、VM async swap 撤銷/回復 PTE 尚未實作；因此未跑 async runtime/QEMU/GDB 驗收，也未編譯整合後的核心。
- 下一步：恢復程式工作後，先加入明確 runtime-start query 與 block poll hook，實作 per-device request submit/poll/collect 和 boot synchronous fallback，再調整 swap/VM eviction 的取消與 PTE rollback 語意；每步只做必要編譯，依最新使用者指示暫不跑額外驗收矩陣。
- 實作內容：移除 runtime global active-device request selector；每個 BlockDevice 保存獨立 request/header/status/buffer ownership、gate 及 completion event；scheduler poll 只觀察 VirtIO used ring 並發布完成，不進入 FS/VM/usercopy。
- 方法與完成判準：每個裝置至多一個 in-flight request，request/buffer 在 completion 前不可釋放或重用。排隊 gate wait 可取消；DMA submit 後進入不可取消 critical depth，kill 只標 pending，待裝置安全完成、清理 request 並逐層釋放 gate 後才於 trap boundary terminate。timeout 目前維持 fail-stop，不提前返回並重用可能仍被 DMA 存取的 buffer；裝置復原留待第 3 階段。
- 設計前置風險：VM gate 單獨不足以保護即將 swap-out 的 user frame，因其他 U-mode process 仍可能存取被驅逐的實體頁；非同步 swap-out 前須 revoke victim PTE 並 `sfence`，保留 RAM/bounce snapshot，失敗時 restore 原 PTE，且 scheduler reap 的 `vm_destroy` 只能 try/defer，不能睡眠等待 VM gate。Async FS/VM 也會讓 pipe ring 在 usercopy yield 時、共享 OFD offset 在檔案 I/O yield 時被其他 process 改動，因此需 pipe/OFD ownership gate，或在 gate 外先 copy/pin、gate 內 commit；只替 FS/VM 加 gate 不夠。核心診斷 route 應改為 per-Process，避免跨行程的 set/reset 互相清除。建議鎖序：OFD/pipe → FS（含 syscall fs_staging 外層）→ VM → Block；FS 不反向取得 OFD，scheduler poll/reap 不得睡眠。DMA 已提交後的通用 event cancel 不可立即 unwind／釋放 device gate 或 swap slot；2B 要定義不可中斷區或 critical depth，kill 延後至 DMA safe completion 後清理，排 gate 時才可取消。timeout 需裝置安全停止／完成後再發布，否則沿用 fail-stop；UI poll 即使從 process_resume 呼叫也需明確保證 nonblocking，不能等待 VM gate。這些尚屬 2B 設計待辦，未在 2A 實作或驗收。

### 2C. GPU 非阻塞 present

- 進度：未開始；須待 2B 共用事件/取消語意穩定。
- 實作內容：將 GPU transfer/flush 拆成 request state machine，由 UI 保留 dirty 區域直到 completion，GPU command/response buffer 在完成前有明確 owner。
- 方法與完成判準：scheduler/UI poll 不 busy-wait；錯誤及 timeout 時不重用 DMA 仍可存取的記憶體。本步只實作安全非阻塞與錯誤傳遞，不宣稱 device reset 復原；復原策略屬第 3 階段。

## 3. GPU 錯誤降級與裝置復原

- 進度：未開始。
- 實作內容：處理 GPU 錯誤並保留 dirty 區域以便重畫，建立裝置復原與明確診斷路徑。
- 方法與完成判準：DMA／佇列確認安全停止後才 reset/recover，否則保守停用裝置或停機；驗證故障不會誤報成功、不會重用裝置仍在存取的緩衝區，並且磁碟提交後不盲目重試。

## 4. 資源所有權、引用回收與核心堆疊保護

- 進度：未開始。
- 實作內容：整理 TTY、事件與同步資源所有權／引用計數，驗證 exit/exec 清理，為核心堆疊加入 guard page。
- 方法與完成判準：透過實際頁表映射變更實作 guard page；驗證最後一個引用離開後回收資源，越界核心堆疊存取會觸發保護故障。

## 5. 視窗管理

- 進度：未開始。
- 實作內容：在 TTY/session 分離後加入多視窗管理：建立／關閉、焦點、拖曳、縮放、遮擋與合成。
- 方法與完成判準：讓各視窗分別綁定 endpoint；驗證視窗顯示、焦點輸入及畫面更新互不混淆。

## 6. 檔案原子替換、錯誤語意與統一診斷

- 進度：未開始；此工作不屬於第 1 階段。
- 實作內容：整理檔案安全暫存與原子替換路徑、寫入錯誤語意，統一核心與裝置診斷資訊。
- 方法與完成判準：替換結果只呈現完整舊版或完整新版；提交後回錯時，明確回報結果不確定狀態及診斷資訊。

## 文件與網站同步（程式實作暫停期間）

- 目標與範圍：依目前原始碼同步程式碼導覽、主題章節、README、網站首頁與歷程摘要；保留舊規格驗收、特殊事蹟與媒體，不修改核心程式。
- 方法：以第 1 階段、2A、2B1 已實作內容為現況，將 2B2 pointer refactor 與尚未實作的非同步 request／復原清楚區分。細節連回本計畫，避免網站另維護一份不同步的演算法紀錄。
- 步驟與進度：第一批導覽／主題文件已以 `1e08d74` 提交並 push；第二批 README／首頁／歷程摘要與本計畫同步於本批提交。既有工具已將網站建置並發布到 gh-pages `1f99d02`，公開頁面已更新。
- 驗收方式：文件建置與線上頁面內容快速確認；本輪不新增測試程式碼，不重新執行 OS runtime／QEMU／GDB 或測試矩陣。
- 實際結果：VitePress 建置成功（僅有 bundle size 提示）；公開首頁 HTTP 200，已包含「目前開發進度」與 2B2 暫停內容，並保留「特殊事蹟」。此次只核對線上 HTML 內容，未做瀏覽器視覺驗收，未重跑 OS 測試。

## 進度維護

每完成一個步驟後，更新本檔與 [`plan_current.md`](./plan_current.md) 的進度。依使用者要求，本檔須記錄實際採用的解決觀念／演算法、必要虛擬碼、驗收結果與限制。未執行或未完成對應 runtime 驗證的項目不得記為完成；若當前條件無法進行 runtime 驗證，需明確保留未完成狀態並記錄限制。

依使用者最新指示，優先實作、更新進度並分階段 commit/push；額外測試暫緩。只記錄實際做過的 build/smoke，未執行項目標示 NOT_RUN，不宣稱全面驗收。
