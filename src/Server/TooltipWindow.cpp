// Copyright (c) 2026 and onwards The McBopomofo Authors.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "TooltipWindow.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>

#include "Globals.h"
#include "UTFHelper.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

const wchar_t* const TOOLTIP_WINDOW_CLASS = L"WinMcBopomofoTooltipWindow";

namespace {

bool IsEmojiCodePoint(char32_t cp) {
  return (cp >= 0x1F300 && cp <= 0x1FAFF) || (cp >= 0x2600 && cp <= 0x27BF) ||
         (cp >= 0xFE00 && cp <= 0xFE0F);
}

HFONT CreateUiFont(const wchar_t* faceName, LONG height) {
  LOGFONTW lf = {};
  lf.lfHeight = height;
  lf.lfWeight = FW_NORMAL;
  lf.lfQuality = CLEARTYPE_QUALITY;
  wcscpy_s(lf.lfFaceName, faceName);
  return CreateFontIndirectW(&lf);
}

int DrawOrMeasureTooltipRun(HDC hdc, std::wstring_view text, int x, int y,
                            HFONT textFont, HFONT emojiFont, bool draw) {
  int cursorX = x;
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(0, 0, 0));

  size_t i = 0;
  while (i < text.size()) {
    size_t start = i;
    char32_t cp = text[i];
    size_t cpLen = 1;
    if (i + 1 < text.size() && IS_HIGH_SURROGATE(text[i]) &&
        IS_LOW_SURROGATE(text[i + 1])) {
      cp = (((text[i] - 0xD800) << 10) | (text[i + 1] - 0xDC00)) + 0x10000;
      cpLen = 2;
    }
    const bool useEmoji = IsEmojiCodePoint(cp);
    i += cpLen;
    while (i < text.size()) {
      char32_t nextCp = text[i];
      size_t nextLen = 1;
      if (i + 1 < text.size() && IS_HIGH_SURROGATE(text[i]) &&
          IS_LOW_SURROGATE(text[i + 1])) {
        nextCp =
            (((text[i] - 0xD800) << 10) | (text[i + 1] - 0xDC00)) + 0x10000;
        nextLen = 2;
      }
      if (IsEmojiCodePoint(nextCp) != useEmoji) {
        break;
      }
      i += nextLen;
    }

    std::wstring_view run = text.substr(start, i - start);
    HFONT runFont = useEmoji ? emojiFont : textFont;
    HGDIOBJ oldFont = SelectObject(hdc, runFont);
    SIZE size = {};
    GetTextExtentPoint32W(hdc, run.data(), static_cast<int>(run.size()), &size);
    if (draw) {
      TextOutW(hdc, cursorX, y, run.data(), static_cast<int>(run.size()));
    }
    SelectObject(hdc, oldFont);
    cursorX += size.cx;
  }
  return cursorX - x;
}

size_t NextUtf16CodePointOffset(std::wstring_view text, size_t offset) {
  if (offset >= text.size()) return text.size();
  if (offset + 1 < text.size() && IS_HIGH_SURROGATE(text[offset]) &&
      IS_LOW_SURROGATE(text[offset + 1])) {
    return offset + 2;
  }
  return offset + 1;
}

size_t PreviousUtf16CodePointOffset(std::wstring_view text, size_t offset) {
  if (offset == 0) return 0;
  size_t previous = std::min(offset, text.size()) - 1;
  if (previous > 0 && IS_LOW_SURROGATE(text[previous]) &&
      IS_HIGH_SURROGATE(text[previous - 1])) {
    --previous;
  }
  return previous;
}

}  // namespace

TooltipWindow::TooltipWindow()
    : hwnd_(nullptr),
      ownerHwnd_(nullptr),
      hInstance_(nullptr),
      renderMode_(RenderMode::kD2D),
      dpiScale_(1.0f),
      cursorUtf16Offset_(0),
      underlineUtf16Start_(0),
      underlineUtf16End_(0),
      keyKeyPreeditStyle_(false),
      pD2DFactory_(nullptr),
      pRenderTarget_(nullptr),
      pDWriteFactory_(nullptr),
      pTextFormat_(nullptr),
      pTextLayout_(nullptr),
      pTextBrush_(nullptr),
      pBgBrush_(nullptr),
      pBorderBrush_(nullptr) {
  createDeviceIndependentResources_();
}

