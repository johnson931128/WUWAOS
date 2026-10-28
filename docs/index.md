# WUWAOS 實作紀錄

從第一條指令，到能建立行程、編輯檔案的小型作業系統。

WUWAOS 是以 C 與 RISC-V 組合語言撰寫、真正運行於 QEMU 虛擬電腦上的作業系統。這份文件把設計按照主題整理，連接教科書觀念、目前程式碼與可以親手觀察的操作。

**[從環境與啟動開始 →](./topics/getting-started.md)**

## 閱讀路線

| 想理解什麼 | 從這裡開始 |
| --- | --- |
| entry.S 怎麼進入 C，最後顯示提示字元？ | [開機流程](./topics/boot.md) |
| 使用者程式怎麼請核心幫忙？ | [例外與系統呼叫](./topics/traps.md) |
| 行程如何建立、排程、結束與回收？ | [行程管理](./topics/processes.md) → [計時器與 RR 排程](./topics/scheduling.md) |
| CPU 如何找到資料，缺頁時發生什麼？ | [Sv39](./topics/memory.md) → [按需分頁與交換](./topics/paging.md) |
| 輸入的文字如何存到磁碟？ | [輸入輸出](./topics/io.md) → [WUFS](./topics/filesystem.md) |
| 如何寫自己的互動程式？ | [Shell 與 fan](./topics/applications.md) |
| 如何觀察行程的 CPU 與記憶體用量？ | [workstate 即時監測](./topics/monitoring.md) |

## 目前的系統邊界

| 項目 | 現況 |
| --- | --- |
| 平台 | QEMU virt、RV64、單核心、128 MiB RAM |
| 執行模式 | S-mode 核心、U-mode 使用者程式 |
| 行程 | 最多 4 個 PCB；fork／exec／exit／wait 與事件等待 |
| 核心上下文 | RV64 swtch.S；每行程保留 C 呼叫堆疊、獨立排程器堆疊 |
| 排程 | 單核心 RR、50 ms 時間片；使用者程式可搶占，核心內不搶占 |
| 記憶體 | Sv39 三級頁表、4 KiB 頁、按需分頁、FIFO 置換、獨立 Swap |
| 磁碟 | VirtIO 區塊 I/O、自訂 WUFS、重新啟動後保留檔案 |
| 操作 | Shell、固定 writer \| reader 管道、fan 全螢幕文字編輯器 |
| 監測 | workstate 全螢幕；每秒更新行程、CPU 派發時間、RSS／Swap 與整體 RAM |

這是現有功能的實作紀錄，不是承諾與 Linux 相容。沒有一般磁碟 ELF 載入器、通用檔案描述號、網路或圖形桌面。

## 這份文件如何組織

主題章節描述目前設計；[原始規格與驗收紀錄](./history.md) 保留開發過程。舊規格中的架構可能已被後續版本取代，閱讀時應留意每份文件的階段說明。

本站只展示架構說明與操作指令，OS 原始碼以 GitHub 連結提供，不內嵌程式碼。OS 與文件變更經使用者授權提交到 main；網站發布另由專用工具處理。驗收結果是各規格所記錄的當時操作，網站建置不代表重新執行過 OS 驗證。

## 參考與致意

閱讀版面與循序章節安排參考 [《1000 行打造作業系統》](https://operating-system-in-1000-lines.vercel.app/tw/)。本文內容依 WUWAOS 自己的程式碼整理，沒有複製該書文章。作業系統概念參考 repository 的 `notes/` 課程筆記，各章另列對應檔案；筆記全文不包含在網站中。
