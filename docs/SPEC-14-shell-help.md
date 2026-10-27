# Spec 14：逐行命令說明

## 目標與範圍

將難以閱讀的 help 命令列表改成每行 `<cmd> <description>`，命令與參數在左，右側對齊用途說明。只修改 user/shell.c 的說明文字，不更動命令解析、系統呼叫與核心；不新增測試程式碼。確認符合規格並驗證後依 AGENTS.md commit／push，不自動部署公開網站。

## 設計與步驟

- 保留既有 ASCII 英文終端說明；以固定空格讓描述從第 33 欄開始，所有說明行小於 80 欄。
- spawn／run 的內建程式、trace on／off、vmstat 與 mem 各種操作分別列出，不再用一串管線符號壓在同一行。writer | reader 的管線語法保留。
- 每行提供相應用途；worker 的事件通道 7、counter 的通道 9、wait 回收、絕對路徑、零起算頁號、單 token 寫入與退出 QEMU 的操作仍有說明。
- 行程、檔案、記憶體以空白行分隔；沒有新增翻頁或其他命令。沿用單一 user_puts 的固定字串輸出，不配置額外緩衝區。
- 建置確認使用者映像仍符合 linker 的 16 KiB code／4 KiB data 限制，於 QEMU 正常輸入 help 確認對齊、內容與返回提示字元，更新文件。

## 驗收紀錄

2026-10-27，WSL Ubuntu／Clang 18／QEMU 8.2.2：

- make 通過 -Wall/-Wextra/-Werror；kernel.elf 與 user.elf 無未解析符號，user.elf text=9696、data=0、bss=24 bytes，符合原有映像限制。
- 使用獨立的既有 timeout-check.img，以 readonly 模式啟動 QEMU，正常輸入 help。29 行命令各有用途說明，包含分開的 spawn／run／trace／mem 操作，輸出結束返回 wuwaos>。
- 核對所有描述欄均從第 33 欄開始；保留 worker／counter 的喚醒通道、wait 回收、fan 快捷鍵與路徑／頁號提示。
- 日誌位於忽略的 build/help-console.log。Ctrl+A、X 正常結束 QEMU，未修改原有使用者磁碟，沒有新增測試程式碼。
