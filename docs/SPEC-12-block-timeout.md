# Spec 12：VirtIO 磁碟請求逾時與停止核心

後續使用者於 2026-10-27 授權 commit／push，將本階段與前階段已完成的分頁／Swap、文件網站工作一併提交到 main。以下「不 commit／push」保留實作當時的範圍；本次 Git 提交不等於重新部署公開網站。

## 目標與範圍

使用者要求磁碟長時間未回應時輸出錯誤並停止。單次已提交的 512-byte VirtIO 請求預設等待 5 秒；用硬體時間計數器計時，逾時印出診斷並停止整個 WUWAOS，QEMU 保持開啟。不新增測試程式碼、不 commit／push；保留既有未提交工作。

## 設計與演算法

- `kernel/block.h` 的 BLOCK_IO_TIMEOUT_MS 為可調整常數，預設 5000 ms。`kernel/block.c` 使用 rdtime（讀取硬體時間計數器指令），依目前 QEMU virt 10 MHz 時基換算；同 splash.c 的平台假設，不新增 FDT 時基解析或計時器中斷。
- 準備 header／資料／status 描述項與 available ring 後，在公布 available.index 前記錄 start；保留原有 memory fence，再更新 available.index 與通知裝置。
- 輪詢 used.index；若已不同於 consumed，走原完成流程。未完成時用 unsigned 64-bit 的 now-start 計算經過 ticks，達 5 秒後再讀一次 used.index，避免完成恰好出現在檢查間隙卻誤報。
- 若仍未完成，輸出裝置 MMIO 位址、角色（probe／WUFS／swap）、read／write、磁區、經過毫秒、門檻與佇列完成計數。關閉監督模式中斷並進入不返回的 kernel_idle；不消耗完成項、不釋放頁框、不返回上層、不重用請求與緩衝區。
- 逾時不代表請求已取消；裝置可能晚到 DMA。沒有安全取消／reset 機制前，不能當成普通 -5 返回後繼續配置原緩衝區。此政策是教學系統的 fail-stop（出錯後停止）策略，並非成熟 OS 的完整恢復流程。
- 覆蓋開機讀取格式標記、WUFS 及 Swap 的共用 transfer 路徑；有效的裝置錯誤完成仍照原流程返回錯誤，不等滿 5 秒。沒有提交請求的參數錯誤仍立即返回。
- 5 秒是每個磁區請求的門檻，不是整個檔案或八磁區頁面交換的總門檻；裝置初始化的設定代數穩定讀取、UART 輸出、其他等待不在本次範圍。

## 執行步驟與驗收條件

1. 新增門檻、硬體計時與不返回的診斷停止路徑。
2. 正常建置，在獨立映像 QEMU 觀察檔案讀寫與分頁／交換正常。
3. 使用 QEMU 裝置 I/O 節流或偵錯能力造成請求延遲，不加入客體故障／測試程式。確認約 5 秒後印出診斷並停止進展，沒有繼續回收或執行 shell。
4. 更新程式碼導覽與主題文件；歷史 Spec 10 的未驗證紀錄保留，明確連結本次更新。

## 觀念來源

`notes/ch12  I_O Systems.md` 的 I/O 錯誤處理與裝置分層；具體 5 秒與停止政策是 WUWAOS 的設計。Linux NVMe 的普通 I/O 30 秒、管理命令 60 秒僅作比較，不代表 VirtIO 的通用要求。

## 驗收紀錄

2026-10-27，WSL Ubuntu、Clang 18、QEMU 8.2.2、OpenSBI 1.3，virt／單 hart／128 MiB。

- `make` 通過 -Wall/-Wextra/-Werror，kernel.elf 的 llvm-nm -u 無未解析符號；git diff --check 與文件網站建置通過。沒有新增測試程式或自動斷言，沒有 commit／push／網站部署。
- 使用新建 build/timeout-check.img 與 timeout-swap.img，未使用原有 wuwaos.img。正常開機辨識兩顆磁碟；write／cat 讀回 alive。8 頁常駐預算下填寫 10 個 heap 頁，第 0 頁首／中／尾讀回 65，vmstat 顯示 swap_in=5、swap_out=11、clean_drop=6、invalid_faults=0、io_errors=0；mem free 正常。
- QEMU monitor 的 `block_set_io_throttle wuwaos 0 1 0 0 0 0` 將 WUFS 讀取限制為 1 byte/s，再執行正常 cat：輸出 device=0x10008000、role=WUFS、op=read、sector=30、elapsed_ms=5000、limit_ms=5000、consumed=1062、used=1062，接著印出保留請求並停止核心。
- 第二輪只對交換磁碟設 bps_wr=1，正常開機後用 mem alloc／write 製造置換：輸出 device=0x10007000、role=swap、op=write、sector=9、elapsed_ms=5000、limit_ms=5000、consumed=3、used=3。觸發點在缺頁處理中的 dirty victim 寫出，沒有返回上層補頁或錯誤回收。
- 兩次逾時後 monitor 皆觀察 PC=0x8020036a；nm／反組譯確認 kernel_idle 位於 0x80200366，內容為 wfi 與跳回迴圈。監督模式中斷全域開關為關閉；逾時後沒有新的 shell 提示。使用 monitor quit 結束兩個 QEMU，沒有留存模擬器程序。
- 日誌在忽略的 build/timeout-normal.log、build/timeout-swap-write.log。逾時輸出中的時間以客體硬體計數器衡量，不是主機工具的等待秒數。

未驗證：完成恰在門檻交界的競爭、64-bit 計數器繞回、開機探測階段的實際逾時、UART 自身無回應、裝置逾時後晚到 DMA、真實硬體與非 10 MHz 平台。沒有宣稱取消請求、保證磁碟交易原子性或恢復裝置；停止核心只確保軟體不繼續重用尚未完成請求的記憶體。
