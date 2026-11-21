# 00 · 專案簡介與系統架構

WUWAOS 是一個跑在 QEMU `virt` 上的 [RISC-V 64 位元](./glossary.md#riscv)教學作業系統。你可以從終端機啟動程式、用管線傳送資料、讀寫磁碟，再沿著程式碼追蹤這些操作如何通過 CPU、核心和裝置。

## 從主機到使用者程式

| 層次 | 負責什麼 |
| --- | --- |
| 主機工具 | Clang 和 Make 在主機組譯、編譯與連結 RISC-V 核心及使用者程式。Python 工具準備磁碟映像。 |
| [QEMU](./glossary.md#qemu) 與 [OpenSBI](./glossary.md#opensbi) | QEMU 提供模擬的 CPU、RAM 和裝置。OpenSBI 是開機韌體，也提供給核心設定計時器、啟動其他 Hart 等服務。 |
| WUWAOS 核心 | 管理行程、執行緒、頁表、檔案和裝置。核心與使用者程式藉由[系統呼叫](./glossary.md#syscall)交接。 |
| 使用者程式 | Shell 執行命令與管線。`fan` 編輯檔案，`workstate` 顯示行程與資源用量。相容的靜態 [ELF](./glossary.md#elf) 也能從 [WUFS](./glossary.md#wufs) 載入。 |

例如，Shell 執行 `cat /note` 時，程式向核心請求開檔和讀取。核心透過 WUFS 找出檔案資料，再請磁碟驅動取回。程式最後把 bytes 寫到終端。這條路徑用到後文的[系統呼叫](./02-traps-and-syscalls.md)、[排程](./04-scheduling.md)和 [磁碟 I/O](./07-io.md)。

## 平台與範圍

現行目標是 QEMU `virt`、128 MiB RAM 和 OpenSBI。文字操作介面（CLI）使用 [UART](./glossary.md#uart)，圖形目標另外接 [VirtIO](./glossary.md#virtio) GPU 和輸入裝置，讓圖形 Terminal 可執行 Shell。核心能執行符合 WUWAOS 系統呼叫約定的靜態 RV64 ELF，Linux 的執行檔和動態連結器不在支援範圍。

核心入口負責初始化裝置、記憶體和行程，再交給排程器。開機交接、模組分工與舊版驗收各有紀錄。 [核心入口](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/boot/main.c) · [開機流程](../topics/boot.md) · [模組分層規格](../SPEC-33-module-architecture.md)

接著讀[如何執行](./01-running.md)。
