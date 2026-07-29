# KeyKey 41 — 倚天紀念版

![Windows 11](https://img.shields.io/badge/Windows%2011-x64%20%2B%20x86-0078D6?logo=windows)
![Version](https://img.shields.io/badge/version-0.9.4--beta.1-B45DB7)
![License](https://img.shields.io/badge/license-MIT-green)

KeyKey 41 是一套 Windows 11 繁體中文注音輸入法，保留 Yahoo! 奇摩輸入法
（Yahoo KeyKey）的使用習慣，並以現代 Windows TSF 架構重新實作。

這是非官方、非營利的紀念與相容性專案，與 Yahoo 或其關係企業無關。
Yahoo 及 Yahoo! 為其權利人的商標。本專案不包含 Yahoo 的線上服務、品牌資產或
已下線的資料庫。

## 主要功能

- KeyKey 風格智慧注音與自動選字
- 倚天 41 鍵、傳統／標準注音兩種鍵盤配置
- 繁體、簡體與英文模式
- 左 Shift 切換中文／英文
- 可設定繁／簡快捷鍵，預設 `Ctrl+F3`
- 全形／半形切換及語言列狀態顯示
- Yahoo KeyKey 習慣的右 Shift 與 Ctrl 符號輸入
- 符號表
- 使用者自建詞庫
- 候選窗顏色設定；預設黑底、白字、紫色選取列
- x64 與真正的 x86/WOW64 TIP，相容新舊 Windows 軟體

詳細涵蓋範圍請參閱
[KeyKey 功能說明](docs/keykey-feature-coverage.md)及
[Windows 相容性與發布檢查表](docs/windows-compatibility-release-checklist.md)。

## 下載與安裝

請從 [Releases](../../releases) 下載最新的 MSI。

目前測試版本：`0.9.4-beta.1`

安裝檔未經商業程式碼簽章。Windows SmartScreen 可能顯示未知發行者；請只從本
repository 的 Releases 下載，並核對 release notes 所列 SHA-256。

AMD64 套件安裝於：

```text
C:\Program Files\KeyKey41\EtenTribute\
```

同一套件包含 x64 與 x86 TIP。Windows 會讓 64-bit 與 32-bit 軟體載入相符的
元件，並共用一個 x64 輸入法 server、設定及詞庫。

## 常用按鍵

| 操作 | 預設按鍵 |
|---|---|
| 中文／英文切換 | 左 Shift |
| 繁體／簡體切換 | Ctrl+F3 |
| 全形／半形切換 | Shift+Space |
| 中文逗號 | 右 Shift+, |
| 中文句號 | 右 Shift+. |
| 左雙引號 | 右 Shift+[ → 『 |
| 右雙引號 | 右 Shift+] → 』 |

快捷鍵、鍵盤配置、候選窗與顏色可在「KeyKey 41 偏好設定」中調整。設定變更會
立即自動儲存。

## 支援範圍

主要目標為 Windows 11 x64，並包含供 32-bit/WOW64 軟體載入的 x86 TIP。
已針對一般 Win32、Office、記事本、瀏覽器及常見檔案對話框設計相容路徑。

密碼欄位、UAC 安全桌面、明確停用 TSF/IMM32 的遊戲或特殊自繪控制項，可能由
Windows 或應用程式禁止第三方輸入法。ARM64 尚未完成原生實機驗證。

## 從原始碼建置

需求：

- Visual Studio 2022，含 Desktop development with C++ 及 x64/x86 tools
- Windows 10/11 SDK
- CMake 3.20+
- WiX Toolset 7 與 UI、Util extensions

```powershell
git clone --recurse-submodules https://github.com/whyren0324/KeyKey41-Eten-Tribute.git
cd KeyKey41-Eten-Tribute
.\build_msi.ps1
```

MSI 會輸出至 `dist\`。技術文件位於 [docs](docs/README.md)。

## 專案來源與授權

本專案以
[win-mcbopomofo](https://github.com/openvanilla/win-mcbopomofo)、
[McBopomofo](https://github.com/openvanilla/McBopomofo)及
[YahooArchive/KeyKey](https://github.com/YahooArchive/KeyKey)
的公開程式、設計概念與使用經驗為基礎延伸。

原始碼依 [MIT License](LICENSE.txt) 發布。第三方元件仍依各自授權條款使用。

