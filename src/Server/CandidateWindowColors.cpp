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

#include "CandidateWindowColors.h"

#include <windows.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/base.h>

#include <filesystem>
#include <string>

#include "Log.h"
#include "PathCompat.h"
#include "UTFHelper.h"

namespace McBopomofo {
namespace {

constexpr uint32_t kKeyKeyHighlightPurple = 0xB45DB7;

uint32_t ToRgb(const winrt::Windows::UI::Color& color) {
  return (static_cast<uint32_t>(color.R) << 16) |
         (static_cast<uint32_t>(color.G) << 8) | static_cast<uint32_t>(color.B);
}

bool EnsureWinrtApartmentInitialized() {
  static thread_local bool initialized = false;
  if (initialized) {
    return true;
  }

  try {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    initialized = true;
    return true;
  } catch (const winrt::hresult_error& e) {
    if (e.code() == RPC_E_CHANGED_MODE) {
      initialized = true;
      return true;
    }
    return false;
  }
}

bool AppsUseLightTheme() {
  DWORD useLightTheme = 1;
  DWORD size = sizeof(useLightTheme);
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                     reinterpret_cast<LPBYTE>(&useLightTheme), &size);
    RegCloseKey(hKey);
  }
  return useLightTheme != 0;
}

int ReadCustomColor(const wchar_t* key) {
  std::filesystem::path path(fcitx5_compat::userDirectory());
  path /= "mcbopomofo.ini";
  return GetPrivateProfileIntW(L"UI", key, -1, path.wstring().c_str());
}

uint32_t ContrastText(uint32_t rgb) {
  const uint32_t r = (rgb >> 16) & 0xFF;
  const uint32_t g = (rgb >> 8) & 0xFF;
  const uint32_t b = rgb & 0xFF;
  return (r * 299 + g * 587 + b * 114) >= 128000 ? 0x101010 : 0xFFFFFF;
}

}  // namespace

IPC::CandidateWindowColors ReadCandidateWindowColors() {
  IPC::CandidateWindowColors colors;
  colors.text = 0xFFFFFF;
  colors.background = 0x000000;
  colors.border = 0x505050;
  colors.highlightBackground = kKeyKeyHighlightPurple;

  const int customHighlight = ReadCustomColor(L"CandidateHighlightColor");
  const int customBackground = ReadCustomColor(L"CandidateBackgroundColor");
  const int customText = ReadCustomColor(L"CandidateTextColor");
  if (customHighlight >= 0) {
    colors.highlightBackground = static_cast<uint32_t>(customHighlight);
  }
  if (customBackground >= 0) {
    colors.background = static_cast<uint32_t>(customBackground);
  }
  if (customText >= 0) {
    colors.text = static_cast<uint32_t>(customText);
  }
  colors.highlightText = ContrastText(colors.highlightBackground);
  colors.border = colors.background == 0x000000 ? 0x505050 : colors.border;

  return colors;
}

}  // namespace McBopomofo