TooltipWindow::~TooltipWindow() {
  Destroy();
  discardDeviceResources_();
  if (pTextLayout_) {
    pTextLayout_->Release();
  }
  if (pTextFormat_) {
    pTextFormat_->Release();
  }
  if (pDWriteFactory_) {
    pDWriteFactory_->Release();
  }
  if (pD2DFactory_) {
    pD2DFactory_->Release();
  }
}

void TooltipWindow::createDeviceIndependentResources_() {
  if (!pD2DFactory_) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory_);
  }
  if (!pDWriteFactory_) {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&pDWriteFactory_));
  }
  if (pDWriteFactory_ && !pTextFormat_) {
    pDWriteFactory_->CreateTextFormat(
        L"Microsoft JhengHei UI",  // Good UI font for Traditional Chinese
        NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        keyKeyPreeditStyle_ ? 20.0f : 15.0f,
        L"zh-TW", &pTextFormat_);
  }
}

void TooltipWindow::createDeviceResources_() {
  if (!pRenderTarget_ && hwnd_) {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    pD2DFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                     D2D1::PixelFormat(), 96.0f, 96.0f),
        D2D1::HwndRenderTargetProperties(hwnd_, size), &pRenderTarget_);

    if (pRenderTarget_) {
      pRenderTarget_->CreateSolidColorBrush(
          D2D1::ColorF(0x000000),
          &pTextBrush_);
      pRenderTarget_->CreateSolidColorBrush(
          D2D1::ColorF(keyKeyPreeditStyle_ ? 0xF4F4F4 : 0xFFFFE0),
          &pBgBrush_);
      pRenderTarget_->CreateSolidColorBrush(
          D2D1::ColorF(keyKeyPreeditStyle_ ? 0x707070 : 0x000000),
          &pBorderBrush_);
    }
  }
}

void TooltipWindow::SetKeyKeyPreeditStyle(bool enabled) {
  if (keyKeyPreeditStyle_ == enabled) return;
  keyKeyPreeditStyle_ = enabled;
  if (pTextLayout_) {
    pTextLayout_->Release();
    pTextLayout_ = nullptr;
  }
  if (pTextFormat_) {
    pTextFormat_->Release();
    pTextFormat_ = nullptr;
  }
  discardDeviceResources_();
  createDeviceIndependentResources_();
}

void TooltipWindow::discardDeviceResources_() {
  if (pRenderTarget_) {
    pRenderTarget_->Release();
    pRenderTarget_ = nullptr;
  }
  if (pTextBrush_) {
    pTextBrush_->Release();
    pTextBrush_ = nullptr;
  }
  if (pBgBrush_) {
    pBgBrush_->Release();
    pBgBrush_ = nullptr;
  }
  if (pBorderBrush_) {
    pBorderBrush_->Release();
    pBorderBrush_ = nullptr;
  }
}

bool TooltipWindow::Create(HINSTANCE hInstance) {
  if (hInstance) {
    hInstance_ = hInstance;
  }
  if (hwnd_) return true;
  if (!hInstance_) return false;

  WNDCLASSEXW wcex = {0};
  wcex.cbSize = sizeof(WNDCLASSEXW);
  wcex.style = CS_IME;
  wcex.lpfnWndProc = wndProc_;
  wcex.hInstance = hInstance_;
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = NULL;  // Handled by D2D
  wcex.lpszClassName = TOOLTIP_WINDOW_CLASS;

  RegisterClassExW(
      &wcex);  // Ignore failure as it might be registered by another instance

  hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                          TOOLTIP_WINDOW_CLASS, L"", WS_POPUP | WS_CLIPCHILDREN,
                          0, 0, 100, 30,  // Initial dummy size
#ifdef WINMCBOPOMOFO_SERVER_SIDE_POPUP
                          nullptr,
#else
                          ownerHwnd_,
#endif
                          nullptr, hInstance_, this);

  EnableWindowDropShadow(hwnd_);

  return hwnd_ != nullptr;
}

