# 後續實作計畫

目前整體狀態：第 1 階段、2A 與 2B1 有既有驗收紀錄；2026-11-08 的 2B2、2C 與第 3–6 階段已實作並完成一次必要整合編譯，未經 runtime 驗證。依使用者指示不執行測試、不撰寫測試程式碼，不以編譯成功代替功能驗收。演算法、各階段提交及限制見本檔末段。Spec 33 的原始碼分層工作已完成。

## CLI 磁碟開機（2026-11-09）

- 已實作，並在 Windows PowerShell、WSL Ubuntu、QEMU 8.2.2 與 OpenSBI 1.3 驗收。
- 保留主機交叉編譯。從專案根目錄的 PowerShell 執行 `wsl -d Ubuntu -- make run-disk`，由 QEMU `virt` 的 OpenSBI 進入唯讀 flash 載入器，從一顆 WUFS0002 磁碟載入 `/boot/kernel.elf`，最後進入 UART shell。
- 沿用 `tools/make_disk.py` 安裝核心與保存檔案。載入器在 `0x20000000` 的 flash 中執行，從 QEMU `virt` 的八個 MMIO slot 找出單顆 modern VirtIO block。它輪詢讀取 WUFS 目錄、inode 與直接／間接區塊，先驗證所有 ELF 載入區段，再複製檔案內容及清零 BSS。裝置 reset 完成後，將原始 hart ID 與裝置樹交給核心入口。
- 預設磁碟 `build/wuwaos-cli.img` 與新建驗收映像都啟動至 `wuwaos/>`，`echo disk-boot-ok` 與 `ls /boot` 回傳預期結果。用同一顆驗收磁碟重啟並讀回先前寫入的檔案，重啟時直接使用韌體與磁碟。QEMU 參數沒有 `-kernel`；移除磁碟時，載入器在核心啟動前顯示 `boot: virtio-blk init failed`。序列記錄見 `build/disk-boot-acceptance.log`、`build/disk-boot-final-fresh.log` 與 `build/disk-boot-final-reboot.log`。
- 範圍為 QEMU `virt`、單 hart、128 MiB、唯讀載入器與一顆 WUFS0002 磁碟。CLI 磁碟開機要求 journal 無待重播紀錄，且此入口停用 swap。其他平台及損毀映像未進行實機驗收。

### 文件與網站同步

