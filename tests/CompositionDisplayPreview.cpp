#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include "TooltipWindow.h"

namespace {

constexpr wchar_t kWindowClass[] = L"KeyKey41CompositionDisplayPreview";
bool g_keyKeyMode = false;
TooltipWindow g_preeditWindow;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) {
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT client = {};
      GetClientRect(hwnd, &client);
      FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

      HFONT font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH, L"Microsoft JhengHei UI");
      HGDIOBJ oldFont = SelectObject(hdc, font);
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(35, 35, 35));
      const wchar_t* title = L"KeyKey 41 組字顯示測試";
      TextOutW(hdc, 34, 34, title, lstrlenW(title));

      SetTextColor(hdc, RGB(70, 70, 70));
      const wchar_t* committedText = L"已確認文字：這是一個";
      TextOutW(hdc, 34, 96, committedText, lstrlenW(committedText));
      SIZE committed = {};
      GetTextExtentPoint32W(hdc, committedText, lstrlenW(committedText),
                            &committed);

      if (!g_keyKeyMode) {
        SetTextColor(hdc, RGB(180, 65, 183));
        const wchar_t* preedit = L"尚未確認的測試";
        TextOutW(hdc, 34 + committed.cx, 96, preedit, lstrlenW(preedit));
        SetTextColor(hdc, RGB(105, 105, 105));
        const wchar_t* description =
            L"模式 1：文字留在輸入位置，以設定的顏色提示。";
        TextOutW(hdc, 34, 150, description, lstrlenW(description));
      } else {
        SetTextColor(hdc, RGB(105, 105, 105));
        const wchar_t* description =
            L"模式 3：未確認文字顯示在輸入點旁的獨立視窗。";
        TextOutW(hdc, 34, 150, description, lstrlenW(description));
      }

      SelectObject(hdc, oldFont);
      DeleteObject(font);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      g_preeditWindow.Destroy();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
  g_keyKeyMode = commandLine && wcsstr(commandLine, L"keykey");

  WNDCLASSEXW windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = instance;
  windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32513));
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  windowClass.lpszClassName = kWindowClass;
  RegisterClassExW(&windowClass);

  HWND hwnd = CreateWindowExW(
      0, kWindowClass,
      g_keyKeyMode ? L"KeyKey 41 — Yahoo KeyKey 浮動組字框測試"
                   : L"KeyKey 41 — 未確認文字顏色測試",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 180, 180, 850, 280, nullptr,
      nullptr, instance, nullptr);
  if (!hwnd) return 1;

  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  if (g_keyKeyMode) {
    RECT windowRect = {};
    GetWindowRect(hwnd, &windowRect);
    g_preeditWindow.SetKeyKeyPreeditStyle(true);
    g_preeditWindow.Create(instance);
    g_preeditWindow.UpdateUI("尚未確認的測試");
    g_preeditWindow.Move(windowRect.left + 390, windowRect.top + 92);
  }

  MSG message = {};
  while (GetMessageW(&message, nullptr, 0, 0)) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return 0;
}
