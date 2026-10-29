# Spec 24：WUWAOS 功能示範影片

## 目標與範圍

在真實執行中的 QEMU 操作目前可示範的主要功能，錄成影片並放在 GitHub repository。影片使用獨立 WUFS 與 swap 磁碟，保留既有使用者磁碟；不修改作業系統程式、不加入測試程式碼。

## 錄製方式

使用 WSL Ubuntu 執行 QEMU virt、單核心與 128 MiB RAM；透過標準輸入送出命令，擷取真實序列埠輸出並渲染終端畫面，再以 FFmpeg 編碼 MP4。外框與章節標題是後製顯示；內容是當次 QEMU 輸出，並非 Windows 桌面錄影。影片無旁白。獨立的示範磁碟及暫存錄製工具放在 Git 忽略的 build 目錄，不納入提交。

## 示範內容

1. 開機動畫、shell、help 和清畫面。
2. spawn worker、ps、PCB 檢視、wakeup 與 wait 回收。
3. run counter 的 fork／exec、等待、喚醒與回收；counter 會主動 yield，這段不能單獨證明計時器搶占。
4. demo sync 顯示 Mutex 與 Semaphore 等待、喚醒和回收。
5. writer | reader 的管道與回收。
6. 切換目錄、建立／寫入／讀取／複製檔案，刪除檔案與確認後刪除資料夾。
7. fan 編輯、儲存與 cat 讀回。
8. workstate 顯示 CPU 和記憶體資料，按 q 返回 shell。
9. 降低 VM 常駐頁預算、配置／寫入頁面、查看缺頁與 swap 計數、讀回頁面，再清理並恢復預算。
10. 行程狀態、help 及退出 QEMU。

## 驗證與限制

實際 QEMU 終端紀錄確認 worker 和 counter 收集、sync demo 完成、pipe 傳輸、檔案寫入與 fan 讀回；VM 統計顯示 swap_in=7、swap_out=14，頁 0 的三處資料均讀回 65。抽查影片中編輯器、退出後畫面、workstate 及 VM 畫面；影片為約 2 分鐘、1280×720、H.264 MP4、6 fps。各功能的其他邊界並未由此影片完整驗證，顯示畫面只是一次示範。錄製未使用或修改原本的 build/wuwaos.img。

影片：[WUWAOS QEMU demo on GitHub](https://github.com/johnson931128/WUWAOS/blob/main/docs/media/wuwaos-demo.mp4)。

README 另提供由同一 MP4 轉製的 960×540、4 fps 完整 GIF 動畫，可直接播放但沒有播放控制；原始 MP4 保留較高畫質與播放控制。