- 更新 README、入門與開機章、首頁架構圖、程式碼導覽與歷程。標明 `run-disk` 的磁碟開機與 `run`／GUI 的直接開機，舊規格保留當時驗收。同步移除現行文件對磁碟核心與系統內編譯的過時說明。
- 驗收範圍是來源差異、網站建置、關鍵連結及 GitHub Pages。從乾淨的來源提交發布，避免將既有未提交的文件修改帶入網站。
- VitePress 發布設定下的本機建置通過。修正排程與記憶體兩章的課程筆記連結，改指 GitHub 上對應檔案，保留 VitePress 的死連結檢查。這次文件同步沿用 2026-11-09 的 OS 驗收紀錄。
- 文件來源提交 `8b8ce26` 已推送至 `main`。從該提交的乾淨工作樹建置並推送 `gh-pages` `4725bb6`，GitHub Pages 部署成功。公開首頁、入門、開機、歷程及 journal 復原頁均回傳 HTTP 200，並核對更新內容及歷程／復原錨點。 [部署紀錄](https://github.com/johnson931128/WUWAOS/actions/runs/42569717948)

## 1. 多 TTY 與 session 物件化

- 進度：完成；本階段驗收時 UI 為單一 Terminal view，後續第 5 階段已實作四視窗（runtime NOT_RUN）。
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

- 進度：完成實作與本階段驗收；2B2／2C 後續已實作，但不沿用此處的 runtime 驗收。
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

- 進度：完成；此處保留 2B1 當時的同步 Block 驗收，後續 2B2 非同步另記實作與未驗證範圍。
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

- 進度：2026-11-08 功能已實作，整合編譯成功，runtime NOT_RUN。Block／VM agents 按本檔末段演算法分工；先前暫停點為 pointer refactor。
- 已完成方法與編譯：移除 `active` selector 與欄位 alias macros；`init_device`、`transfer`、flush helper 明確使用 `BlockDevice *`，各裝置 queue index/header/status 由自身物件持有。`make build/obj/drivers/virtio/block.o` 成功。
- 本輪成果：per-device Gate／completion event／bounce buffer、request state、scheduler poll、runtime no-wait guard 與 VM swap PTE 撤銷／回復皆已實作；提交 `7e0f826`、`eb84ff7`，故障隔離另見 `19be53a`。只完成編譯與靜態 review，未跑 async runtime／QEMU／GDB 驗收。
- 後續限制：boot 保留同步 fallback，每裝置一次一筆請求；不是多 queue block scheduler。安全 reset 後隔離裝置，不自動重試或 runtime remount；已換出且只存在故障 Swap 裝置的頁面仍可能無法讀回。
- 實作內容：移除 runtime global active-device request selector；每個 BlockDevice 保存獨立 request/header/status/buffer ownership、gate 及 completion event；scheduler poll 只觀察 VirtIO used ring 並發布完成，不進入 FS/VM/usercopy。
- 方法與完成判準：每個裝置至多一個 in-flight request，request/buffer 在 completion 前不可釋放或重用。排隊 gate wait 可取消；DMA submit 後進入不可取消 critical depth，kill 只標 pending，待裝置安全完成、清理 request 並逐層釋放 gate 後才於 trap boundary terminate。timeout 目前維持 fail-stop，不提前返回並重用可能仍被 DMA 存取的 buffer；裝置復原留待第 3 階段。
- 設計前置風險：VM gate 單獨不足以保護即將 swap-out 的 user frame，因其他 U-mode process 仍可能存取被驅逐的實體頁；非同步 swap-out 前須 revoke victim PTE 並 `sfence`，保留 RAM/bounce snapshot，失敗時 restore 原 PTE，且 scheduler reap 的 `vm_destroy` 只能 try/defer，不能睡眠等待 VM gate。Async FS/VM 也會讓 pipe ring 在 usercopy yield 時、共享 OFD offset 在檔案 I/O yield 時被其他 process 改動，因此需 pipe/OFD ownership gate，或在 gate 外先 copy/pin、gate 內 commit；只替 FS/VM 加 gate 不夠。核心診斷 route 應改為 per-Process，避免跨行程的 set/reset 互相清除。建議鎖序：OFD/pipe → FS（含 syscall fs_staging 外層）→ VM → Block；FS 不反向取得 OFD，scheduler poll/reap 不得睡眠。DMA 已提交後的通用 event cancel 不可立即 unwind／釋放 device gate 或 swap slot；2B 要定義不可中斷區或 critical depth，kill 延後至 DMA safe completion 後清理，排 gate 時才可取消。timeout 需裝置安全停止／完成後再發布，否則沿用 fail-stop；UI poll 即使從 process_resume 呼叫也需明確保證 nonblocking，不能等待 VM gate。這些尚屬 2B 設計待辦，未在 2A 實作或驗收。

### 2C. GPU 非阻塞 present

- 進度：2026-11-08 已實作（`ea6671e`），整合編譯成功，runtime NOT_RUN；CPU shadow 與 DMA backing 分離，poll 驅動 transfer／flush，後續 dirty 合併待下一次。
- 實作內容：將 GPU transfer/flush 拆成 request state machine，由 UI 保留 dirty 區域直到 completion，GPU command/response buffer 在完成前有明確 owner。
- 方法與完成判準：scheduler/UI poll 不 busy-wait；錯誤及 timeout 時不重用 DMA 仍可存取的記憶體。本步只實作安全非阻塞與錯誤傳遞，不宣稱 device reset 復原；復原策略屬第 3 階段。

## 3. GPU 錯誤降級與裝置復原

- 進度：2026-11-08 已實作（Block `19be53a`、GPU `e48845b`），整合編譯成功，timeout／reset／復原結果 NOT_RUN。
- 實際方法：Block 逾時或 malformed completion 進 RESETTING，poll 逐次確認 status=0，100 ms 仍未確認則 fail-stop；確認後 offline 並完成錯誤，不重試寫入。GPU 最多兩次 reset 重建，逐 poll 完成協商／create／attach／scanout，成功後全畫面重送；只有 present flush 成功才重置 retry 計數。耗盡重試停用 GUI，保留可能被 DMA 存取的 storage，UART 路徑保留。`6ef3029`／`3bc436d` 將關鍵診斷改為 raw UART，避免 poll 進入 TTY callback。
- 實作內容：處理 GPU 錯誤並保留 dirty 區域以便重畫，建立裝置復原與明確診斷路徑。
- 方法與完成判準：DMA／佇列確認安全停止後才 reset/recover，否則保守停用裝置或停機；驗證故障不會誤報成功、不會重用裝置仍在存取的緩衝區，並且磁碟提交後不盲目重試。

## 4. 資源所有權、引用回收與核心堆疊保護

- 進度：2026-11-08 已實作 stack guard（`b555d29`），整合編譯成功；生命週期只做靜態 review，runtime NOT_RUN。
- 實際方法與限制：process/scheduler 各保留一頁 4 KiB 未映射 guard + 16 KiB 可用 stack，共享 kernel mapping 對所有 process roots 生效。只清 stack，避免寫 guard。TTY/event/OFD/exit/exec 沿用已有 close/ref/defer 邏輯，本輪未聲稱全面證明所有回收邊界。C trap handler 可辨識 guard 並輸出 PID／範圍，但沒有 emergency trap stack；若 trap.S 儲存 TrapFrame 時已踩 guard，仍可能遞迴 fault，不能保證診斷可達或復原。
- 實作內容：整理 TTY、事件與同步資源所有權／引用計數，驗證 exit/exec 清理，為核心堆疊加入 guard page。
- 方法與完成判準：透過實際頁表映射變更實作 guard page；驗證最後一個引用離開後回收資源，越界核心堆疊存取會觸發保護故障。

## 5. 視窗管理

- 進度：2026-11-08 已實作（`05b8124`），整合編譯成功，GUI／輸入／多視窗 runtime NOT_RUN。
- 實際方法：固定 4 個 view slot，各有 TerminalModel／TTY handle／geometry，z-order 底到頂合成、頂到底 hit-test。點圖示建立新 window，點視窗置頂／focus，標題 drag／右下角 resize 夾限畫面；鍵盤 keymap 全域保存但只向 focused TTY 投遞。重畫整個 shadow scene，再提交 dirty 聯集，避開複雜遮擋增量演算法。
- 關閉與限制：close 改為關閉 session、解除 consumer、釋放一次 UI ref；shell 遇到 ERR_IO／ERR_BAD_FD 退出，避免關閉後無限 yield 重試。shell exit 後畫面保留至手動 close。resize 是固定 100×30 VT 的裁切 viewport，不重排文字、沒有 TTY resize ABI／捲動檢視／最小化；放大不增加列數。建立 shell 仍受全域 PCB/OFD/記憶體容量限制。
- 實作內容：在 TTY/session 分離後加入多視窗管理：建立／關閉、焦點、拖曳、縮放、遮擋與合成。
- 方法與完成判準：讓各視窗分別綁定 endpoint；驗證視窗顯示、焦點輸入及畫面更新互不混淆。

## 6. 檔案原子替換、錯誤語意與統一診斷

- 進度：2026-11-08 已實作（`496b757`），整合編譯成功，儲存／取消／故障／重播 runtime NOT_RUN。
- 實際方法：`fs_replace`／`SYS_FS_REPLACE=41`／`fs_replacefile` 在 FS scope 與同一 redo transaction 中配置新 inode/data，完整 staged 後才切換 dir entry、回收舊 inode；目標 open 回 BUSY，directory 拒絕，容量超出則整筆 abort。需要保留新舊資料所需空間，仍受 1152-sector journal 上限限制。Fan save 使用此 API。
- Commit 結果：cancel-defer 跨完整持久化 commit；header 寫嘗試前錯誤為 ERR_IO，開始嘗試 header 後任一失敗保守回 `ERR_UNCERTAIN=-200` 並 FS offline。Fan 保留 dirty buffer、阻止再次 save，要求重啟／重新開啟確認，不把回錯當成一定未存檔。一般 FD write/O_TRUNC 仍是各自交易，不提供整段 shell redirection rollback。統一診斷 header 直出 raw UART（subsystem／operation／error／uncertain／detail），Block/GPU/FS 關鍵故障已接入。
- 實作內容：整理檔案安全暫存與原子替換路徑、寫入錯誤語意，統一核心與裝置診斷資訊。
- 方法與完成判準：替換結果只呈現完整舊版或完整新版；提交後回錯時，明確回報結果不確定狀態及診斷資訊。

## 前次文件與網站同步（2026-11-07，程式實作暫停期間）

- 目標與範圍：依目前原始碼同步程式碼導覽、主題章節、README、網站首頁與歷程摘要；保留舊規格驗收、特殊事蹟與媒體，不修改核心程式。
- 方法：以第 1 階段、2A、2B1 已實作內容為現況，將 2B2 pointer refactor 與尚未實作的非同步 request／復原清楚區分。細節連回本計畫，避免網站另維護一份不同步的演算法紀錄。
- 步驟與進度：第一批導覽／主題文件已以 `1e08d74` 提交並 push；第二批 README／首頁／歷程摘要與本計畫同步於本批提交。既有工具已將網站建置並發布到 gh-pages `1f99d02`，公開頁面已更新。
- 驗收方式：文件建置與線上頁面內容快速確認；本輪不新增測試程式碼，不重新執行 OS runtime／QEMU／GDB 或測試矩陣。
- 實際結果：VitePress 建置成功（僅有 bundle size 提示）；公開首頁 HTTP 200，已包含「目前開發進度」與 2B2 暫停內容，並保留「特殊事蹟」。此次只核對線上 HTML 內容，未做瀏覽器視覺驗收，未重跑 OS 測試。

## 2026-11-08 剩餘階段執行規格與實際演算法

以下是本輪開始前先行定案的實作方案；實際成果、提交與限制另見各階段及收尾紀錄，不等同已驗收。目標為實現剩餘階段的功能；不擴充網路、POSIX ABI 或任意硬體支援。Luna high agents 按檔案所有權平行工作，各階段自行提交精確檔案；主代理負責依賴整合、review、必要修正、分批 push，最後更新文件網站並部署。

### 共用約束與依賴

- Block agent：`drivers/virtio/block.*` 與 `kernel/proc/process.*` 的 runtime query／poll hook。VM agent：`kernel/mm/vm.*`、swap 相關與後續 guard API。GPU/UI agent：`drivers/virtio/gpu.*`、`ui/desktop/*`、terminal model/renderer。FS 階段在 Block agent 完成後接續，避免同時改同檔。
- 既有 gate 次序 OFD/Pipe → FS → VM → Block；poll 不進 FS/VM、不 usercopy、不睡眠。Boot 允許同步初始化，runtime no-wait context 在 submit 之前回 BUSY。
- 2B2 的 Block 與 VM 撤銷保護必須一起整合後，才能把 runtime 路徑視為已實作。GPU 2C 完成後才接入復原與多視窗。第 4 階段修改 process stack 時須等 Block agent 釋放 `process.*`。
- 不跑單元、整合、QEMU、GDB、操作或故障注入測試，不新增任何測試程式碼。只允許整合所必需的編譯及文件發布建置；所有 runtime／視覺／故障復原結果標示 NOT_RUN。Review 只檢查實際差異與相依介面，不宣稱可執行驗收。

### 2B2：每裝置單一 request、completion poll 與 swap 撤銷

- BlockDevice 長期保存 Gate、event、512-byte bounce buffer、header/status、request state、deadline/result。狀態為 IDLE → IN_FLIGHT → DONE → IDLE，timeout 不得直接重用 buffer。對外 read/write/flush 保留同步函式介面，但 runtime caller 在內部 event wait 讓出 CPU。
- 寫入先把 caller bytes 搬到 device bounce，讀取完成後才從 bounce 複製回 caller；裝置不直接 DMA 到可回收的 user frame 或暫時 stack。每裝置 Gate 持有至 collect 結束，wait 排隊可取消；submit 前進入 cancel-defer，直到 completion、collect、離開 Gate 後才結束 defer。
- runtime 開始後，沒有可等待的 process context 一律在任何 queue mutation 前回 BUSY；boot/pre-scheduler 用同一 submit/poll/collect 狀態機同步輪詢。scheduler source poll 掃兩顆裝置，僅 harvest used ring、檢查 status/deadline、更新 state 並 notify，不遞迴呼叫高層 I/O。

  ```text
  transfer(device, op, caller):
      reject runtime no-wait context
      enter device gate; begin cancel-defer
      prepare persistent header/status/bounce; snapshot completion event
      publish descriptors + avail index with memory fences; state = IN_FLIGHT
      while state == IN_FLIGHT:
          runtime: wait_event(snapshot); refresh snapshot
          boot: poll_device()
      collect result; successful read copies bounce -> caller
      state = IDLE; leave gate; end cancel-defer; return result

  poll_device():
      if used index advanced: acquire fence; validate head/status; state = DONE; notify
      else if deadline reached: retain DMA storage; fail-stop (until stage 3 safe reset)
  ```

- VM eviction 持 VM scope 選 victim，保存完整 PTE，先 clear PTE 並 `sfence.vma`，pin victim 再做可能睡眠的 swap I/O。其他行程碰到該頁會進 fault 並等 VM Gate；不能趁 DMA 期間寫 victim。成功才公布 swap metadata、釋放 frame；失敗恢復原 PTE／sfence、保留 dirty RAM，釋放新配 slot。整段撤銷至 restore/commit 使用 cancel-defer，避免 kill 在還原前截斷流程。既有 swap slot 失敗可能部分覆寫，但 RAM 仍為唯一有效副本，必須保持 dirty，不能視該 slot 為乾淨 backing。

### 2C：GPU 非阻塞 transfer／flush

- GPU 長期保存 request/response、in-flight rectangle、start/deadline；runtime `gpu_present` 排入 dirty rectangle，不在 UI poll 忙等。poll 狀態機 IDLE → TRANSFER → FLUSH → IDLE；每次 poll 最多完成有限步數，未完成直接返回。
- CPU 繪製 shadow framebuffer，DMA backing 為獨立固定記憶體；IDLE 才把 dirty rectangle 從 shadow 複製到 backing，TRANSFER/FLUSH 期間不覆寫 backing，後續 dirty 合併待下一次 submit。完成 flush 才清除 in-flight ownership；錯誤保留重畫需求。
- Boot resource setup 可用有界同步 command；runtime 不將 stack 指標放入 descriptor。UI 需持續呼叫 gpu_poll，即使當下沒有新 dirty。

### 第 3 階段：安全停止、GPU 降級／復原與 Block 隔離

- timeout 或 malformed completion 先進 RESETTING，寫 VirtIO status=0 並在有界期限觀察 status==0，取得 reset acknowledgement 後才允許清 queue 或重用 DMA storage。未確認 reset：GPU 停用但保留所有 buffer；Block 維持 fail-stop，不能偽造安全取消。
- GPU reset 成功後重新協商、建 queue/resource/scanout，保留 shadow framebuffer 並要求全畫面重送；重試次數有界，失敗降級為 UART 可用、GUI disabled，不讓 scheduler 無限忙等。恢復 poll 各步有界，不從 UI callback 進入同步等待。
- Block reset 確認後將該裝置標 offline，以錯誤完成原 request 並喚醒 caller，不自動重送寫入、不 runtime remount；FS 原有 offline/journal boot replay 決定重啟後內容。寫入結果可能不確定，需留給第 6 階段統一錯誤語意。Swap 失敗由 VM 還原 RAM/PTE。

### 第 4 階段：生命週期與真實 guard page

- 逐一檢查 exit/exec、TTY owner/OFD refs、event source close 與 gate/defer balance；只修正實際 ownership 缺口，不增加通用大型框架。TTY generation/refs 保持原設計，window 的 owner ref 只釋放一次，callback detach 後才可重用 view storage。
- 每 process stack 改為 page-aligned storage：一頁 guard + 16 KiB stack；scheduler stack 同樣配置 guard。boot 建立共用核心頁表後移除 guard 虛擬頁的 leaf PTE，所有 process root 共用該核心 mapping。初始化只清 stack 範圍，絕不 memset 已取消映射的 guard。
- guard 是實際不可存取映射，非 canary；正常 stack top/trap frame layout 不變。trap 診斷辨識 guard 地址後輸出目前 PID／stack 範圍並停止，避免故障 handler 遞迴破壞記憶體；不宣稱能從已耗盡的 stack 恢復。

### 第 5 階段：有界多視窗、焦點、拖曳與縮放

- 固定上限 window slots，每 slot 保存獨立 TerminalModel、TTY handle、幾何、visible/focus、session 狀態；z-order 陣列由底到頂排列，輸入從頂往底 hit-test，鍵盤只送 focused TTY。每次點 Terminal icon 建立新視窗，容量滿時有明確回饋。
- 點 body/title 置頂並 focus；title drag 以滑鼠與原始位置差更新，clamp 在 desktop；resize grip 更新有界幾何。縮放若先採固定 100×30 內容的 viewport／顯示縮放，必須明確記錄，不能宣稱已有 TTY resize ABI 或 shell SIGWINCH。
- 簡單軟體合成：有 dirty 時依序重畫 wallpaper → 所有可見視窗（底到頂、裁切）→ cursor，再交給非同步 gpu_present。先使用全畫面 dirty 保證遮擋／移動後背景正確，暫不做複雜區域最佳化。
- 關閉採明確語意：關閉視窗會關閉 session endpoint、解除 callback 並 release UI ref；已持 OFD 的程式讀寫收到錯誤，直到最後引用離開才釋放 endpoint。隱藏與關閉需有不同操作或只提供關閉，文件說清楚改變。

### 第 6 階段：單次交易的安全替換與診斷

- 新增 `fs_replace(path, bytes, length)`，在同一 FS scope／redo transaction 內建立新 inode/data、寫完整內容、原子切換 directory entry，再回收舊 inode。禁止替換 directory、拒絕仍被 open OFD 引用的目標（先保留簡單安全語意，不實作 unlink-open inode）。journal 容量不足時整筆 abort，舊名稱仍指向舊檔。
- 不使用可碰撞的暫存路徑：新 inode 在 transaction staging 內建立，只有完成才發布 dir entry。缺檔可建立；用 SYS_FS_REPLACE/user API 讓 fan 儲存採用此機制，既有一般 FD write 維持原語意。
- Commit 依序 journal payload → flush → commit header → flush → home writes → flush → clear header → flush。commit header 發布前失敗與發布後失敗分開回報；一旦 header write 嘗試開始而失敗，保守回 `ERR_UNCERTAIN`，FS offline，不能說操作一定未發生；重啟 replay 決定完整版本。kill 不可截斷已開始的持久性 commit，整段 commit 使用 cancel-defer。
- 統一最小診斷 API／格式，記錄 subsystem、operation、error、結果是否 uncertain、device/sector 或 PID；以 UART 輸出避免向失效 GUI 遞迴診斷。只整合 Block/GPU/FS 的關鍵故障，不為一般使用者流程加入內部實作術語。

### 收尾：review、文件、部署

- 每位 agent 完成階段後自行 commit 精確檔案，不 push，不將其他 agent 變更混入 commit。共用 plan 的狀態由主代理統一寫入，agent 回報實際演算法、檔案與限制以便合併。
- 主代理 review 跨階段依賴：poll context、gate order、DMA lifetime、VM PTE rollback、window/TTY refs、FS commit uncertainty；必要修改後做一次整合編譯，不跑測試。
- 依實際程式更新 CODE_GUIDE、topics、README/index/history、plan_current/plan；保留舊規格證據、媒體與 portable v1 差異。以既有 GitHub Pages 工具建置部署；本輪網站不執行瀏覽器／操作測試，發布狀態與 build 結果如實記錄。

### 本輪實際執行紀錄

- 演算法規格先以 `0f1ec8b` 提交，再由三位 Luna high agents 依檔案所有權實作；各階段自行 commit，主代理分批 push。上述各階段條目列有對應提交及實際限制。
- 主代理 review 修正的功能依賴：Block 競爭者須先等 Gate 而非因 IN_FLIGHT 提前回錯；read IOERR 允許短 used length；reset ACK 後加 I/O acquire fence；GPU retry 計數要到成功 flush 才清；closed TTY 必須讓 shell 退出，避免無限重試占 PCB；關鍵 poll 故障診斷直走 UART，不進 TTY callback。這些均為靜態差異 review，非執行測試。
- 必要整合編譯：在 WSL Ubuntu 執行 `make -j2 all build/user.bin build/app.bin build/fan.bin build/workstate.bin build/kernel-disk.elf`，退出碼 0，kernel 與四個 user images 連結成功；沒有執行 disk-image／run 目標，不掛載或更新 guest 磁碟。
- 測試：依使用者明確要求，沒有執行測試、QEMU、GDB、故障注入、GUI／瀏覽器操作，也沒有新增測試程式碼。不得將上述功能稱為已通過 runtime 驗收。
- 文件／部署：CODE_GUIDE、主題章節、README、首頁與歷程已依實際功能同步；`fc3ddc9`、`4ea6061`、`ab1e003` 記錄導覽與內容更新。既有 `tools/publish-docs.ps1` 的 VitePress 建置成功（只有既有 bundle size 提示），發布 gh-pages `89420e0eda8aebb377c48154ecccb3ec4d9a25e5`；GitHub Pages [部署工作 42460237950](https://github.com/johnson931128/WUWAOS/actions/runs/42460237950) 已 completed/success。
- 部署確認僅讀取 GitHub workflow 狀態，未做公開頁面／瀏覽器操作測試。既有 screenshot、特殊事蹟與 portable v1 檔案未替換；新版功能只在目前 source，不在舊 ZIP。所有本輪修改已依階段提交並分批 push，最後以本計畫紀錄提交收尾。

## 文件互動化第一輪（2026-11-08）

- 目標：將網站由文字紀錄提升為「HTML/CSS/Vue 互動示意 + 詳細文字 + 原始碼對照」，先完成 Block Async I/O、FD/OFD、換頁／Swap、WUFS journal／安全替換四章，再作為其他章節的範本。本輪只改文件與網站，不改 OS。
- 視覺規則：大流程／狀態／資料搬移以 DOM/CSS 排版，可操作且支援窄螢幕；Mermaid 只留函式依賴與小流程。靜態生圖須先交使用者審閱，本輪不新增靜態圖；不直接轉載無明確公開重製授權的教科書圖片。
- 教學結構：具體問題 → 筆記概念 → 模組／資料位置 → 可單步互動 → 欄位與參數表 → 函式與回傳值 → 失敗／限制。可以引用使用者筆記敘述，標明檔名與章節；將筆記概念和 WUWAOS 實際實作分開。
- 動畫方法：每個情境是明確的教學 frame 陣列，frame 同時保存敘述、active actor、狀態及資料位置，不靠動畫時間猜測狀態。單步／前後／reset／scenario 切換更新同一索引；播放只以可清除 timer 推進，切換或離頁停止。預設不自動播放，尊重 reduced-motion；文字和狀態標籤不依賴顏色辨識。這些是示意模型，非 QEMU trace 或即時 OS 模擬。
- 第一章 Block：read/write／IOERR／timeout reset 成功或未確認／kill defer；顯示 caller／scheduler／device、Gate owner、bounce、request state 與 event；依實際 submit/poll/collect 與 reset 路徑建模。
- 第二章 FD/OFD：open／dup／fork／再次 open／read／close 的固定情境，顯示每行程 FD 表、共享 OFD offset/ref 與相同檔案；避免把兩次 open 畫成共用 offset。本輪不新增任意 C 程式直譯器。
- 第三章 paging：FIFO victim／pin／PTE revoke／swap I/O／成功回收或失敗 restore，顯示 page/PTE/frame/slot 的狀態；保留原有詳細章節並擴充 API/欄位表。
- 第四章 WUFS：staging → payload flush → commit header flush → home checkpoint → clear；區分 header 前 abort、header 後 uncertain/offline 與 reboot replay，不把回錯等同未保存。
- 執行／提交：Luna agents 分檔實作，每章（元件與該章文章）獨立 commit；主代理 review 模型與 source 一致性、整合導覽、寫作規約及計畫，再分批 push／部署。保留歷史規格、特殊事蹟、媒體與 portable v1 caveats。
- 建置／驗收：沿用使用者先前要求，不跑 OS 或瀏覽器操作測試、不新增測試程式碼；只做必要 VitePress 建置與 GitHub Pages 部署狀態確認。成功建置不代表互動／視覺已驗收，記錄 NOT_RUN。
- 進度：四章元件與詳細文字已實作並按章提交，導覽／首頁／寫作規約已接入；必要文件建置成功並完成部署。瀏覽器操作與視覺驗收 NOT_RUN。
- 章節提交：規格 `297c5b7`、導覽／規約 `35fd341`；FD/OFD `e171e58`，Block `083d5db`／`13fdebe`，paging `119c82f`，journal `261f6ac`。本輪只修改網站與文件，不修改 OS 或新增測試程式碼。
- 實際解決方式：各 Vue 元件以 typed/static frame arrays 儲存每步資料，computed 由 scenario/index 產生唯一當前畫面；DOM/CSS cards 表示行程／物件／資料位置，不產生大型 Mermaid。原生按鈕與 select 支援單步、播放／暫停、reset、切情境，timer 在結束／切換／unmount 清除；樣式沿用 VitePress 明暗色變數與手機堆疊、reduced-motion。
- Review 補正：Block 的裝置完成和 poll 喚醒分開，bounce 容量 512 bytes／alignment 16 bytes 分開、event wait 回傳 long，kill 示例限定父對直接子行程；FD refs 逐 FD 計數、TTY 不顯示假檔案 offset；Swap 第 3 sector 失敗不先演完 8 sectors，回復後可繼續掃候選，VM Gate 由外層 fault scope 才釋放；journal 的 header write attempt 與 durable commit 分開，replay 明示有效 header／payload／CRC 假設，不保證所有 uncertain 都可恢復。
- 整合調整：開啟 repository Markdown HTML 以局部 import Vue 元件，原 Spec 03 四個命令的角括號改用 inline code 保留文字顯示，未改寫歷史設計或驗收。保留既有截圖與特殊事蹟，沒有新增生成圖／教科書轉載圖片或外部元件套件。
- 建置與部署結果：`tools/publish-docs.ps1` 執行 VitePress 建置成功（既有 bundle size 提示），靜態網站已推送 gh-pages `45a1ac73cf4bf6ea6cc886caa055c3723820b4a3`；[GitHub Pages 工作 42462536432](https://github.com/johnson931128/WUWAOS/actions/runs/42462536432) completed/success。只讀取部署工作狀態，沒有執行瀏覽器／頁面操作測試、OS 測試或新增測試程式碼；動畫實際操作、手機與視覺效果仍未驗收。

## 全站 Visual-first Textbook 改版（2026-11-08）

- Spec：依使用者最新完整需求，改版首頁、12 個主章、4 個既有 labs、導覽、共用視覺／播放與 source mapping；只修改 docs／網站與計畫，不修改 kernel、driver、syscall。保留長篇文字、欄位表、歷史 spec、媒體、特殊事蹟與 portable v1 邊界。
- 最新驗證授權：Phase 5 明確要求網站視覺／互動／響應式驗收，取代前次「不做瀏覽器測試」在本輪網站上的限制；仍不跑 OS 測試、不新增測試程式碼。
- Phase 1 研究：已閱讀指定 xv6、OSTEP、1,000 Lines、VizTools、OS Lab 網頁／相關章節，對照本地 notes 與 source；研究矩陣見 [docs/redesign.md](./docs/redesign.md)，呈現規範見 [docs/visual-system.md](./docs/visual-system.md)。COD 僅確認官方 companion 與筆記／source 對照，不冒稱讀取受限內頁。所有圖原創，不直接複製教材圖。
- Phase 2：先做首頁 clickable architecture／roadmap、RR ready queue＋Gantt（教學 quantum 與 OS 50 ms 分開）、Sv39 bit layout／三級 walk／PTE／frame 組合（BigInt 計算索引、只模擬合法教學映射）；共用 theme tokens 與控制介面。
- Phase 3：提取 usePlayback／PlaybackControls，重畫四 labs 的系統圖，保留全部舊情境與重要文字；step arrays 表示資料、DOM/SVG 表示流程，不依賴動畫時序。
- Phase 4：其餘主章加入 A/B/C 適用原創圖，保留七段結構與 source／notes／外部閱讀；大型 SVG 支援局部放大，非所有圖都做 simulator。
- Phase 5：VitePress build＋瀏覽器實際驗收 desktop/mobile、light/dark、播放／單步／重設／scenario、RR quantum／VA、zoom、搜尋與導覽、console／溢出；每項只記實際結果。最後整理 docs/redesign-report.md 並分階段 push、使用既有工具部署，確認 GitHub Pages 狀態。
- 分工／commit：Luna high 按檔案所有權實作，階段自行 commit、不互相 stage；主代理做 source review、技術差異核對、網站驗收與修正，分階段 push。共用 API 先定義再消費，禁止重寫別人的未提交修改。
- 進度：Phase 1 研究與規範已整理；Phase 2–5 待實作／驗收。

## 網站總覽與開始使用（2026-11-11）

- 目標：從首頁「親手啟動」進入第一章，選擇 Windows portable 或 WSL 路徑，辨識 WUWAOS shell 提示字元並找到開機流程。
- 實作：總覽保留架構圖與讀取路徑互動。提高圖說、標籤、來源資訊與導覽的字級，使用較深的灰字，窄螢幕將架構節點改為單欄。入門依啟動方式、操作 shell、進階磁碟與原始碼重排。
- 驗收：Chrome 以 1440、720、390、320 CSS px 檢查兩頁，確認必要圖文至少 14px、入門正文 16px、鍵盤焦點可見，總覽、啟動路線錨點及開機章連結可用，頁面無水平溢出。抽查文字配色對比皆達 4.5:1。文件站發布後以公開頁再次驗證。

## 進度維護

每完成一個步驟後，更新本檔與 [`plan_current.md`](./plan_current.md) 的進度。依使用者要求，本檔須記錄實際採用的解決觀念／演算法、必要虛擬碼、驗收結果與限制。未執行或未完成對應 runtime 驗證的項目不得記為完成；若當前條件無法進行 runtime 驗證，需明確保留未完成狀態並記錄限制。

依使用者最新指示，優先實作、更新進度並分階段 commit/push；額外測試暫緩。只記錄實際做過的 build/smoke，未執行項目標示 NOT_RUN，不宣稱全面驗收。
