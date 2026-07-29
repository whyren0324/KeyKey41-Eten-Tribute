# Windows 相容性與 GitHub 發布檢查表

本文件記錄 KeyKey 41（倚天紀念版）在 Windows 11 上的相容性設計決策。
修改安裝程式、TSF TIP、語言列或 GitHub 發行流程時，必須同步檢查本文件。

## 1. 支援範圍

Windows 11 x64 仍會執行大量 32-bit（Win32/WOW64）軟體，因此 AMD64
安裝套件必須同時包含兩個真正的原生 TIP：

- `McBopomofoTIP_x64.dll`：供 64-bit 軟體載入。
- `McBopomofoTIP_x86.dll`：供 32-bit／WOW64 軟體載入。

不可把同一份 x64 DLL 複製後改名成 `_x86.dll`。兩份 DLL 必須分別由
CMake `-A x64` 與 `-A Win32` 編譯，發布前應檢查 PE machine：

- x64：`8664`
- x86：`14C`

ARM64 必須另行原生編譯與實機驗證。在完成前，不得宣稱 AMD64 套件已提供
原生 ARM64 支援，也不得把 x64 檔案改名成 ARM64 後註冊。

## 2. 安裝位置與 Registry View

AMD64 套件統一安裝到：

`C:\Program Files\KeyKey41\EtenTribute\`

不需要因為包含 x86 TIP 就把整套軟體放到 `Program Files (x86)`。DLL
能否被載入取決於 PE 架構與正確的 COM registry view，而不是檔案所在資料夾。

在 Windows x64 上必須：

1. 使用 32-bit `regsvr32` 註冊 x86 TIP，寫入 32-bit COM registry view。
2. 使用 64-bit `regsvr32` 註冊 x64 TIP，寫入 64-bit COM registry view。
3. 先註冊 x86、最後註冊 x64，讓共用 TSF profile 的描述與圖示路徑最後指向
   原生 x64 DLL。
4. 解除安裝時兩個 registry view 都必須解除註冊。

## 3. 單一 Server 架構

x64 與 x86 TIP 都透過相同 named pipe 與同一個 x64
`McBopomofoServer_x64.exe` 通訊，並共用：

- 語言模型與 OpenCC 資料
- 使用者詞庫
- `mcbopomofo.ini`
- 候選窗設定與繁簡設定

在 AMD64 Windows 上不可同時自動啟動 x86 與 x64 server，否則可能搶占
named pipe、重複顯示 UI 或產生設定競爭。

## 4. 語言列與繁／簡／英狀態同步

有些軟體會為「開啟舊檔」、「另存新檔」、自繪文字框或浮動視窗建立新的：

- TSF document manager
- TSF context
- UI thread
- Win32 對話框

因此不能只在使用者按下左 Shift 時刷新語言列。TIP 必須在下列事件重新發布
語言列 icon、文字與 tooltip：

- `ITfKeyEventSink::OnSetFocus`
- `ITfThreadMgrEventSink::OnSetFocus`
- `ITfThreadFocusSink::OnSetThreadFocus`
- `ITfThreadMgrEventSink::OnPushContext`
- `ITfThreadMgrEventSink::OnPopContext`
- `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` 的 `OnChange`

新 TIP instance 啟用時，不可無條件把
`GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` 設為中文。只有 compartment 尚無有效值
時才可初始化；否則必須保留 Windows 或 host application 提供的狀態。

## 5. 需要涵蓋的應用類型

每次候選版至少應人工測試：

- 64-bit Office（例如 PowerPoint）
- 64-bit 記事本
- Chromium／Edge 類瀏覽器
- 32-bit Win32／WOW64 軟體
- 一般 Win32「另存新檔」對話框
- 軟體自製的檔名輸入框
- 主視窗與對話框分屬不同 UI thread 的程式
- 切換視窗、切換分頁、開啟浮動視窗後的第一個輸入框

每個場景至少確認：

- 左 Shift 可在繁／英或簡／英之間切換。
- 實際輸出與右下角「繁／簡／英」一致。
- Ctrl+F3（或使用者設定的組合）可切換繁／簡。
- x86 與 x64 程式可使用相同設定、詞庫和候選窗配色。

## 6. 合理限制

下列情況可能由 Windows 或 host application 明確禁止輸入法，不應嘗試繞過：

- 密碼或安全輸入欄位
- UAC／安全桌面
- 明確停用 TSF／IMM32 的遊戲或自繪控制項
- 未提供任何文字輸入介面的特殊 canvas
- 系統政策禁止第三方輸入法的環境

文件與 GitHub README 應使用「盡量涵蓋一般 Windows 軟體」，不可宣稱
「保證支援所有軟體」。

## 7. GitHub 發布前檢查

- [ ] 原始碼版本、EXE/DLL VERSIONINFO、MSI ProductVersion 一致。
- [ ] 新 MSI 使用新版本號，避免 Windows Installer 快取舊 DLL。
- [ ] x64 與 x86 TIP 均由對應 toolchain 重新編譯。
- [ ] PE machine 驗證為 x64 `8664`、x86 `14C`。
- [ ] x64 單元／回歸測試全部通過。
- [ ] x86 TIP 至少完成 Release 編譯與載入測試。
- [ ] MSI 在 AMD64 上同時註冊 x86 與 x64 TIP。
- [ ] 僅啟動 x64 server。
- [ ] 升級會先關閉已載入舊 TIP 的程式。
- [ ] 解除安裝會清除兩個 COM registry view。
- [ ] 實測繁／簡／英圖示與實際輸出一致。
- [ ] 實測 32-bit 軟體及其「另存新檔」檔名欄位。
- [ ] 實測候選窗黑底、白字、紫色選取列與自訂配色。
- [ ] 更新 `README.md`、版本說明、已知限制與 SHA-256。
- [ ] 發布檔名能清楚表示 Windows x64 套件內含 x86 相容 TIP。
- [ ] 若尚未實測 ARM64，不在 release notes 宣稱原生 ARM64 支援。