bool TooltipWindow::recreateWindow_() {
  if (!hInstance_) {
    return false;
  }

  RECT rc = {0};
  if (hwnd_) {
    GetWindowRect(hwnd_, &rc);
    Destroy();
  }

  if (!Create(hInstance_)) {
    // LogMessage("TooltipWindow recreate failed owner=%p", ownerHwnd_);
    return false;
  }

  if (rc.right > rc.left && rc.bottom > rc.top) {
    SetWindowPos(hwnd_, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left,
                 rc.bottom - rc.top, SWP_NOACTIVATE);
  }

  // LogMessage("TooltipWindow recreated hwnd=%p owner=%p", hwnd_,
  //            ownerHwnd_);
  return true;
}

void TooltipWindow::updateRenderMode_() { renderMode_ = RenderMode::kD2D; }

void TooltipWindow::SetOwnerWindow(HWND ownerHwnd) {
  if (ownerHwnd && !IsWindow(ownerHwnd)) {
    ownerHwnd = nullptr;
  }
  if (ownerHwnd == hwnd_) {
    ownerHwnd = nullptr;
  }
  if (ownerHwnd_ == ownerHwnd) {
    return;
  }

#ifndef WINMCBOPOMOFO_SERVER_SIDE_POPUP
  const HWND previousOwner = ownerHwnd_;
#endif
  ownerHwnd_ = ownerHwnd;
  updateRenderMode_();
#ifdef WINMCBOPOMOFO_SERVER_SIDE_POPUP
  // Server-side popups are owned by McBopomofoServer.exe. ownerHwnd_ is kept
  // only for compatibility with the shared owner-update path.
#else
  if (!hwnd_) {
    return;
  }

  if (previousOwner != ownerHwnd_ && recreateWindow_()) {
    return;
  }

  SetWindowLongPtrW(hwnd_, GWLP_HWNDPARENT,
                    reinterpret_cast<LONG_PTR>(ownerHwnd_));
  // LogMessage("TooltipWindow owner updated hwnd=%p owner=%p", hwnd_,
  // ownerHwnd_);
#endif
}

void TooltipWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

void TooltipWindow::UpdateUI(const std::string& tooltipText,
                             size_t utf8CursorIndex, int utf8UnderlineStart,
                             int utf8UnderlineEnd) {
  if (!hwnd_) return;

  if (tooltipText.empty()) {
    // LogMessage("TooltipWindow UpdateUI empty text -> hide");
    Hide();
    return;
  }

  // LogMessage("TooltipWindow UpdateUI hwnd=%p owner=%p textLen=%llu", hwnd_,
  //            ownerHwnd_, static_cast<unsigned long
  //            long>(tooltipText.size()));
  dpiScale_ = GetDpiScaleForWindow(hwnd_);
  displayString_ = McBopomofo::Utf8ToUtf16(tooltipText);
  cursorUtf16Offset_ =
      utf8CursorIndex == std::string::npos
          ? displayString_.size()
          : std::min(displayString_.size(),
                     McBopomofo::Utf8OffsetToUtf16Offset(tooltipText,
                                                        utf8CursorIndex));
  underlineUtf16Start_ = 0;
  underlineUtf16End_ = 0;
  if (utf8UnderlineStart >= 0 &&
      utf8UnderlineEnd > utf8UnderlineStart) {
    underlineUtf16Start_ = McBopomofo::Utf8OffsetToUtf16Offset(
        tooltipText, static_cast<size_t>(utf8UnderlineStart));
    underlineUtf16End_ = McBopomofo::Utf8OffsetToUtf16Offset(
        tooltipText, static_cast<size_t>(utf8UnderlineEnd));
    underlineUtf16Start_ =
        std::min(underlineUtf16Start_, displayString_.size());
    underlineUtf16End_ = std::min(underlineUtf16End_, displayString_.size());
  }
  rebuildLayoutAndResize_();
}

