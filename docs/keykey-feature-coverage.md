# KeyKey 功能對照與 Win11 實作

本專案不是把舊式 Windows IMM DLL 重新編譯，而是以 Windows 11 支援的 TSF
Text Service 實作相同類型的輸入體驗。KeyKey 原始專案的 OpenVanilla /
Manjusri 智慧注音概念，由現代 McBopomofo 語言模型與組字格引擎承接。

| 功能面向 | Windows 11 實作 |
|---|---|
| 智慧注音與詞句自動選字 | `KeyHandler`、`ReadingGrid`、`McBopomofoLM` |
| 傳統逐字注音 | Plain Bopomofo input mode |
| 候選字、翻頁、游標選詞 | TSF UI Element + 自訂候選窗 |
| 選字學習 | `UserOverrideModel` |
| 使用者詞彙 | `UserPhrasesLM` |
| 關聯詞 | `AssociatedPhrasesV2` |
| 中文標點與符號 | 標點語言模型、符號／Unicode 輸入狀態 |
| 中文／英文、全／半形 | TSF 語言列按鈕與輸入狀態 |
| 繁簡轉換 | OpenCC |
| 詞典服務 | `DictionaryService` |
| 設定介面 | 原生 Win32 設定程式 |
| Win11 應用相容性 | x64/x86/ARM64 TSF TIP |

## 本發行版的鍵盤政策

設定介面只提供 `ETen` 與 `Standard`，預設值是 `ETen`。這裡的 ETen 是完整的
倚天 41 鍵配置，而不是倚天 26 鍵；Standard 是傳統／標準注音鍵盤。引擎內保留
其他配置的解析程式碼，只為維持上游測試與資料相容性，不會在產品設定中顯示。

## 舊 KeyKey 中未直接照搬的部分

Yahoo 搜尋熱門詞、線上更新、Yahoo 品牌服務及舊 CEROD 加密資料庫屬於已下線
的網路／商業服務，不適合在新的輸入法中重現。其餘與中文輸入直接相關的核心
能力由本專案的離線元件提供。
