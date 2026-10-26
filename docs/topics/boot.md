# 02 · 開機流程

CPU 不認識 `entry.S` 或 `main.c` 這些檔名，只會執行記憶體中的機器指令。兩個來源檔最後會被連結到同一份核心映像。

## 從韌體到 C

```text
QEMU virt → OpenSBI → 0x80200000 的 _start
  → 設定 stack、清除 BSS → call kernel_main
  → 裝置／頁表／檔案系統／行程初始化
  → sret → 使用者入口 → shell
```

OpenSBI（RISC-V 韌體）完成早期準備，將控制權交給 S-mode（監督模式）核心。`kernel/linker.ld` 指定 `_start` 為入口，從 `0x80200000` 放置核心。

## entry.S：先準備 C 所需的環境

原始碼：[kernel/entry.S（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/entry.S)。

`la sp, __stack_top` 設定 sp（堆疊指標）。C 的函式呼叫與區域資料會用到堆疊，因此必須先準備它。

`1:` 到 `2:` 的迴圈逐 byte 清除 BSS（未明確指定初值的靜態資料區）。例如 PCB 陣列與頁面管理欄位會從零開始。清除 B 個 bytes 的成本是 O(B)，不另外配置記憶體。

`call kernel_main` 才是兩個檔案的接點：組合語言以函式呼叫進入 C。若 C 意外返回，後面的 WFI（等待中斷指令）迴圈阻止 CPU 繼續執行未知內容。

## main.c：把子系統串起來

原始碼：[kernel/main.c（GitHub）](https://github.com/johnson931128/WUWAOS/blob/main/kernel/main.c)。連結為已提交版本，最新未提交裝置初始化調整不會隨文件發布。

先安裝 trap（陷阱／例外入口），再顯示動畫、探索區塊裝置、建立核心分頁映射與掛載檔案系統。最後建立第一個行程並交給行程恢復流程。

`fdt` 指向 FDT（硬體裝置樹），是韌體交給核心的硬體描述資料。核心用它尋找 VirtIO 裝置，實體頁配置器也會保留它占用的記憶體。

## 開機動畫與觀察

`kernel/splash.c` 透過 UART 輸出 ANSI 終端控制碼，以圖案中點為中心逐欄揭露 WUWAOS。這時尚未進入使用者模式；動畫中的忙等不是排程器，也不代表已啟用計時器中斷。

執行 `make run`，應依序看到字樣、核心與裝置資訊、最後的 `wuwaos>`。`/boot/kernel.elf` 是 WUFS 中的副本；目前真正啟動核心仍由 QEMU 的 `-kernel` 載入。

## 延伸資料

觀念參考：`notes/ch2 Operating-System Structures.md` 的開機與核心結構。實作細節與早期動畫驗收見[程式碼導覽](../CODE_GUIDE.md)及 [Spec 04](../SPEC-04-boot-splash.md)。
