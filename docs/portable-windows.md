# Windows portable 編譯與執行環境

這個發行包包含 WUWAOS 原始碼，以及已安裝完成的 MSYS2、Clang、LLD、LLVM、Make、Python 和 QEMU。解壓後可以離線編譯與執行，不需要 installer、管理員權限或 WSL，也不會修改系統 PATH。

適用於 Windows 10／11 x64。請解壓到可寫入的本機目錄，例如 `C:\WUWAOS`；建議路徑使用英數字，避免空白及特殊字元。公司電腦仍須允許執行下載的程式。

## 下載與使用

到 [Windows portable v1 Release](https://github.com/johnson931128/WUWAOS/releases/tag/portable-windows-v1) 下載 `WUWAOS-portable-windows-x64.zip`（約 1.41 GiB），完整解壓後進入其中的 `WUWAOS-portable-windows-x64` 目錄。請預留約 8 GiB 磁碟空間。

README 的 [Windows portable release](https://github.com/johnson931128/WUWAOS#windows-portable-release) 章節提供可直接貼上執行的 PowerShell 指令，包含下載、SHA-256 校驗、解壓與啟動。環境設定就是解壓完整發行包，不需要再安裝編譯器、Python、QEMU 或 MSYS2。

在 PowerShell 執行：

```powershell
# 檢查隨附工具
.\Start-WUWAOS.cmd check

# 編譯核心並建立新的 WUFS 與 swap 映像
.\Start-WUWAOS.cmd build

# 開啟圖形桌面；需要時會自動重新編譯
.\Start-WUWAOS.cmd gui

# 文字模式；需要時會自動重新編譯
.\Start-WUWAOS.cmd

# 進入使用隨附工具的 Bash
.\Start-WUWAOS.cmd shell
```

請勿同時啟動兩個使用相同磁碟映像的 QEMU。圖形模式關閉 QEMU 視窗即可結束；文字模式按 Ctrl+A，放開後按 X。磁碟資料保留在 `build`，重新下載發行包前可先備份自己的映像。

包內不含原電腦的 WUFS／swap 映像、使用者目錄、下載快取、執行紀錄或套件簽章私鑰。第一次編譯會建立全新的磁碟映像。啟動入口不讀取個人 shell 設定，也不執行套件更新；一般編譯與開機不需要網路。

此環境用於 Windows x64；macOS、Linux 和其他 CPU 平台請另行準備對應工具。

## 授權與套件來源

WUWAOS 原創程式碼使用 MIT 授權；桌面字型使用 SIL OFL 1.1。隨附的 MSYS2 與開發工具各自依其原有授權散布，不能把整個發行包一概視為 MIT 授權。

完整說明見原始碼根目錄的 `THIRD_PARTY_NOTICES.md`、`licenses/README.md` 及發行包內的 `PORTABLE-MANIFEST.json`。套件清單記錄版本、授權、上游與原始碼套件連結，並保留工具原本附帶的授權文件。再次散布工具時，仍需遵循各套件的授權條款與適用的對應原始碼要求。

## 重新打包

在已有完整 portable 環境的專案根目錄執行：

```powershell
& .\build\portable\msys64\ucrt64\bin\python.exe .\tools\package-portable.py
```

輸出在 `build\releases`：

- `WUWAOS-portable-windows-x64.zip`：原始碼與環境。
- `WUWAOS-portable-windows-x64.zip.sha256`：壓縮包的 SHA-256。
- `WUWAOS-portable-windows-x64.manifest.json`：來源 commit、工作目錄是否有修改，以及套件版本、授權與來源連結。壓縮包內也有同一份清單。

打包器只收錄 Git 已追蹤的原始碼，以及打包／啟動所需的新檔案。其他新原始碼需先加入 Git，才會被收錄。預設不覆寫既有壓縮包；再次打包可使用 `--output build/releases/另一個檔名.zip`。

可用以下指令核對下載檔案的雜湊值：

```powershell
Get-FileHash .\WUWAOS-portable-windows-x64.zip -Algorithm SHA256
```

## 上傳 GitHub

把 ZIP、`.sha256` 和 `.manifest.json` 當作 GitHub Release 附件上傳。不要將整個 `build/portable` 加入 Git；原始碼 repository 只需要保留啟動入口、打包工具與說明。GitHub Release 的單一附件需小於 2 GiB。

發布後續版本時，請在專案頁的 **Releases → Draft a new release** 選擇新的版本 tag、上傳上述三個附件，再發布。若電腦已設定 GitHub CLI，也可以使用（範例採用下一個版本的 tag）：

```powershell
gh release create portable-windows-v2 `
  .\build\releases\WUWAOS-portable-windows-x64.zip `
  .\build\releases\WUWAOS-portable-windows-x64.zip.sha256 `
  .\build\releases\WUWAOS-portable-windows-x64.manifest.json `
  --draft --title "WUWAOS Windows portable environment" `
  --notes "Windows x64 offline build and QEMU environment. Extract the ZIP and run Start-WUWAOS.cmd gui."
```

只有 GitHub Release 附件包含工具環境；GitHub 自動產生的 Source code ZIP 不包含 `build/portable`。
