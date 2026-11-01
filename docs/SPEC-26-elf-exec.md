# Spec 26：從 WUFS 路徑載入 ELF

## 目標與範圍

讓 shell `run <path>` 以 fork 建立子行程，透過 WUWAOS 自訂系統呼叫從 WUFS 讀取路徑指向的 ELF 並執行；絕對及相對路徑使用現有 CWD 規則。既有 `run worker|quick|counter` 與內建 exec 保留。首版無命令列參數、PATH 搜尋、動態連結與 Linux syscall 相容性。

## 格式與載入設計

支援 ELF64、小端、ET_EXEC、RISC-V、soft-float ABI；依程式標頭讀取 PT_LOAD，支援 WUWAOS 固定佈局 code 0x400000–0x403fff 的 RX 段與 data 0x404000–0x404fff 的 RW 段，不允許 W+X、頁面重疊或超出區域；執行入口必須落在可執行段內且適當對齊。檢查標頭大小、表格／檔案邊界、整數溢位、segment 大小與對齊；拒絕動態連結、直譯器與不支援權限及格式。BSS（未初始化資料）補零。其他可忽略的非載入標頭不映射。

先建立獨立 VM，將段內容載入私有頁；載入的頁標記為需要 swap backing，不依賴原檔案繼續存在或不被更改。參與 FIFO 換頁、fork 複製與 exit/wait 回收。全部驗證及載入成功才替換舊位址空間，失敗則銷毀新空間、保留原程式。相容現有 RISC-V WUWAOS syscall ABI，不保證 Linux 執行檔相容。

## 執行與驗收

1. 建置並以使用者已存在的 shell ELF 做 QEMU 實際載入與正常退出；不用新增測試程式碼。
2. 驗證不存在、非 ELF、格式不支援等失敗後 shell 繼續；確認路徑、清理與原有內建功能。
3. 在 swap 和 fork 等情況檢查程式內容、資源回收；補上實測或未驗證的邊界。
4. 更新程式碼導覽、主題網站、README 與 spec；驗收後 commit/push。

參考使用者筆記 `notes/ch3 Process.md`、`notes/ch9 Main Memory.md`，ELF 格式見 [ELF program headers](https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.pheader.html) 與 [RISC-V ELF psABI](https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc)。設計描述的是 WUWAOS 首版子集。

## WUFS 檔案匯入

關閉 QEMU 後，使用主機端 `tools/make_disk.py --install <主機檔案> <WUFS絕對路徑>` 將已編譯檔案放進磁碟；父目錄必須存在，目的檔存在時拒絕覆寫，整個匯入在內存準備後才寫入映像。不要在 QEMU 使用該磁碟時從主機修改映像。

## 驗收紀錄與限制

使用 WSL／Clang 和獨立 QEMU 磁碟，將既有 `build/user.elf` 及 `build/app.elf` 匯入 `/bin`：`run /bin/shell.elf` 成功執行外部 shell；在執行中刪除該 ELF、把常駐預算調到 8 並操作 12 頁 heap，之後 swap_in=22、swap_out=18，讀回第 0 頁三處皆為 65；該外部 shell 進一步 fork，子行程與父行程資料分離且均能結束回收。不存在檔案回 -2、目錄回 -21、一般文字或不符合使用者位址配置的核心 ELF 回 -8；shell 能繼續。相對路徑 `run app.elf` 成功載入既有 app 映像，但 app 的內建角色參數為 0 時自行回 -22，並非載入器失敗。`run quick`、pipe 和 demo sync 保持可用；host 匯入既有路徑拒絕，映像 SHA-256 前後相同。

無 swap、使用者常駐預算 8 頁時，載入 shell ELF 回 -12，子行程被回收且父 shell 可繼續；恢復 64 頁後成功執行。另一次無 swap、預算 8 頁下填寫多個 heap 頁，父 shell 在取指缺頁時因無可用頁以 -12 結束，屬目前 VM 的資源不足行為。仍未逐項注入毀損 ELF 標頭、裝置失敗、物理 RAM 完全耗盡，或測試所有 16 個行程額滿時載入；不能保證所有不支援指令可在載入時識別。首版程式入口 a0=0、a1=0，無 argc/argv；僅使用 WUWAOS 自訂 syscall ABI，Linux ELF 不能因此視為可用。客戶端沒有自動編譯器或通用動態連結器。
