# 00 · 專案簡介與系統架構

<div class="tb-box problem">
<span class="tb-label">本章要解決的問題</span>

現代電腦裡只有一套 CPU、實體記憶體與磁碟，卻要同時執行多個應用程式。如果讓每個程式直接讀寫硬體暫存器與實體位址，一個有 Bug 的程式就能把別人的資料覆寫掉，甚至讓整台機器當機。**作業系統（Operating System）**存在的根本目的，就是在硬體與使用者之間建立一層**受控的抽象與隔離邊界**。
</div>

## 恐龍書核心理論：作業系統的兩種角色

在《Operating System Concepts》（恐龍書）第 1–2 章中，作業系統同時扮演兩種角色：

1. **服務提供者（Service Provider）**：把複雜的硬體細節包裝成統一介面——包含命令直譯器（Shell）、程式載入執行、I/O 操作、檔案系統與行程通訊——讓應用程式不必自己寫驅動程式就能讀檔或印出文字。
2. **資源管理者（Resource Allocator）與控制程式（Control Program）**：公平分配 CPU 時間、實體頁框與磁碟區塊，並透過**雙重模式（Dual-Mode Operation）**區分「使用者模式」與「核心模式」，防止非法操作破壞系統（Protection）。

```
  使用者程式 (User Mode / U-mode)     ← Shell、編輯器、監測工具（不能直接碰硬體）
  ──────────── ecall / sret ──────────── 系統呼叫邊界（受控入口）
  作業系統核心 (Kernel Mode / S-mode)  ← 行程排程、虛擬記憶體、檔案系統、驅動程式
  ──────────── MMIO / IRQ ───────────── 硬體控制與中斷邊界
  實體／虛擬硬體 (Hardware + M-mode)   ← CPU、RAM、計時器、UART、VirtIO 磁碟/GPU
```

## 從主機到使用者程式：WUWAOS 的四層分工

WUWAOS 是一個跑在 QEMU `virt` 上的 [RISC-V 64 位元](./glossary.md#riscv)教學作業系統。把恐龍書的分層模型對照到 WUWAOS 的真實環境，整套系統由以下四層構成：

| 層次 | 對應理論角色 | WUWAOS 實際負責內容 |
| --- | --- | --- |
| 主機工具 | 交叉編譯環境 | Clang 與 Make 在主機編譯、連結 RISC-V 核心與使用者程式；Python 工具製作 WUFS 磁碟映像。 |
| [QEMU](./glossary.md#qemu) 與 [OpenSBI](./glossary.md#opensbi) | 硬體與韌體（M-mode） | QEMU 提供模擬的 RV64 CPU、128 MiB RAM 與裝置；OpenSBI 作為開機韌體，並提供計時器與多 Hart 啟動等 SBI 服務。 |
| WUWAOS 核心 | 核心態（S-mode） | 管理行程與執行緒、Sv39 頁表、檔案系統（WUFS）與裝置驅動，透過[系統呼叫](./glossary.md#syscall)服務使用者程式。 |
| 使用者程式 | 使用者態（U-mode） | Shell 執行命令與管線，`fan` 編輯檔案，`workstate` 觀察系統狀態；也能從 [WUFS](./glossary.md#wufs) 載入相容的靜態 [ELF](./glossary.md#elf)。 |

### 課本實例對照：執行 `cat /note` 時發生了什麼？

當你在 Shell 輸入 `cat /note` 時，這一行指令會貫穿整個作業系統的四大子系統：

1. **Shell（U-mode）** 透過 `fork` / `exec` 啟動 `cat`，向核心發出 `open` 與 `read` [系統呼叫](./02-traps-and-syscalls.md)。
2. **核心（S-mode）** 透過 WUFS 解析 `/note` 的 inode 與磁區位置，並交給 [VirtIO 磁碟驅動](./07-io.md) 發送讀取請求。
3. 等待磁碟期間，[排程器](./04-scheduling.md)可先切換到其他就緒執行緒，不讓 CPU 空轉。
4. 磁碟完成後喚醒原執行緒，核心將資料複製回使用者緩衝區，`cat` 再透過 `write` 把內容輸出到終端機。

<div class="tb-box wuwaos">
<span class="tb-label">WUWAOS 實作範圍與設計邊界</span>

- **目標平台**：QEMU `virt`、128 MiB RAM、OpenSBI。CLI 使用 [UART](./glossary.md#uart)，圖形組態另接 [VirtIO](./glossary.md#virtio) GPU 與輸入裝置（核心端繪製桌面與 Terminal，視窗內的 Shell 仍在 U-mode）。
- **執行檔相容性**：支援符合 WUWAOS 自訂 syscall ABI 的靜態 RV64 ELF；不實作 Linux ABI 與動態連結器，讓整份核心程式碼保持精簡可讀。
</div>

## 延伸閱讀與互動架構圖

你可以在[首頁系統架構圖](../index.md#atlas-title)滑過或點選任何元件，直接追蹤 `echo hello` 與檔案讀取如何跨層運作。

- 原始碼與沿革：[核心入口 (`kernel/boot/main.c`)](https://github.com/johnson931128/WUWAOS/blob/3a3b273/kernel/boot/main.c) · [開機流程](../topics/boot.md) · [模組分層規格](../SPEC-33-module-architecture.md)
