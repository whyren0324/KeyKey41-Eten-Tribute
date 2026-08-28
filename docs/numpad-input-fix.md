# 中文模式右側數字鍵盤修正

## 問題

啟用中文輸入法且組字區為空時，即使 Num Lock 已開啟，右側數字鍵盤的數字仍可能無法輸入到目前的應用程式。

## 原因

TSF 會先呼叫 `OnTestKeyDown` 詢問輸入法是否要處理按鍵。原本的邏輯會讓空組字狀態下的方向鍵及編輯鍵直接交回應用程式，但 Num Lock 開啟後產生的 `VK_NUMPAD0` 到 `VK_NUMPAD9` 等按鍵會先被輸入法宣告攔截。

輸入法伺服器稍後雖然會在空組字狀態判定不處理這些按鍵，部分 TSF 應用程式卻不會重新送出已在 `OnTestKeyDown` 被攔截的按鍵，因此使用者看不到數字輸出。

## 修正方式

在 `src/Client/McBopomofoTIP.cpp` 增加 `IsNumberPadPrintableKey`，辨識以下按鍵：

- `VK_NUMPAD0` 到 `VK_NUMPAD9`
- `VK_DECIMAL`
- `VK_ADD`
- `VK_SUBTRACT`
- `VK_MULTIPLY`
- `VK_DIVIDE`
- `VK_SEPARATOR`

當中文輸入法開啟、但目前沒有組字內容或候選字時，`OnTestKeyDown` 對上述按鍵直接回傳不攔截，讓目前的應用程式原生處理。

正在組字或顯示候選字時，仍保留原本的數字鍵盤組字及標點處理，不改變既有行為。Num Lock 關閉時，數字鍵盤送出的方向鍵、Home、End、Page Up、Page Down、Insert、Delete 等按鍵也維持原有功能。

## 已完成驗證

- Release 版本的 `McBopomofoTIP` 已成功編譯。
- 輸出：`build/bin/Release/McBopomofoTIP_v2.dll`

## 發布前人工測試

請至少在記事本及一個常用的 TSF 應用程式（例如瀏覽器或 Office）測試：

- [ ] 中文模式、組字區為空、Num Lock 開啟：右側 `0` 到 `9` 都能輸出半形數字。
- [ ] 中文模式、組字區為空、Num Lock 開啟：右側 `.`、`+`、`-`、`*`、`/` 可正常輸入。
- [ ] 中文模式、Num Lock 關閉：方向鍵、Home、End、Page Up、Page Down、Insert、Delete 行為正常。
- [ ] 已有注音組字時：右側數字鍵盤不會造成崩潰或遺失既有組字內容。
- [ ] 候選字視窗開啟時：數字鍵盤行為與修正前一致。
- [ ] 英文模式下：右側數字鍵盤行為正常。