void TooltipWindow::rebuildLayoutAndResize_() {
  if (displayString_.empty()) {
    return;
  }

  if (pTextLayout_) {
    pTextLayout_->Release();
    pTextLayout_ = nullptr;
  }

  if (pDWriteFactory_ && pTextFormat_) {
    pDWriteFactory_->CreateTextLayout(
        displayString_.c_str(), (UINT32)displayString_.length(), pTextFormat_,
        10000.0f, 10000.0f, &pTextLayout_);
  }

  int width = 0;
  int height = 0;
  if (renderMode_ == RenderMode::kGDI) {
    HDC screenDc = GetDC(nullptr);
    if (screenDc) {
      const float fontSize = keyKeyPreeditStyle_ ? 20.0f : 15.0f;
      const LONG textHeight = -std::max(
          11L, static_cast<LONG>(std::lround(fontSize * dpiScale_)));
      HFONT textFont = CreateUiFont(L"Microsoft JhengHei UI", textHeight);
      HFONT emojiFont = CreateUiFont(L"Segoe UI Emoji", textHeight);
      HGDIOBJ oldFont = SelectObject(screenDc, textFont);
      TEXTMETRICW tm = {};
      GetTextMetricsW(screenDc, &tm);
      SelectObject(screenDc, oldFont);

      width = DrawOrMeasureTooltipRun(screenDc, displayString_, 0, 0, textFont,
                                      emojiFont, false) +
              static_cast<int>(std::lround(
                  (keyKeyPreeditStyle_ ? 20.0f : 16.0f) * dpiScale_));
      height = (tm.tmHeight + tm.tmExternalLeading) +
               static_cast<int>(std::lround(
                   (keyKeyPreeditStyle_ ? 10.0f : 8.0f) * dpiScale_));

      DeleteObject(textFont);
      DeleteObject(emojiFont);
      ReleaseDC(nullptr, screenDc);
    }
  }

  if (width == 0 || height == 0) {
    float textWidth = 0, textHeight = 0;
    if (pTextLayout_) {
      DWRITE_TEXT_METRICS metrics;
      pTextLayout_->GetMetrics(&metrics);
      textWidth = metrics.width;
      textHeight = metrics.height;
    }

    width = (int)std::ceil(textWidth * dpiScale_) +
            (int)((keyKeyPreeditStyle_ ? 20 : 16) * dpiScale_);
    height = (int)std::ceil(textHeight * dpiScale_) +
             (int)((keyKeyPreeditStyle_ ? 10 : 8) * dpiScale_);
  }

  // Enforce a minimum size
  width = std::max(width, (int)(20 * dpiScale_));
  height = std::max(height, (int)(20 * dpiScale_));

  // LogMessage("TooltipWindow layout hwnd=%p width=%d height=%d dpi=%.3f",
  // hwnd_,
  //            width, height, dpiScale_);

  SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, width, height,
               SWP_NOMOVE | SWP_NOACTIVATE);
  if (pRenderTarget_) {
    pRenderTarget_->Resize(D2D1::SizeU(width, height));
  }
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void TooltipWindow::Move(int x, int y) {
  if (hwnd_) {
    // LogMessage("TooltipWindow Move hwnd=%p owner=%p x=%d y=%d", hwnd_,
    //            ownerHwnd_, x, y);
    const float oldScale = dpiScale_;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    dpiScale_ = GetDpiScaleForWindow(hwnd_);
    if (std::abs(dpiScale_ - oldScale) > 0.001f) {
      rebuildLayoutAndResize_();
    } else {
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
  }
}

void TooltipWindow::Hide() {
  if (hwnd_) {
    // LogMessage("TooltipWindow Hide hwnd=%p owner=%p", hwnd_, ownerHwnd_);
    ShowWindow(hwnd_, SW_HIDE);
  }
}

LRESULT CALLBACK TooltipWindow::wndProc_(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) {
  TooltipWindow* pThis = nullptr;

  if (uMsg == WM_NCCREATE) {
    CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
    pThis = (TooltipWindow*)pCreate->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
  } else {
    pThis = (TooltipWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (pThis) {
    if (uMsg == WM_PAINT) {
      return pThis->onPaint_(hwnd);
    } else if (uMsg == WM_DPICHANGED) {
      pThis->dpiScale_ = (float)LOWORD(wParam) / 96.0f;
      RECT* prcNewWindow = (RECT*)lParam;
      SetWindowPos(hwnd, NULL, prcNewWindow->left, prcNewWindow->top,
                   prcNewWindow->right - prcNewWindow->left,
                   prcNewWindow->bottom - prcNewWindow->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      pThis->rebuildLayoutAndResize_();
      return 0;
    } else if (uMsg == WM_DISPLAYCHANGE) {
      ::InvalidateRect(hwnd, nullptr, FALSE);
    }
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT TooltipWindow::onPaint_(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);

  RenderMode activeMode = renderMode_;
  if (activeMode == RenderMode::kD2D) {
    createDeviceResources_();
    // Fall back to GDI rendering if Direct2D initialization fails.
    // This is crucial in sandboxed processes (like Microsoft Edge or Chrome)
    // where GPU/Direct3D device recreation can be blocked after device loss.
    if (!pRenderTarget_) {
      renderMode_ = RenderMode::kGDI;
      activeMode = RenderMode::kGDI;
    }
  }

  if (activeMode == RenderMode::kGDI) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(keyKeyPreeditStyle_
                                         ? RGB(244, 244, 244)
                                         : RGB(255, 255, 224));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(
        PS_SOLID, 1,
        keyKeyPreeditStyle_ ? RGB(112, 112, 112) : RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    const float fontSize = keyKeyPreeditStyle_ ? 20.0f : 15.0f;
    const LONG textHeight = -std::max(
        11L, static_cast<LONG>(std::lround(fontSize * dpiScale_)));
    HFONT textFont = CreateUiFont(L"Microsoft JhengHei UI", textHeight);
    HFONT emojiFont = CreateUiFont(L"Segoe UI Emoji", textHeight);
    DrawOrMeasureTooltipRun(hdc, displayString_,
                            static_cast<int>(std::lround(
                                (keyKeyPreeditStyle_ ? 10.0f : 8.0f) *
                                dpiScale_)),
                            static_cast<int>(std::lround(
                                (keyKeyPreeditStyle_ ? 5.0f : 4.0f) *
                                dpiScale_)),
                            textFont, emojiFont, true);
    const bool hasPhraseUnderline =
        underlineUtf16End_ > underlineUtf16Start_;
    if (keyKeyPreeditStyle_ &&
        (hasPhraseUnderline || cursorUtf16Offset_ < displayString_.size())) {
      const int textX = static_cast<int>(std::lround(10.0f * dpiScale_));
      const size_t underlineStartOffset =
          hasPhraseUnderline ? underlineUtf16Start_ : cursorUtf16Offset_;
      const size_t underlineEndOffset =
          hasPhraseUnderline
              ? underlineUtf16End_
              : NextUtf16CodePointOffset(displayString_, cursorUtf16Offset_);
      const int underlineStart =
          textX + DrawOrMeasureTooltipRun(
                      hdc,
                      std::wstring_view(displayString_).substr(
                          0, underlineStartOffset),
                      0, 0, textFont, emojiFont, false);
      const int underlineWidth = DrawOrMeasureTooltipRun(
          hdc,
          std::wstring_view(displayString_).substr(
              underlineStartOffset,
              underlineEndOffset - underlineStartOffset),
          0, 0, textFont, emojiFont, false);
      HPEN underlinePen =
          CreatePen(PS_SOLID, std::max(2, (int)(2 * dpiScale_)),
                    RGB(180, 65, 183));
      HGDIOBJ oldUnderlinePen = SelectObject(hdc, underlinePen);
      const int underlineY = rc.bottom - std::max(3, (int)(4 * dpiScale_));
      MoveToEx(hdc, underlineStart, underlineY, nullptr);
      LineTo(hdc, underlineStart + std::max(underlineWidth, 2), underlineY);
      SelectObject(hdc, oldUnderlinePen);
      DeleteObject(underlinePen);
    } else if (keyKeyPreeditStyle_) {
      HPEN caretPen = CreatePen(PS_SOLID, std::max(2, (int)(3 * dpiScale_)),
                                RGB(180, 65, 183));
      HGDIOBJ oldCaretPen = SelectObject(hdc, caretPen);
      MoveToEx(hdc, rc.right - std::max(3, (int)(4 * dpiScale_)),
               std::max(2, (int)(3 * dpiScale_)), nullptr);
      LineTo(hdc, rc.right - std::max(3, (int)(4 * dpiScale_)),
             rc.bottom - std::max(2, (int)(3 * dpiScale_)));
      SelectObject(hdc, oldCaretPen);
      DeleteObject(caretPen);
    }
    DeleteObject(textFont);
    DeleteObject(emojiFont);
    EndPaint(hwnd, &ps);
    return 0;
  }

  createDeviceResources_();
  if (pRenderTarget_) {
    pRenderTarget_->BeginDraw();
    pRenderTarget_->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale_, dpiScale_));
    if (pBgBrush_) {
      pRenderTarget_->Clear(pBgBrush_->GetColor());
    } else {
      pRenderTarget_->Clear(D2D1::ColorF(D2D1::ColorF::White));
    }

    if (pTextLayout_ && pTextBrush_) {
      pRenderTarget_->DrawTextLayout(D2D1::Point2F(8.0f, 4.0f), pTextLayout_,
                                     pTextBrush_,
                                     D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }

    const bool hasPhraseUnderline =
        underlineUtf16End_ > underlineUtf16Start_;
    if (keyKeyPreeditStyle_ && pTextLayout_ &&
        (hasPhraseUnderline || cursorUtf16Offset_ < displayString_.size())) {
      const size_t underlineStartOffset =
          hasPhraseUnderline ? underlineUtf16Start_ : cursorUtf16Offset_;
      const size_t underlineEndOffset =
          hasPhraseUnderline
              ? underlineUtf16End_
              : NextUtf16CodePointOffset(displayString_, cursorUtf16Offset_);
      const size_t lastCodePointOffset =
          PreviousUtf16CodePointOffset(displayString_, underlineEndOffset);
      DWRITE_HIT_TEST_METRICS startHit = {};
      DWRITE_HIT_TEST_METRICS endHit = {};
      FLOAT startX = 0.0f;
      FLOAT startY = 0.0f;
      FLOAT endX = 0.0f;
      FLOAT endY = 0.0f;
      if (SUCCEEDED(pTextLayout_->HitTestTextPosition(
              static_cast<UINT32>(underlineStartOffset), FALSE, &startX,
              &startY, &startHit)) &&
          SUCCEEDED(pTextLayout_->HitTestTextPosition(
              static_cast<UINT32>(lastCodePointOffset), TRUE, &endX, &endY,
              &endHit))) {
        ID2D1SolidColorBrush* underlineBrush = nullptr;
        if (SUCCEEDED(pRenderTarget_->CreateSolidColorBrush(
                D2D1::ColorF(0xB441B7), &underlineBrush))) {
          const float y = 4.0f + startHit.top + startHit.height + 1.0f;
          const float width = std::max(endX - startX, 2.0f);
          pRenderTarget_->DrawLine(
              D2D1::Point2F(8.0f + startX, y),
              D2D1::Point2F(8.0f + startX + width, y),
              underlineBrush, 2.0f);
          underlineBrush->Release();
        }
      }
    }

    // Draw border
    D2D1_SIZE_F size = pRenderTarget_->GetSize();
    pRenderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    if (pBorderBrush_) {
      pRenderTarget_->DrawRectangle(
          D2D1::RectF(0.5f, 0.5f, size.width - 0.5f, size.height - 0.5f),
          pBorderBrush_, 1.0f);
    }

    if (keyKeyPreeditStyle_ && !hasPhraseUnderline &&
        cursorUtf16Offset_ >= displayString_.size()) {
      ID2D1SolidColorBrush* caretBrush = nullptr;
      if (SUCCEEDED(pRenderTarget_->CreateSolidColorBrush(
              D2D1::ColorF(0xB441B7), &caretBrush))) {
        pRenderTarget_->FillRectangle(
            D2D1::RectF(size.width - 4.0f, 3.0f, size.width - 1.0f,
                        size.height - 3.0f),
            caretBrush);
        caretBrush->Release();
      }
    }

    HRESULT hr = pRenderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
      discardDeviceResources_();
      InvalidateRect(hwnd, nullptr, FALSE);
    }
  }

  EndPaint(hwnd, &ps);
  return 0;
}
