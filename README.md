# WUWAOS

![Language: C](https://img.shields.io/badge/language-C-00599C?logo=c&logoColor=white)
![Architecture: RISC-V 64](https://img.shields.io/badge/architecture-RISC--V%2064-283272?logo=riscv&logoColor=white)
![Platform: QEMU](https://img.shields.io/badge/platform-QEMU-FF6600?logo=qemu&logoColor=white)
![Project: Educational OS](https://img.shields.io/badge/project-educational%20OS-2E8B57)

WUWAOS 是使用 **C 和 RISC-V 組合語言**開發的小型作業系統，能在 QEMU 模擬的 RISC-V 64-bit 電腦上啟動。這個專案透過實作真正的核心與使用者程式，學習行程、排程、記憶體和檔案系統如何協作。

[閱讀實作文件網站](https://johnson931128.github.io/WUWAOS/) · [程式碼與架構導覽](docs/CODE_GUIDE.md) · [規格與驗收紀錄](docs/history.md)

## 系統概念

| 主題 | WUWAOS 的實作 |
| --- | --- |
| 執行環境 | QEMU `virt`、RISC-V 64-bit、單核心、128 MiB RAM；OpenSBI 啟動核心 |
| 行程與排程 | PCB（行程控制區塊）、fork／內建程式 exec／wait／exit，以及計時器驅動的 RR（輪詢排程） |
| 同步與通訊 | 自製 Mutex（互斥鎖）、Semaphore（號誌）、等待／喚醒及固定的 `writer \| reader` 管道 |
| 虛擬記憶體 | Sv39 三級頁表、按需分頁、FIFO 頁面置換及 swap（交換區） |
| 檔案系統 | VirtIO 區塊裝置、自訂 WUFS，支援目錄和檔案持久保存 |
| 使用者介面 | shell、`fan` 文字編輯器、`workstate` 監測工具，以及 `demo sync` 同步示範 |

WUWAOS 目前著重作業系統基本機制，不是 Linux 相容系統。核心由 QEMU 使用 `-kernel` 載入；尚未支援從磁碟載入一般 ELF 執行檔、系統內 C 編譯器、網路或圖形桌面。詳細限制與後續規劃請參考[文件網站](https://johnson931128.github.io/WUWAOS/)及 [Spec 21](docs/SPEC-21-native-compilation.md)。

## 啟動畫面

![WUWAOS 在 QEMU 的開機畫面](docs/images/wuwaos-boot.png)

*實際 QEMU／PowerShell 畫面，截圖來自先前版本；目前 shell 提示字元會顯示所在目錄，例如 `wuwaos/>`。*

## 啟動方式

以下以 Windows PowerShell、WSL Ubuntu，以及專案位於 `C:\programing\WUWAOS` 為例。WSL 需要 Clang、LLD、llvm-objcopy、Make、Python 3、QEMU RISC-V 與 OpenSBI。

在 PowerShell 執行：

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/c/programing/WUWAOS && make run'
```

`make run` 先由 WSL 的 Clang 交叉編譯 RISC-V 程式，再啟動 QEMU。開機進入 `wuwaos/>` 後輸入 `help` 可以查看系統指令；例如 `demo sync` 會展示父子行程使用同步工具的過程。

退出 QEMU：按 **Ctrl+A**，放開後按 **X**。`exit` 只結束 shell，不會關閉 QEMU。WUFS 虛擬磁碟位於 `build/wuwaos.img`，重新啟動後保留檔案；`make clean` 不會刪除磁碟映像。

## 文件

- [開始使用與主題章節](docs/index.md)：開機、行程、排程、記憶體、I/O、檔案系統與使用者程式。
- [同步示範](docs/SPEC-23-sync-demo.md)：觀察 Mutex 與 Semaphore 的等待和喚醒。
- [程式碼與架構導覽](docs/CODE_GUIDE.md)：各程式檔案的責任及設計。
- [規格與驗收索引](docs/history.md)：各階段的設計、驗證與限制；舊規格保留當時背景。
- [課程筆記](notes/)：作業系統觀念與名詞的參考來源。
