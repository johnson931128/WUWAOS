# Spec 29：Windows 單指令 QEMU 桌面啟動器

日期：2026-11-01

狀態：已實作；本地 PowerShell、啟停、viewer、lock 與磁碟保護驗收完成。

## 目標

- Windows/WSL1 使用者從 repo 根目錄執行一個 PowerShell 指令，即完成建置、背景 QEMU 啟動及原生 VNC viewer 開啟。
- 關閉 viewer 時，透過本次 QMP session 關閉本次 QEMU；若 QMP 無法關閉，僅在 Linux PID 與唯一 VM 名稱相符時停止該 QEMU。
- 保留 `make run` 的 UART 文字操作，以及 Linux/WSLg 的 `make run-gui` 和手動 `make run-gui-vnc`。
- 防止建置更新被執行中 QEMU 使用的磁碟映像；提供 `-Snapshot` 使用獨立暫存映像。

## 工作範圍

新增 `tools/run-desktop.ps1`，支援 Windows PowerShell 5.1 與 PowerShell 7。Viewer 可由 `-ViewerPath` 指定，或從 PATH、專案 `build/tigervnc` 及 TigerVNC/TightVNC 標準安裝目錄尋找。VNC 和 QMP 使用動態、loopback-only 連接埠；啟動器使用獨立 session 名稱、pidfile、log 目錄與排他 lock。建置和 QEMU 輸出記錄在 `build/desktop-<session-id>`。

Makefile 將 GUI block image 路徑、VM name、serial output 和 pidfile 變成可覆寫變數，由 launcher 傳入，不維護另一份 QEMU 參數清單。預設磁碟路徑維持既有 `build/wuwaos.img` 和 `build/wuwaos-swap.img`，客體寫入照常保存。`-Snapshot` 將兩個 image 複製到本次 session 目錄，建置更新及 QEMU 都使用副本；結束後刪除副本並保留記錄。

啟動前檢查 WSL 執行環境及 running distributions 的 QEMU command line，若有程序參照本 repo WUFS 映像即拒絕更新。排他 lock 防止兩個 launcher 同時使用 repo images。透過 QMP greeting、capabilities 和 `query-name` 確認端點屬於本次 instance；正常關閉使用 QMP `quit`。Fallback 由 Linux `/proc/<pid>/cmdline` 驗證 `-name` 後才送 SIGTERM，不使用全名程序終止。

不新增測試程式碼，不更動 WSL/Windows 安全設定、AGENTS.md 或既有未追蹤檔案。

## 執行步驟

1. 解析 repo 相對路徑，檢查磁碟映像、viewer、wsl.exe、WSL toolchain、運行中 QEMU 及 launcher lock。
2. 選擇未使用的 loopback VNC/QMP ports；必要時建立隔離磁碟副本。
3. 前景等待 WSL `disk-image` 建置結束；此階段不啟動 VM。建置完成後才透過共用 QEMU recipe 的 `run-gui-vnc-session` 啟動背景 VM，將 serial 轉存至 session log，並接上本次 QMP、pidfile 及磁碟路徑。`StartupTimeoutSeconds` 只計算 QEMU/QMP ready，不包含建置時間。
4. 以 QMP greeting、`qmp_capabilities` 和唯一 VM name 驗證啟動，再開啟 Windows viewer；限時檢查 viewer process 與 QEMU 的 VNC client/service。
5. 監看 viewer 和 QEMU；任一結束時關閉另一側，等待程序停止及 ports 釋放，移除 snapshot 副本並釋放排他 launcher lock（空 lock 檔可保留）。
6. 更新 README、開機主題、程式碼導覽和歷史索引，記錄實際驗收及未執行項目。

## 驗收條件

- PowerShell 5.1 和 PowerShell 7 能解析並執行 launcher。
- Viewer 正常開啟，QMP `query-vnc` 可見 loopback client；關閉 viewer 後本次 QEMU 結束、VNC/QMP ports 釋放。
- QEMU 意外結束時 viewer 關閉；invalid viewer、依賴缺少、啟動逾時或 port 衝突有清楚錯誤並清理本次建立程序。
- 已有 QEMU/launcher 時拒絕第二次啟動，不終止其他 VM；失敗及 Snapshot 驗收前後，原始磁碟 SHA-256 相同。
- 文件建置成功；不新增任何測試程式碼。

## 驗收紀錄

環境為 Windows PowerShell 5.1、PowerShell 7、WSL1 Ubuntu、QEMU 8.2.2 與 TigerVNC 1.16.2；沒有新增測試程式碼。

- Windows PowerShell 5.1 與 PowerShell 7 parser 均通過；`npm run docs:build` 通過（VitePress 對大 chunk 發出既有體積提示）。
- 以 `powershell.exe -NoProfile -File tools/run-desktop.ps1 -Snapshot -StartupTimeoutSeconds 60` 實際建置並啟動。serial log 確認 VirtIO GPU 1280×720 scanout、tablet ready 及 WUWAOS shell 開機完成；launcher 的 QMP `query-vnc` 顯示 loopback service `5991` 與 client `127.0.0.1`。TigerVNC stderr 記錄已連到 `127.0.0.1:5991` 並完成 RFB 3.8 協議協商。
- 對本次 TigerVNC PID 發出停止後，launcher 以 QMP 正常關閉本次 QEMU並回傳 exit code 0。之後 WSL 沒有本次 QEMU；VNC `5991` 與 QMP `44882` TCP ports 均無 listener，Snapshot images 已刪除，session logs 保留。
- 啟動前後原始 `build/wuwaos.img` SHA-256 均為 `6BA61B2E9E6C5AF4931DD39D338619BF66BF25C5639BB916ADC59B6B62FD4989`；原始 swap SHA-256 均為 `F6DDB73F54CF434916200D752A0596B14DA7D6534FE0F8CF454E6B1C02078355`。
- QEMU/viewer 活動時第二次 launcher 在建置前被 OS file lock 拒絕。`-Snapshot -ViewerPath C:\missing\viewer.exe` 於 viewer preflight 回報明確錯誤，未建置或修改映像；原始 image hashes 維持相同。
- 使用者另回報已驗證桌面畫面；此紀錄不把該回報擴大為人工滑鼠操作或本輪新截圖證據。

## 限制與未驗證

- 本輪未刻意造成 QMP readiness timeout、QEMU 自行崩潰或 VNC 連接埠競態；其 cleanup 路徑以唯一 name、pidfile 與 Linux cmdline ownership check 實作，未作故障注入驗收。
- 本輪未另外執行 QMP `screendump`；Spec 28 保留的 guest framebuffer 截圖描述桌面像素，這一輪驗證的是 Windows viewer 的實際 VNC client 連線與關閉生命週期。
- Windows viewer 實體滑鼠操作及 UI 自動擷取仍沿用 Spec 28 的 `NOT_RUN` 限制；QMP client 證據不代表人工滑鼠操作驗收。
