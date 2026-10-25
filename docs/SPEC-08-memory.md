# Spec 08：動態實體頁面與 Sv39 三級分頁

## 目標與範圍

將既有靜態 AddressSpace 陣列改為動態 4 KiB 實體頁面配置，保留 RV64 Sv39 的 L2 → L1 → L0 硬體格式：每表 4096 bytes、512 個 8-byte PTE。保留既有 inode 修復與磁碟資料，不新增測試程式碼，不自行 commit／push。

## 設計

- 平台範圍仍為 Makefile 的 QEMU virt、單 hart、128 MiB RAM（0x80000000–0x88000000）。不是通用硬體記憶體探索；不支援任意 RAM 大小、initrd 或任意韌體保留區配置。
- `page.c` 使用 free list（空閒頁面串列）管理 `__kernel_end` 向上對齊至 FDT 起始位置向下對齊之間的頁面。韌體、核心、固定核心堆疊、FDT 及其後整段 RAM 不加入空閒串列。檢查 FDT magic／大小與範圍；FDT 不符合上述標頭與範圍檢查時停止啟動。此檢查不代替通用 RAM／保留區探索，執行時仍須遵守既定平台配置。
- 每次配置先清零頁面；配置／回收以串列頭 O(1) 操作，另用配置 bitmap 偵測非法或重複回收。單核心不加鎖。
- `vm_walk` 使用 `(va >> (12 + 9 * level)) & 511` 取得各層索引，需要時配置下層表；`vm_map` 安裝對齊的 4 KiB leaf（末級映射），拒絕覆蓋既有映射。
- 常駐 kernel root（核心根分頁表）映射核心與剩餘 RAM，核心文字 RX、唯讀資料 R、其他 RAM RW；UART／VirtIO MMIO 為 RW，全部不設 U（使用者存取權限）。每行程共用核心分支，低位址 L1 複製裝置分支後再建立私有使用者映射。
- 使用者布局維持 16 KiB code RXU、4 KiB data RWU、4 KiB stack RWU；建立時配置各私有頁面，不再保留五份固定 AddressSpace。核心堆疊與 PCB 仍為固定四欄。
- fork 複製使用者頁內容，不複製父分頁表指標。exec 先建立完整新空間；成功後切換 satp（位址轉換設定暫存器）再釋放舊空間。配置失敗保留原行程並回傳 -12。
- Zombie 保留記憶體到 wait／孤兒回收；排程器先切核心根表，再完成回收與下一個行程選擇。核心堆疊保留，避免釋放仍在執行 C 程式的堆疊。
- 銷毀空間時遞迴釋放私有使用者頁及私有表，跳過與核心範本相同的共用分支。建立中失敗也走同一清理流程。ps 顯示實際空閒／總頁數。

## 執行步驟與驗收條件

1. 加入實體頁面配置器，整合開機與建置。
2. 將 vm 改為通用三級查表、映射、建立、複製、銷毀。
3. 串接建立失敗、fork、exec、wait、孤兒與 pipeline 失敗清理。
4. 編譯與未解析符號檢查；QEMU 實際操作 fork／exec／counter／wait／kill／固定 pipe 與檔案讀寫。
5. 以 ps 比對建立前、Zombie 階段與回收後的空閒頁數；回收後應回到基準值。以 QEMU monitor 檢查三級表及 U／R／W／X 權限；記錄尚未驗證路徑。

## 觀念來源與限制

