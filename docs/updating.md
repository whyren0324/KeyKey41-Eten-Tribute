# KeyKey 41 更新工具

更新包支援 Windows x64，需先安裝一次含 x64/x86 TIP 的輸入法。
更新時保持以同一個 Windows 使用者登入，先完成／送出正在組字的內容，再以系統管理員執行更新包中的 `Update.cmd`。

## 日常更新

選單提供：

1. Backend：快速更新後端及設定程式，不切換前端 DLL。
2. Full：更新後端與 x64/x86 前端，適用右 Shift 等按鍵修改。
3. Status：顯示前端註冊路徑、實際載入前端的程式、後端路徑與檔案版本。
4. Rollback：恢復上一份更新前快照；再次回復可切換回另一版。
5. Validate only：唯讀驗證完整更新包，不停程式、不改登錄。
6. Restore MSI baseline：恢復第一次更新前的原始安裝，供 MSI 維護前使用。

更新不用先移除輸入法。工具將新版放在安裝目錄 `updates\<包版本及識別碼>`，
正常停止同工作階段的輸入法後端／設定視窗，切換指標並啟動新版。
新後端須回覆 IPC 版本與實際 PID，才能判定啟動成功。
使用者詞庫、偏好與選字紀錄位於 AppData，更新不覆蓋這些檔案。

完整更新後，已開啟程式可能繼續載入舊 DLL。工具列出它們，讓使用者存檔後重開。
若 Windows 的 COM／TSF 快取仍未切換，登出再登入。工具不會強制終止瀏覽器、重啟 Explorer、ctfmon 或整台電腦。
狀態檢查可能無法讀取部分保護程序的模組，會明確列出不完整檢查數量。

第一次從舊版升級時，舊 DLL 尚不認得 ActiveServerPath；既有程式若在更新期間重新啟動舊後端，
健康檢查會失敗並嘗試回復。存檔、關閉測試程式後再更新；首次切換後登出登入可讓所有程式使用新前端。
之後的新版 DLL 會從機器登錄讀取 ActiveServerPath，後端更新後會啟動選定的新版。

## 指令模式

```powershell
# 在更新包根目錄執行；更新／回復需同帳戶提升權限
.\scripts\Update-KeyKey.ps1 -Action Backend
.\scripts\Update-KeyKey.ps1 -Action Full
.\scripts\Update-KeyKey.ps1 -Action Status
.\scripts\Update-KeyKey.ps1 -Action Rollback
.\scripts\Update-KeyKey.ps1 -Action Baseline
.\scripts\Update-KeyKey.ps1 -Action Plan -PlanMode Full
```

Backend 包只可選 Backend；Full 包兩種模式皆可用。
檔案版本相同的開發 build 會以不同更新包 ID／目錄區分，狀態顯示版本加路徑。
更新包為本機可信 build，雜湊可驗證完整性，不構成簽章或來源認證。

## 開發者建立更新包

先編譯 Release 的 x64 Server／Config／TIP，Full 更新還需編譯 x86 TIP：

```powershell
.\build_update.ps1 -Mode Full -X64BuildRoot build -X86BuildRoot build_x86
# 只改後端時，可不提供 x86 build
.\build_update.ps1 -Mode Backend -X64BuildRoot build
```

OpenCC 字典預設使用 `build_brian`，可透過 `-OpenCCBuildRoot` 指定其他原生字典 build。
缺檔、版本不一致、PE 架構錯誤、校驗碼不符時拒絕更新。
打包程式不自動執行安裝、不自行修改系統。

## 回復與維護

更新前寫入 `updates\previous.json`，包含 x64/x86 註冊路徑、後端指標、目前使用者自動啟動值與 SID。
失敗時自動恢復指標並嘗試啟動舊後端。舊檔永不在更新時刪除，故仍載入舊 DLL 的程式可正常繼續。
電源中斷等情況可使用 Rollback；若自動回復失敗，錯誤訊息會指出快照位置。
回復舊後端時可只驗證程序存活，因較舊版本沒有新版健康命令。
更新工具不卸載 TSF profile，直接保留身份並切換既有 COM InProcServer32 預設值。
圖示資源可仍指向 MSI 原始 DLL，原始安裝檔案必須保留。

更新包目前是 MSI 安裝之上的開發更新層，MSI 不追蹤 updates 內的檔案。
**要再執行 MSI 修復、升級或移除前，必須先將更新層回復至 MSI 的原始前端／後端**；
多次更新時 Rollback 僅保留上一版，應保存初始基線快照（工具保存在 `baseline.json`）。
不要在已載入舊 DLL 的程式尚未退出時清理舊版本目錄。
此版本不支援跨 IPC 協定大改、不支援 ARM64／原生 32 位元 Windows、不自動清理版本目錄。

## 驗證範圍

交付時應完成：PowerShell 語法解析、更新包唯讀驗證、損壞／缺檔／架構不符／路徑跳脫測試、
前端 x64/x86 與後端編譯、既有 CTest。
實際 registry 切換、回復、UAC、同帳戶／多工作階段、應用程式保持開啟的行為需在已安裝環境驗證。
不能以 Plan 成功當成已完成實機更新。
