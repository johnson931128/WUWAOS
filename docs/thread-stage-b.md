# Thread 階段 B/C：多 Thread 核心與共享生命週期

2026-11-10：在階段 A 的 TCB 排程基礎上，接入同一 Process 的多條 user Thread、額外 user stacks、create/join/exit/gettid，以及共享資源安全清理。

## 實作範圍

- 每 Process 最多 8 個占用 TCB slots、全系統仍最多 32 個 TCB。NEW、Ready、Running、Waiting 與未 join 的 Zombie 都占槽；TID 單調遞增。
- `SYS_THREAD_CREATE=42`、`JOIN=43`、`EXIT=44`、`GETTID=45` 已接入核心及 `user/lib/api.h`。Thread return 會經 `user/lib/thread.S` trampoline 轉成 Thread exit。
- 每個額外 Thread 使用 32 KiB RW stack、1 頁未映射 guard、16 KiB guarded kernel stack。VM page metadata、demand fault、FIFO/swap、clone、destroy、usage 都納入額外 stack；stack slot 在發布 Ready 前保留並預載最上方一頁。
- Join 使用 TID claim、cycle scan、狀態 copyout 與額外 stack 回收；只有 scheduler 確認所有 Process Thread 已離開 kernel stack 後才做群組資源清理。Process exit、kill 與未處理 user fault 都會取消同 Process 其他 Thread。
- Fork/exec 檢查所有占用 TCB slots；只允許唯一 Thread。Fork 複製目前額外 user stack 的 reservation/內容，exec 回到主 stack。`image_changing` 阻止同時建立 Thread。
- Read/write 在 syscall 開始固定 FD 對應的 OFD 並持有短期引用，阻塞時不會因 close/reuse 而改操作新 FD。FD open/pipe 使用保留標記，pipe descriptor 僅在 copyout 後發布。FD table 修改使用單 hart syscall 下的短 SIE=0 原子區段，不跨等待持有 table lock。
- 相對路徑在 syscall 入口複製 cwd；同一個 FS copy 的 source/destination 共用同一快照。Child wait 使用單一 TID claim；取消與 copyout 失敗保留可重試結果。TTY raw owner 使用呼叫者 TID。
- workstate 將已回收 Thread 的 ticks 留在 Process aggregate，並將同一 VmSpace RSS 計算一次。群組清理完成後 Zombie Process 不再保有 TCB；`ps` 顯示 TID 0，`pcb` 說明沒有占用 Thread slots。

## 簡化與尚未完成

單 hart 下以短、不可切換的 descriptor table 操作、FD reservation 與 OFD pins 保護共享 FD table，沒有再引入第二把 table gate。未增加 `demo threads`、Thread 專用清單命令或新的可觀測 TID 列表；目前診斷保留 Process row 與代表 TID。這些選擇不改動 Spec 的 syscall/stack ABI 與生命週期契約。

## 驗證狀態

僅執行核心/使用者映像編譯與連結：

```powershell
$env:MSYSTEM='UCRT64'
& .\build\portable\msys64\usr\bin\bash.exe --noprofile --norc -c 'export PATH=/ucrt64/bin:/usr/bin:/bin; make -j4 all build/kernel-disk.elf'
```

結果通過，輸出記錄為 `build/thread-stage-b-build.log`。依要求未新增或執行測試、未跑 QEMU/GUI，未碰日常磁碟。所有多 Thread 排程、join race、swap、I/O close/reopen、kill/fault/DMA 與 fork/exec runtime 行為仍標為 **NOT_RUN**；編譯通過不代表驗收通過。