- 課程筆記 `notes/ch9 Main Memory.md` §9.3 分頁、§9.4.1 階層式分頁：筆記中的雙層 32-bit 範例用於解釋分層概念；WUWAOS 實作為 Sv39 三層，不能直接套用範例表項數。
- 硬體規格：[RISC-V Supervisor ISA](https://docs.riscv.org/reference/isa/priv/supervisor.html)。4 KiB 頁面使用三級查表；本階段不實作大頁面。
- 不加入 heap／sbrk／mmap、缺頁自動配置、swap、COW、多核心或計時器搶占。動態分配物理頁不代表使用者程式已能任意擴充記憶體。

## 驗收紀錄

2026-10-26，WSL Ubuntu／Clang 18／QEMU 8.2.2／OpenSBI 1.3，virt、單 hart、128 MiB。未新增測試程式碼；操作既有 shell、內建 app 及 QEMU monitor。

- `make clean && make` 通過 `-Wall -Wextra -Werror`；`llvm-nm -u build/kernel.elf build/user.elf build/app.elf` 沒有未解析符號；`git diff --check` 通過。
- 本輪配置池 `[0x802a4000, 0x87e00000)`，共 31580 頁。常駐核心映射使用 67 頁；shell 再使用 10 頁，`ps` 基準 free=31503。每行程四張私有表、六頁使用者內容，消耗 10 頁。位址與頁數隨核心大小及平台配置可能改變。
- PID 2 fork 後為 Zombie，free=31493；子 data／stack 顯示 101／101，父仍 1／100。`wait 2` 收到 0，free 回到 31503。
- PID 3／4 分別 fork 後 exec counter，保持 PID 並等待 channel 9，free=31483。`wakeup 9` 喚醒兩個，依 3:3、4:3、3:2、4:2、3:1、4:1 輸出；wait 均收到 0，free 回到基準。exec 後只保留每行程 10 頁，舊映像已回收。
- PID 5 run worker、kill／wait 收到 -9；固定 pipe PID 6／7 傳完 80 bytes、EOF 並回收；PID 8 run quick、wait 收到 40。各流程結束後 free 都回到 31503。
- 直接 spawn PID 9／10／11 三個 worker 後填滿四欄，free=31473；再 spawn 回 -11。逐一 kill／wait 後 free=31503，沒有額外頁面遺留。
- QEMU monitor `xp` 證據：shell root 0x802e7000 的 L2[0]=0x200ba001 → L1 0x802e8000；其 L1[2]=0x200ba801 → L0 0x802ea000；L0[0]=0x200ba4db → code 實體頁 0x802e9000，flags=0xdb（V/R/X/U/A/D，沒有 W）。data L0[4]=0x200bb8d7，stack leaf=0x200bbcd7，flags=0xd7（V/R/W/U/A/D，沒有 X）。PID 3 的 root=0x802fb000，code leaf=0x200bf4db，指向不同的 0x802fd000。
- shell 的 L1[128] 指向共用 UART L0，UART leaf=0x040000c7（沒有 U／X）；`info mem` 在核心根表下確認核心文字 RX、唯讀資料 R、剩餘 RAM RW，以及 UART／VirtIO MMIO RW，均沒有 U。
- PID 12 quick 成為 Zombie 後不 wait，直接讓 shell exit；核心回收 shell 及無父 Zombie 後輸出 `All processes reaped; idle`。monitor 確認 satp=0x80000000000802a4（常駐核心根表），free_count=0x7b19（31513），所有行程私有頁面均歸還，沒有銷毀活動根表的錯誤。
- 獨立 `build/memory-check.img` 建立 `/home/user/memory.txt`、寫入並讀回 `paging-works`；完整重編譯並重啟 QEMU，讀回相同內容，再執行 fork／wait，free 仍回到基準。原有 `build/wuwaos.img` 未修改。操作記錄為忽略的 `build/memory-console.log` 與 `build/memory-reboot-console.log`，monitor 檢查見本對話工具輸出。

尚未動態驗證：實體頁面耗盡時的部分建表清理、exec 配置失敗保留舊映像、pipeline 建立中途配置失敗，以及故意的無效使用者指標／權限違規。這些路徑已作靜態檢查，但四欄 PCB 的 -11 驗收不能當成記憶體耗盡 -12 的驗收。未驗證任意 FDT 保留區、多 hart、不同 RAM 配置；它們不在本階段支援範圍。
