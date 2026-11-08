# 全站圖像教學研究與設計對照

研究日期：2026-11-08。此頁記錄改版前的教材研究、借鑑方式及與 WUWAOS 的差異；圖解由本專案重新繪製，不把其他 OS 的結構或執行結果當成本專案實作。

## 研究方法與來源

| 來源 | 已檢視內容與圖像結構 | 採用方式 |
| --- | --- | --- |
| [MIT xv6 RISC-V Book](https://mit-pdos.github.io/xv6-riscv-book/) | Organization、Page tables、Traps、Scheduling、File system、Logging。記憶體圖由位址欄位、表項、下一層表逐級連接；trap 與 context 圖分開硬體／核心路徑。 | 借鑑分層、位址標記和保存／還原的視覺關係；不複製 trampoline、核心佈局或多核心鎖設計。 |
| [OSTEP](https://research.cs.wisc.edu/wind/OSTEP/) | [Scheduling §7.7](https://research.cs.wisc.edu/wind/OSTEP/cpu-sched.pdf)、[Advanced Page Tables](https://research.cs.wisc.edu/wind/OSTEP/vm-smalltables.pdf)、[FSCK/Journaling](https://research.cs.wisc.edu/wind/OSTEP/file-journaling.pdf)。由簡化工作負載推演，再加入限制。 | 先列示例假設，再逐步說明；以時間軸對照 ready queue，持久化則明確區分 log 與 home。連到原教材，不在本站重製整章。 |
| [OS in 1,000 Lines](https://operating-system-in-1000-lines.vercel.app/tw/) | 從 boot 到 syscall／VirtIO／FS 的循序章節；[Page Table](https://operating-system-in-1000-lines.vercel.app/tw/11-page-table) 連接觀念、程式與觀察。 | 借鑑問題 → 最小流程 → 原始碼的順序。該教材的 RV32／Sv32 範例不能套用到本站 RV64／Sv39。 |
| [OSTEP VizTools](https://srg-ics-uplb.github.io/cmsc125/viztools/index.html) | [Scheduling](https://srg-ics-uplb.github.io/cmsc125/viztools/scheduling/) 與索引中的 trap、paging、swapping 示範，以決策步驟串接事件敘述。 | 每一步同時更新圖、狀態和敘述；不複製示範程式或把它的演算法清單當成 WUWAOS 功能。 |
| [OS Lab](https://oslab.xikai.me/) | [CPU Scheduling](https://oslab.xikai.me/cpu-scheduling) 的控制區、參數、就緒佇列、Gantt、事件與指標；已用瀏覽器檢視其行動版排列。 | 只保留幫助理解的輸入與視圖；WUWAOS 單核心 RR，不提供該站的多核心、MLFQ 或一般配置策略。 |
| [Operating System Concepts 10e](https://www.os-book.com/OS10/slide-dir/index.html) | 官方章節投影片入口與專案 `notes/` 對應課程內容。 | 狀態圖、分頁與 I/O 概念以筆記對照；官方投影片使用條款不等於一般公開網站轉載授權，因此不直接搬圖。 |
| [Computer Organization and Design RISC-V Edition](https://shop.elsevier.com/books/book-companion/9780128203316) | 官方 companion 與書目入口；CPU／記憶體位址與軟硬體界面以本專案筆記及 source 交叉對照。 | 借鑑將指令、暫存器、位址欄位分開說明的教學方向；沒有取得並重製該書內頁圖片，也不宣稱已讀取全部受限教材。 |

## 十二章的圖解研究對照

| 章節 | 原創視覺方案 | 參考案例／圖像結構 | WUWAOS 差異與 source |
| --- | --- | --- | --- |
| Getting Started | A：分層系統地圖；B：學習路線 | [xv6 organization](https://mit-pdos.github.io/xv6-riscv-book/first.html) 的核心職責分區；[1,000 Lines 目錄](https://operating-system-in-1000-lines.vercel.app/tw/) 的依賴順序。 | 單一靜態連結 S-mode kernel，UI 也在 S-mode；以 `kernel/boot/main.c` 與實際目錄分區。 |
| Boot | B：QEMU → OpenSBI → entry → kernel services → process | [1,000 Lines Boot](https://operating-system-in-1000-lines.vercel.app/tw/04-boot) 的分步啟動敘述。 | QEMU `-kernel` 載入 ELF；WUFS `/boot/kernel.elf` 副本不參與 boot。`arch/riscv/entry.S`、`kernel/boot/main.c`。 |
| Traps | B：U/S mode 泳道與 TrapFrame | [xv6 trap fig. 4.1](https://mit-pdos.github.io/xv6-riscv-book/trap.html) 的進入／返回路徑。 | 使用 `trap_entry`／`trap_handler` 與共享核心映射；不畫 xv6 trampoline/uservec。ecall 的 sepc+4 與 page fault retry 分開。 |
| Processes | A/B：PCB、state graph、雙 stack context switch | [xv6 process layout](https://mit-pdos.github.io/xv6-riscv-book/first.html)、[context switch fig. 8.2](https://mit-pdos.github.io/xv6-riscv-book/sched.html)。 | 固定 16 PCB；KernelContext 與 TrapFrame 分工；共用 scheduler stack，無 SMP p->lock。`process.c`、`context.h`、`swtch.S`。 |
| Scheduling | C/D：ready queue、Gantt、目前執行者 | [OSTEP RR figs. 7.6–7.7](https://research.cs.wisc.edu/wind/OSTEP/cpu-sched.pdf)、[VizTools scheduling](https://srg-ics-uplb.github.io/cmsc125/viztools/scheduling/)、[OS Lab](https://oslab.xikai.me/cpu-scheduling)。 | kernel 固定 50 ms；網站可調 quantum 只是教學比較，沒有改 OS。核心不被 timer 搶占，wait/yield 可合作切換。 |
| Memory | A/B/D：VA bit layout、三級表、PTE 與 frame | [xv6 page tables figs. 3.1–3.4](https://mit-pdos.github.io/xv6-riscv-book/mem.html)、[OSTEP page tables](https://research.cs.wisc.edu/wind/OSTEP/vm-smalltables.pdf)。 | 9/9/9/12 bits，4 KiB leaf only。WUWAOS user VA 小於 2^38；不用 xv6 trampoline 地址、超頁或任意硬體 TLB 模擬。 |
| Paging | C：PTE → RAM → swap slot，分成功／rollback | [OSTEP swapping](https://research.cs.wisc.edu/wind/OSTEP/vm-beyondphys.pdf)、本專案 `notes/ch10 Virtual Momory.md`。 | FIFO arrival、pins、VM Gate、先 revoke 再 swap；失敗 restore RAM，外層 fault 才 leave Gate。 |
| I/O | A/C：VirtIO descriptor、avail／used、bounce 與 caller | [1,000 Lines VirtIO](https://operating-system-in-1000-lines.vercel.app/tw/14-virtio)、`notes/ch12  I_O Systems.md` DMA 段落。 | Block completion 為 scheduler poll；512-byte buffer／16-byte alignment；不是 IRQ 完成或直接 user-buffer DMA。 |
| File System | A/C：directory → inode → blocks，log 與 home | [xv6 FS figs. 10.1–10.3](https://mit-pdos.github.io/xv6-riscv-book/fs.html)、[OSTEP journaling](https://research.cs.wisc.edu/wind/OSTEP/file-journaling.pdf)。 | WUFS 80-byte inode、8 direct + 8 indirect、512-byte sector、full-sector bounded redo；不畫 xv6 buffer cache 或 1 KiB block。 |
| Applications | B：Shell → FD/OFD → TTY／pipe／file → syscall | [xv6 interfaces](https://mit-pdos.github.io/xv6-riscv-book/unix.html)、[1,000 Lines syscall](https://operating-system-in-1000-lines.vercel.app/tw/13-system-call)。 | 自訂 ABI；GUI 是同一 user shell image；fan safe replace、受限 ELF，沒有 Linux API／argv 相容性。 |
| Monitoring | A/B：snapshot → 累積 ticks 差 → CPU／RSS／Swap | [OSTEP scheduling metrics](https://research.cs.wisc.edu/wind/OSTEP/cpu-sched.pdf)、`notes/ch5 CPU schedueling.md`。 | CPU 為派發期間時間，含該行程 kernel work；不是硬體 utilization、load average 或純 CPU burst。`workstate.c`。 |
| Writing | A/B：source → chapter／Vue → VitePress → gh-pages | [1,000 Lines 圖文／source 組織](https://operating-system-in-1000-lines.vercel.app/tw/)、本 repository 發布工具。 | repo 是唯一來源；無網站編輯後端；教學模型與 OS 驗收分開，歷史 spec 保留。 |

## 可追溯性與資產政策

所有圖以 Vue/SVG/CSS 重新繪製，來源僅作概念與圖像結構參考。課程筆記可依使用者授權引用敘述，但其中第三方圖片仍需獨立確認授權。靜態向量圖由 source 管理，文字可搜尋／放大；沒有搬運教材截圖、未授權圖像或將外部模擬器嵌入本站。

一般教材概念、WUWAOS source 行為、網站的簡化假設各自標示。網站互動驗證與 OS runtime 驗證分開記錄，參見[詳細計畫](https://github.com/johnson931128/WUWAOS/blob/main/plan.md)。
