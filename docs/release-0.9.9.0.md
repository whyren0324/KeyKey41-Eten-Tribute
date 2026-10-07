# KeyKey 41 v0.9.9.0 — 2026-10-07

## 繁體中文

### 本次更新

- 單按左或右 Shift 都可以切換中文／英文；保留右 Shift 快捷標點。
- 手動選字會記錄選用次數、最近使用及上下文，並保存到本機；重啟輸入法後仍可使用學習結果，改善已選過的詞又回到舊候選的情況。
- 支援長詞學習與同讀音備援。這是本地選字學習改良，尚未接入外部訓練程式或 AI 即時排序。
- 新增獨立更新工具，支援完整更新、後端更新、查看狀態與回復版本；不用先移除輸入法，也不強制關閉使用者程式。
- 設定標題顯示版本；「關於」顯示設定程式、執行中後端及已註冊的 x64/x86 前端版本，並可開啟版本紀錄。

### 如何更新已安裝的輸入法

適用於已安裝 KeyKey 41 的 Windows x64 電腦，不適用於 ARM64。更新包包含 x64 與 x86 前端，支援 32 位元應用程式。

1. 下載本 Release 附件 `KeyKey41-0.9.9.0-update-x64.zip`，完整解壓縮。不要只複製 `Update.cmd`，也不要直接在 ZIP 內執行。
2. 在 `Update.cmd` 上按右鍵，選「以系統管理員身分執行」。請使用平常登入的同一個 Windows 帳號，不要換另一個管理員帳號。
3. 輸入 `2`，選擇 Full（完整更新）。本版修改了前端按鍵行為，不能只更新後端。
4. 等待顯示 `Update complete`。關閉並重開使用輸入法的應用程式，讓它們載入新版前端。
5. 從新版輸入法托盤開啟設定，確認版本為 `0.9.9.0`；「關於」可查看各元件版本與版本紀錄。

通常不需要重新開機。已開啟的程式仍可能載入舊 DLL；若重開應用程式後仍未切換，請登出 Windows 再登入。已註冊版本不等於所有執行中程式的載入版本。

更新失敗會嘗試自動回復原登錄與後端。工具選單 `3` 可查看狀態、`4` 可回復上一版、`6` 可回復原 MSI 基線；進行 MSI 維護前請先回復基線。舊版本檔案會保留，更新工具不會刪除個人詞彙或學習資料。

尚未安裝輸入法的電腦：本附件不是首次安裝程式。請先從 [v0.9.8-beta.1](https://github.com/whyren0324/KeyKey41-Eten-Tribute/releases/tag/v0.9.8-beta.1) 安裝 MSI，再套用本更新包。

驗證：9 組 CTest 功能測試及 7 項更新工具測試通過；已在現有安裝上驗證完整更新與新版 IPC 回應。

## English

### What's new

- Tapping either left or right Shift switches between Chinese and English. Right-Shift punctuation shortcuts are preserved.
- Manual candidate choices now persist locally, including frequency, recency, and context. Learning survives backend restarts and helps previously selected phrases remain preferred.
- Learning supports longer phrases and a same-reading fallback. This is local candidate-learning improvement, not integration with an external trainer or real-time AI ranking.
- A standalone updater supports full updates, backend-only updates, status inspection, and rollback. No prior uninstall or forced closure of user applications is required.
- The settings title displays its version. About shows the settings application, running backend, and registered x64/x86 frontend versions, and provides access to version history.

### Updating an existing installation

For Windows x64 computers with KeyKey 41 already installed; ARM64 is not supported. The package includes x64 and x86 frontends for both 64-bit and 32-bit applications.

1. Download the attached `KeyKey41-0.9.9.0-update-x64.zip` and extract the entire archive. Do not copy only `Update.cmd` or run it inside the ZIP.
2. Right-click `Update.cmd` and select **Run as administrator**. Elevate using your usual Windows account, not a different administrator account.
3. Enter `2` for **Full update**. This release changes frontend key handling, so a backend-only update is insufficient.
4. Wait for `Update complete`, then close and reopen applications that use the input method so they load the new frontend.
5. Open settings from the updated input method's tray menu and verify `0.9.9.0`. About shows component versions and version history.

A reboot is usually unnecessary. Existing applications may retain the old DLL; if reopening them is insufficient, sign out of Windows and sign in again. Registered versions may differ from versions loaded by running applications.

On failure, the updater attempts to restore the previous registration and backend. Menu option `3` inspects status, `4` rolls back one update, and `6` restores the original MSI baseline. Restore the baseline before MSI maintenance. Previous binaries are retained, and the updater does not delete personal phrases or learning data.

For a computer without KeyKey 41: this attachment is not a first-time installer. Install the MSI from [v0.9.8-beta.1](https://github.com/whyren0324/KeyKey41-Eten-Tribute/releases/tag/v0.9.8-beta.1), then apply this update package.

Verification: all 9 CTest suites and 7 updater tests passed. Full updating and the new IPC response were also verified on an existing installation.
