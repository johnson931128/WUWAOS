# 00 · 專案簡介與系統架構

WUWAOS 是一個跑在 QEMU `virt` 上的 [RISC-V 64 位元](./glossary.md#riscv)教學作業系統。從終端機發出的指令可以用來追蹤一次操作如何經過 CPU、核心和裝置。

## 作業系統在管什麼

從使用者的角度，`cat /note` 的結果就是終端印出檔案內容。從系統的角度，這條指令牽涉三種資源。排程器分配 CPU 時間，頁表讓行程使用自己的位址空間，檔案系統與驅動程式則把檔名轉成磁碟請求。核心也負責隔離這些資源，所以讀檔要經過受控的系統呼叫。

把核心、韌體和工具的責任分清楚，便能定位問題。QEMU 提供虛擬機器，OpenSBI 在核心開始前接手開機，並繼續提供計時器等韌體服務。WUWAOS 核心決定執行緒排程和檔案存取。Shell、`fan` 和 `workstate` 則在使用者態請求這些服務。

## 從主機到使用者程式

| 層次 | 負責什麼 |
| --- | --- |
| 主機工具 | Clang 和 Make 在主機組譯、編譯與連結 RISC-V 核心及使用者程式。Python 工具準備磁碟映像。 |
| [QEMU](./glossary.md#qemu) 與 [OpenSBI](./glossary.md#opensbi) | QEMU 提供模擬的 CPU、RAM 和裝置。OpenSBI 是開機韌體，也提供給核心設定計時器、啟動其他 Hart 等服務。 |
| WUWAOS 核心 | 管理行程、執行緒、頁表、檔案和裝置。核心與使用者程式藉由[系統呼叫](./glossary.md#syscall)交接。 |
| 使用者程式 | Shell 執行命令與管線。`fan` 編輯檔案，`workstate` 顯示行程與資源用量。相容的靜態 [ELF](./glossary.md#elf) 也能從 [WUFS](./glossary.md#wufs) 載入。 |

例如，Shell 執行 `cat /note | cat` 時，第一段透過 WUFS 取回檔案資料，再寫入 Pipe。第二段從標準輸入讀取 Pipe，最後把 bytes 寫到終端。這條路徑用到後文的[系統呼叫](./02-traps-and-syscalls.md)、[排程](./04-scheduling.md)和 [磁碟 I/O](./07-io.md)。

### 跟著一次讀檔走

1. Shell 建立管線並啟動兩個 `cat` 執行階段。使用者程式開啟 `/note` 後，用核心回傳的檔案描述符讀取資料。
2. 核心依檔案描述符找到開啟物件，再由 WUFS 找到資料所在磁區。讀取需要等待時，執行緒可讓出 Hart。
3. 資料返回使用者緩衝區後，第一個 `cat` 寫入自己的標準輸出。Shell 事先把它接到 Pipe 寫端，把第二個 `cat` 的標準輸入接到讀端。第二段的標準輸出仍指向終端。 [管線與 cat](https://github.com/johnson931128/WUWAOS/blob/main/user/shell/shell_exec.c)

同一個例子串起本書後續三個問題：核心如何接手（第 02 章）、誰先執行（第 04 章）、資料怎麼移動和保存（第 07、09 章）。

## 平台與範圍

現行目標是 QEMU `virt`、128 MiB RAM 和 OpenSBI。文字操作介面（CLI）使用 [UART](./glossary.md#uart)，圖形目標另外接 [VirtIO](./glossary.md#virtio) GPU 和輸入裝置，讓圖形 Terminal 可執行 Shell。核心能執行符合 WUWAOS 系統呼叫約定的靜態 RV64 ELF，Linux 的執行檔和動態連結器不在支援範圍。

核心入口負責初始化裝置、記憶體和行程，再交給排程器。開機交接、模組分工與舊版驗收各有紀錄。 [核心入口](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/boot/main.c) · [開機流程](../topics/boot.md) · [模組分層規格](../SPEC-33-module-architecture.md)

可以在[首頁系統架構圖](../index.md#atlas-title)點選 CPU、核心或裝置，查看每個元件的責任和對應原始碼。

## 恐龍書與課堂筆記

恐龍書第 10 版 §1.1–1.5、§2.1、§2.8。

- [作業系統的角色、硬體與資源管理](../theory/01-introduction.md)
- [作業系統服務、介面與核心結構](../theory/02-os-structures.md)
