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

#include "McBopomofoTIP.h"

#include <filesystem>
#include <string>

#include "EditSession.h"
#include "ConversionHotkey.h"
#include "Globals.h"
#include "KeyKeyPunctuation.h"
#include "LangBarButton.h"
#include "NamedPipe.h"
#include "PathCompat.h"
#include "StateEditSession.h"
#include "UTFHelper.h"

namespace {

bool ReadDWORDCompartmentValue(ITfThreadMgr* threadMgr, REFGUID compartmentGuid,
                               DWORD* value) {
  if (!threadMgr || !value) {
    return false;
  }

  ITfCompartmentMgr* pCompMgr = nullptr;
  HRESULT hr =
      threadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr);
  if (FAILED(hr)) {
    return false;
  }

  ITfCompartment* pComp = nullptr;
  hr = pCompMgr->GetCompartment(compartmentGuid, &pComp);
  pCompMgr->Release();
  if (FAILED(hr)) {
    return false;
  }

  VARIANT var;
  VariantInit(&var);
  hr = pComp->GetValue(&var);
  pComp->Release();
  if (FAILED(hr) || var.vt != VT_I4) {
    VariantClear(&var);
    return false;
  }

  *value = static_cast<DWORD>(var.lVal);
  VariantClear(&var);
  return true;
}

bool GetPrintableCharacter(WPARAM wParam, LPARAM lParam,
                           const BYTE keyboardState[256], wchar_t* output) {
  const bool ctrlPressed =
      (keyboardState[VK_CONTROL] & 0x80) != 0 ||
      (keyboardState[VK_LCONTROL] & 0x80) != 0 ||
      (keyboardState[VK_RCONTROL] & 0x80) != 0;
  const bool altPressed =
      (keyboardState[VK_MENU] & 0x80) != 0 ||
      (keyboardState[VK_LMENU] & 0x80) != 0 ||
      (keyboardState[VK_RMENU] & 0x80) != 0;
  if (output == nullptr || ctrlPressed || altPressed) {
    return false;
  }
  WCHAR chars[2] = {0};
  if (ToUnicode(static_cast<UINT>(wParam), (lParam >> 16) & 0xFF,
                keyboardState, chars, 2, 0) != 1) {
    return false;
  }
  if (chars[0] < 0x20 || chars[0] > 0x7e) {
    return false;
  }
  *output = chars[0];
  return true;
}

std::wstring ToFullWidth(wchar_t ascii) {
  if (ascii == L' ') {
    return std::wstring(1, static_cast<wchar_t>(0x3000));
  }
  return std::wstring(1, static_cast<wchar_t>(ascii - 0x21 + 0xff01));
}

bool AdviseCompartmentSink(ITfThreadMgr* threadMgr, REFGUID compartmentGuid,
                           ITfCompartmentEventSink* sink, DWORD* cookie) {
  if (!threadMgr || !sink || !cookie) {
    return false;
  }

  ITfCompartmentMgr* pCompMgr = nullptr;
  HRESULT hr =
      threadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr);
  if (FAILED(hr)) {
    return false;
  }

  ITfCompartment* pCompartment = nullptr;
  hr = pCompMgr->GetCompartment(compartmentGuid, &pCompartment);
  pCompMgr->Release();
  if (FAILED(hr)) {
    return false;
  }

  ITfSource* pSource = nullptr;
  hr = pCompartment->QueryInterface(IID_ITfSource, (void**)&pSource);
  if (SUCCEEDED(hr)) {
    hr = pSource->AdviseSink(IID_ITfCompartmentEventSink, sink, cookie);
    pSource->Release();
  }
  pCompartment->Release();
  return SUCCEEDED(hr);
}

void UnadviseCompartmentSink(ITfThreadMgr* threadMgr, REFGUID compartmentGuid,
                             DWORD* cookie) {
  if (!threadMgr || !cookie || *cookie == TF_INVALID_COOKIE) {
    return;
  }

  ITfCompartmentMgr* pCompMgr = nullptr;
  HRESULT hr =
      threadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr);
  if (SUCCEEDED(hr)) {
    ITfCompartment* pCompartment = nullptr;
    hr = pCompMgr->GetCompartment(compartmentGuid, &pCompartment);
    pCompMgr->Release();
    if (SUCCEEDED(hr)) {
      ITfSource* pSource = nullptr;
      hr = pCompartment->QueryInterface(IID_ITfSource, (void**)&pSource);
      if (SUCCEEDED(hr)) {
        pSource->UnadviseSink(*cookie);
        pSource->Release();
      }
      pCompartment->Release();
    }
  }

  *cookie = TF_INVALID_COOKIE;
}

bool IsVirtualKeyDown(int vk) {
  return (GetKeyState(vk) & 0x8000) != 0 ||
         (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool IsCtrlPressed(const BYTE keyboardState[256]) {
  return (keyboardState[VK_CONTROL] & 0x80) != 0 ||
         (keyboardState[VK_LCONTROL] & 0x80) != 0 ||
         (keyboardState[VK_RCONTROL] & 0x80) != 0 ||
         IsVirtualKeyDown(VK_CONTROL) || IsVirtualKeyDown(VK_LCONTROL) ||
         IsVirtualKeyDown(VK_RCONTROL);
}

bool IsShiftPressed(const BYTE keyboardState[256]) {
  return (keyboardState[VK_SHIFT] & 0x80) != 0 ||
         (keyboardState[VK_LSHIFT] & 0x80) != 0 ||
         (keyboardState[VK_RSHIFT] & 0x80) != 0 || IsVirtualKeyDown(VK_SHIFT) ||
         IsVirtualKeyDown(VK_LSHIFT) || IsVirtualKeyDown(VK_RSHIFT);
}

bool IsRightShiftPressed(const BYTE keyboardState[256]) {
  return (keyboardState[VK_RSHIFT] & 0x80) != 0 ||
         IsVirtualKeyDown(VK_RSHIFT);
}

char RightShiftPunctuationKey(WPARAM wParam, LPARAM lParam,
                              const BYTE keyboardState[256]) {
  const UINT scanCode = static_cast<UINT>((lParam >> 16) & 0xFF);
  return McBopomofo::KeyKeyRightShiftPunctuation(
      wParam, scanCode, IsRightShiftPressed(keyboardState));
}

bool IsServerHandledCtrlShortcutKey(WPARAM wParam) {
  switch (wParam) {
    case VK_OEM_COMMA:
    case VK_OEM_PERIOD:
    case '1':
    case VK_OEM_2:
    case VK_OEM_1:
    case VK_OEM_7:
    case VK_OEM_5:
      return true;
    default:
      return false;
  }
}

bool IsServerHandledShortcutKey(WPARAM wParam, const BYTE keyboardState[256]) {
  const bool ctrlPressed = IsCtrlPressed(keyboardState);
  return ctrlPressed && IsServerHandledCtrlShortcutKey(wParam);
}

bool IsStandaloneModifierKey(WPARAM wParam) {
  switch (wParam) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
      return true;
    default:
      return false;
  }
}

bool IsHostEditingKey(WPARAM wParam) {
  switch (wParam) {
    case VK_RETURN:
    case VK_ESCAPE:
    case VK_TAB:
    case VK_BACK:
    case VK_DELETE:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
      return true;
    default:
      return false;
  }
}

bool IsShiftKey(WPARAM wParam) {
  switch (wParam) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
      return true;
    default:
      return false;
  }
}

bool IsAltPressed(const BYTE keyboardState[256]);

bool IsConversionToggleHotkey(WPARAM wParam,
                              const BYTE keyboardState[256]) {
  std::filesystem::path path(McBopomofo::fcitx5_compat::userDirectory());
  path /= "mcbopomofo.ini";
  const std::wstring iniPath = path.wstring();
  if (GetPrivateProfileIntW(L"General", L"ConversionHotkeyEnabled", 1,
                            iniPath.c_str()) == 0) {
    return false;
  }
  const int modifiers =
      GetPrivateProfileIntW(L"General", L"ConversionHotkeyModifiers", 1,
                            iniPath.c_str());
  const int configuredKey =
      GetPrivateProfileIntW(L"General", L"ConversionHotkeyKey", VK_F3,
                            iniPath.c_str());
  const bool ctrl = IsCtrlPressed(keyboardState);
  const bool shift = IsShiftPressed(keyboardState);
  const bool alt = IsAltPressed(keyboardState);
  return McBopomofo::MatchesConversionHotkey(configuredKey, modifiers, wParam,
                                              ctrl, shift, alt);
}

bool IsLeftShiftKeyEvent(WPARAM wParam, LPARAM lParam) {
  if (wParam == VK_LSHIFT) {
    return true;
  }
  if (wParam != VK_SHIFT) {
    return false;
  }
  const UINT scanCode = static_cast<UINT>((lParam >> 16) & 0xff);
  return scanCode == 0x2a;
}

bool IsAltPressed(const BYTE keyboardState[256]) {
  return (keyboardState[VK_MENU] & 0x80) != 0 ||
         (keyboardState[VK_LMENU] & 0x80) != 0 ||
         (keyboardState[VK_RMENU] & 0x80) != 0 || IsVirtualKeyDown(VK_MENU) ||
         IsVirtualKeyDown(VK_LMENU) || IsVirtualKeyDown(VK_RMENU);
}

bool IsOnlyShiftKeyEvent(WPARAM wParam, const BYTE keyboardState[256]) {
  return IsShiftKey(wParam) && !IsCtrlPressed(keyboardState) &&
         !IsAltPressed(keyboardState);
}

bool GetFocusedContext(ITfThreadMgr* threadMgr, ITfContext** context) {
  if (!threadMgr || !context) {
    return false;
  }

  *context = nullptr;

  ITfDocumentMgr* pDocMgr = nullptr;
  if (FAILED(threadMgr->GetFocus(&pDocMgr)) || !pDocMgr) {
    return false;
  }

  HRESULT hr = pDocMgr->GetTop(context);
  pDocMgr->Release();
  return SUCCEEDED(hr) && *context != nullptr;
}

HWND GetContextWindow(ITfContext* context) {
  if (context) {
    ITfContextView* view = nullptr;
    if (SUCCEEDED(context->GetActiveView(&view)) && view) {
      HWND hwnd = nullptr;
      if (SUCCEEDED(view->GetWnd(&hwnd)) && hwnd) {
        view->Release();
        return hwnd;
      }
      view->Release();
    }
  }
  return GetFocus();
}

bool IsUsableLayoutRect(const RECT& rc) {
  return rc.bottom > rc.top &&
         (rc.left != 0 || rc.top != 0 || rc.right != 0 || rc.bottom != 0);
}

bool GetCaretFallbackRect(RECT* rect) {
  if (!rect) {
    return false;
  }

  GUITHREADINFO gti = {0};
  gti.cbSize = sizeof(GUITHREADINFO);
  if (!GetGUIThreadInfo(GetCurrentThreadId(), &gti) || !gti.hwndCaret) {
    return false;
  }

  RECT caretRect = gti.rcCaret;
  POINT topLeft = {caretRect.left, caretRect.top};
  POINT bottomRight = {caretRect.right, caretRect.bottom};
  ClientToScreen(gti.hwndCaret, &topLeft);
  ClientToScreen(gti.hwndCaret, &bottomRight);

  rect->left = topLeft.x;
  rect->top = topLeft.y;
  rect->right = bottomRight.x;
  rect->bottom = bottomRight.y;
  return IsUsableLayoutRect(*rect);
}

class CKeyLayoutEditSession : public CEditSessionBase {
 public:
  CKeyLayoutEditSession(ITfContext* context, RECT* rect, bool* hasRect)
      : CEditSessionBase(context), rect_(rect), hasRect_(hasRect) {}

  STDMETHODIMP DoEditSession(TfEditCookie ec) override {
    if (!rect_ || !hasRect_) {
      return E_INVALIDARG;
    }

    *hasRect_ = false;

    TF_SELECTION selection = {};
    ULONG fetched = 0;
    HRESULT hr = pContext_->GetSelection(ec, TF_DEFAULT_SELECTION, 1,
                                         &selection, &fetched);
    if (SUCCEEDED(hr) && fetched == 1 && selection.range) {
      ITfContextView* view = nullptr;
      if (SUCCEEDED(pContext_->GetActiveView(&view)) && view) {
        RECT rc = {0};
        BOOL clipped = FALSE;
        hr = view->GetTextExt(ec, selection.range, &rc, &clipped);
        if (SUCCEEDED(hr) && IsUsableLayoutRect(rc)) {
          *rect_ = rc;
          *hasRect_ = true;
        }
        view->Release();
      }
      selection.range->Release();
    }

    if (!*hasRect_) {
      *hasRect_ = GetCaretFallbackRect(rect_);
    }
    return S_OK;
  }

 private:
  RECT* rect_;
  bool* hasRect_;
};

bool GetKeyDownLayout(ITfContext* context, TfClientId clientId, RECT* rect) {
  if (!context || !rect) {
    return false;
  }

  bool hasRect = false;
  CKeyLayoutEditSession* session =
      new CKeyLayoutEditSession(context, rect, &hasRect);
  HRESULT editSessionResult = E_FAIL;
  HRESULT hr = context->RequestEditSession(
      clientId, session, TF_ES_SYNC | TF_ES_READ, &editSessionResult);
  session->Release();
  return SUCCEEDED(hr) && SUCCEEDED(editSessionResult) && hasRect;
}

std::string CurrentProcessNameUtf8() {
  wchar_t path[MAX_PATH];
  DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (length == 0) {
    return "";
  }

  std::wstring processPath(path, length);
  size_t slash = processPath.find_last_of(L"\\/");
  if (slash != std::wstring::npos) {
    processPath.erase(0, slash + 1);
  }
  return McBopomofo::Utf16ToUtf8(processPath);
}

}  // namespace

McBopomofoTIP::McBopomofoTIP()
    : cRef_(1),
      ptim_(nullptr),
      tid_(TF_CLIENTID_NULL),
      dwThreadMgrEventSinkCookie_(TF_INVALID_COOKIE),
      dwThreadFocusSinkCookie_(TF_INVALID_COOKIE),
      dwOpenCloseCompartmentEventSinkCookie_(TF_INVALID_COOKIE),
      pComposition_(nullptr),
      pModeIconButton_(nullptr),
      pSwitchLangButton_(nullptr),
      pFullHalfButton_(nullptr),
      pSymbolTableButton_(nullptr),
      pSettingsButton_(nullptr) {
  DllAddRef();
}

McBopomofoTIP::~McBopomofoTIP() { DllRelease(); }

STDAPI McBopomofoTIP::QueryInterface(REFIID riid, void** ppvObj) {
  if (ppvObj == nullptr) {
    return E_INVALIDARG;
  }

  *ppvObj = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor) ||
      IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
    *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
  } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
    *ppvObj = static_cast<ITfKeyEventSink*>(this);
  } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
    *ppvObj = static_cast<ITfCompositionSink*>(this);
  } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
    *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
  } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
    *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
  } else if (IsEqualIID(riid, IID_ITfThreadFocusSink)) {
    *ppvObj = static_cast<ITfThreadFocusSink*>(this);
  } else if (IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
    *ppvObj = static_cast<ITfCompartmentEventSink*>(this);
  }

  if (*ppvObj) {
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDAPI_(ULONG) McBopomofoTIP::AddRef() { return InterlockedIncrement(&cRef_); }

STDAPI_(ULONG) McBopomofoTIP::Release() {
  LONG cr = InterlockedDecrement(&cRef_);
  if (cr == 0) {
    delete this;
  }
  return cr;
}

BOOL McBopomofoTIP::initKeyEventSink_() {
  ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
  HRESULT hr =
      ptim_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
  if (FAILED(hr)) {
    return FALSE;
  }

  hr = pKeystrokeMgr->AdviseKeyEventSink(
      tid_, static_cast<ITfKeyEventSink*>(this), TRUE);
  pKeystrokeMgr->Release();
  return SUCCEEDED(hr);
}

void McBopomofoTIP::uninitKeyEventSink_() {
  if (!ptim_) {
    return;
  }

  ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
  HRESULT hr =
      ptim_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
  if (SUCCEEDED(hr)) {
    pKeystrokeMgr->UnadviseKeyEventSink(tid_);
    pKeystrokeMgr->Release();
  }
}

BOOL McBopomofoTIP::initCompartmentEventSink_() {
  return AdviseCompartmentSink(ptim_, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                               static_cast<ITfCompartmentEventSink*>(this),
                               &dwOpenCloseCompartmentEventSinkCookie_);
}

void McBopomofoTIP::uninitCompartmentEventSink_() {
  UnadviseCompartmentSink(ptim_, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                          &dwOpenCloseCompartmentEventSinkCookie_);
}

BOOL McBopomofoTIP::initThreadMgrEventSink_() {
  ITfSource* pSource = nullptr;
  HRESULT hr = ptim_->QueryInterface(IID_ITfSource, (void**)&pSource);
  if (FAILED(hr)) {
    return FALSE;
  }

  hr = pSource->AdviseSink(IID_ITfThreadMgrEventSink,
                           static_cast<ITfThreadMgrEventSink*>(this),
                           &dwThreadMgrEventSinkCookie_);
  pSource->Release();
  return SUCCEEDED(hr);
}

void McBopomofoTIP::uninitThreadMgrEventSink_() {
  if (dwThreadMgrEventSinkCookie_ == TF_INVALID_COOKIE || !ptim_) {
    return;
  }

  ITfSource* pSource = nullptr;
  if (SUCCEEDED(ptim_->QueryInterface(IID_ITfSource, (void**)&pSource))) {
    pSource->UnadviseSink(dwThreadMgrEventSinkCookie_);
    pSource->Release();
  }
  dwThreadMgrEventSinkCookie_ = TF_INVALID_COOKIE;
}

BOOL McBopomofoTIP::initThreadFocusSink_() {
  ITfSource* pSource = nullptr;
  HRESULT hr = ptim_->QueryInterface(IID_ITfSource, (void**)&pSource);
  if (FAILED(hr)) {
    return FALSE;
  }

  hr = pSource->AdviseSink(IID_ITfThreadFocusSink,
                           static_cast<ITfThreadFocusSink*>(this),
                           &dwThreadFocusSinkCookie_);
  pSource->Release();
  return SUCCEEDED(hr);
}

void McBopomofoTIP::uninitThreadFocusSink_() {
  if (dwThreadFocusSinkCookie_ == TF_INVALID_COOKIE || !ptim_) {
    return;
  }

  ITfSource* pSource = nullptr;
  if (SUCCEEDED(ptim_->QueryInterface(IID_ITfSource, (void**)&pSource))) {
    pSource->UnadviseSink(dwThreadFocusSinkCookie_);
    pSource->Release();
  }
  dwThreadFocusSinkCookie_ = TF_INVALID_COOKIE;
}

STDAPI McBopomofoTIP::Activate(ITfThreadMgr* ptim, TfClientId tid) {
  return ActivateEx(ptim, tid, 0);
}

STDAPI McBopomofoTIP::ActivateEx(ITfThreadMgr* ptim, TfClientId tid,
                                 DWORD dwFlags) {
  UNREFERENCED_PARAMETER(dwFlags);
  // LogMessage("McBopomofoTIP::ActivateEx called with flags: %u", dwFlags);

  if (ptim == nullptr) {
    return E_INVALIDARG;
  }

  updateProcessDisabledState_();

  ptim_ = ptim;
  ptim_->AddRef();
  tid_ = tid;

  if (!initKeyEventSink_()) {
    // LogMessage("Failed to init KeyEventSink");
    return E_FAIL;
  }

  if (!initCompartmentEventSink_()) {
    // LogMessage("Failed to init CompartmentEventSink");
    return E_FAIL;
  }
  if (!initThreadMgrEventSink_()) {
    // LogMessage("Failed to init ThreadMgrEventSink");
    return E_FAIL;
  }
  if (!initThreadFocusSink_()) {
    // LogMessage("Failed to init ThreadFocusSink");
    return E_FAIL;
  }

  // Register LangBar button - must be created BEFORE setting compartment value
  // because SetValue triggers OnChange callback synchronously
  ITfLangBarItemMgr* pLangBarItemMgr = nullptr;
  if (SUCCEEDED(ptim_->QueryInterface(IID_ITfLangBarItemMgr,
                                      (void**)&pLangBarItemMgr))) {
    pModeIconButton_ = new CLangBarButton(this, GUID_LBI_INPUTMODE,
                                          CLangBarButton::Kind::ImeModeMenu);
    pSwitchLangButton_ = new CLangBarButton(
        this, GUID_LBI_SWITCH_LANG, CLangBarButton::Kind::SwitchLanguageToggle);
    pFullHalfButton_ = new CLangBarButton(this, GUID_LBI_FULL_HALF,
                                          CLangBarButton::Kind::FullHalfToggle);
    pSymbolTableButton_ =
        new CLangBarButton(this, GUID_LBI_SYMBOL_TABLE,
                           CLangBarButton::Kind::SymbolTable);
    pSettingsButton_ = new CLangBarButton(this, GUID_LBI_SETTINGS,
                                          CLangBarButton::Kind::SettingsMenu);
    pLangBarItemMgr->AddItem(pModeIconButton_);
    pLangBarItemMgr->AddItem(pSwitchLangButton_);
    pLangBarItemMgr->AddItem(pFullHalfButton_);
    pLangBarItemMgr->AddItem(pSymbolTableButton_);
    pLangBarItemMgr->AddItem(pSettingsButton_);
    pLangBarItemMgr->Release();
  }

  // Preserve the mode supplied by Windows or the host application. Some
  // programs activate a new TIP instance for a dialog or another UI thread;
  // unconditionally forcing OPEN here would make the language bar jump back
  // to Chinese even when the user had switched that input context to English.
  DWORD existingOpenState = 0;
  if (!ReadDWORDCompartmentValue(ptim_, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                 &existingOpenState)) {
    ITfCompartmentMgr* pCompMgr = nullptr;
    if (SUCCEEDED(
            ptim_->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr))) {
      ITfCompartment* pComp = nullptr;
      if (SUCCEEDED(pCompMgr->GetCompartment(
              GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pComp))) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_I4;
        var.lVal = 1;
        pComp->SetValue(tid_, &var);
        VariantClear(&var);
        pComp->Release();
      }
      pCompMgr->Release();
    }
  }

  if (SUCCEEDED(ptim_->QueryInterface(IID_ITfUIElementMgr,
                                      (void**)&pUIElementMgr_))) {
    pCandidateUIElement_ = new CCandidateListUIElement(this);
    pReadingUIElement_ = new CReadingInformationUIElement(this);
    // LogMessage("ITfUIElementMgr successfully acquired.");
  } else {
    // LogMessage("Failed to acquire ITfUIElementMgr.");
  }

  // LogMessage("McBopomofoTIP::ActivateEx succeeded");
  return S_OK;
}

STDAPI McBopomofoTIP::Deactivate() {
  // LogMessage("McBopomofoTIP::Deactivate called");

  if (pUIElementMgr_) {
    if (pCandidateUIElement_) {
      if (dwCandidateUIElementId_ != 0) {
        pUIElementMgr_->EndUIElement(dwCandidateUIElementId_);
        dwCandidateUIElementId_ = 0;
      }
      pCandidateUIElement_->ClearTip();
      pCandidateUIElement_->Release();
      pCandidateUIElement_ = nullptr;
    }
    if (pReadingUIElement_) {
      if (dwReadingUIElementId_ != 0) {
        pUIElementMgr_->EndUIElement(dwReadingUIElementId_);
        dwReadingUIElementId_ = 0;
      }
      pReadingUIElement_->ClearTip();
      pReadingUIElement_->Release();
      pReadingUIElement_ = nullptr;
    }
    pUIElementMgr_->Release();
    pUIElementMgr_ = nullptr;
  }

  if (pModeIconButton_ || pSwitchLangButton_ || pFullHalfButton_ ||
      pSymbolTableButton_ ||
      pSettingsButton_) {
    ITfLangBarItemMgr* pLangBarItemMgr = nullptr;
    if (SUCCEEDED(ptim_->QueryInterface(IID_ITfLangBarItemMgr,
                                        (void**)&pLangBarItemMgr))) {
      if (pModeIconButton_) {
        pLangBarItemMgr->RemoveItem(pModeIconButton_);
      }
      if (pSwitchLangButton_) {
        pLangBarItemMgr->RemoveItem(pSwitchLangButton_);
      }
      if (pFullHalfButton_) {
        pLangBarItemMgr->RemoveItem(pFullHalfButton_);
      }
      if (pSymbolTableButton_) {
        pLangBarItemMgr->RemoveItem(pSymbolTableButton_);
      }
      if (pSettingsButton_) {
        pLangBarItemMgr->RemoveItem(pSettingsButton_);
      }
      pLangBarItemMgr->Release();
    }
    if (pModeIconButton_) {
      pModeIconButton_->Release();
      pModeIconButton_ = nullptr;
    }
    if (pSwitchLangButton_) {
      pSwitchLangButton_->Release();
      pSwitchLangButton_ = nullptr;
    }
    if (pFullHalfButton_) {
      pFullHalfButton_->Release();
      pFullHalfButton_ = nullptr;
    }
    if (pSymbolTableButton_) {
      pSymbolTableButton_->Release();
      pSymbolTableButton_ = nullptr;
    }
    if (pSettingsButton_) {
      pSettingsButton_->Release();
      pSettingsButton_ = nullptr;
    }
  }

  uninitCompartmentEventSink_();
  uninitThreadFocusSink_();
  uninitThreadMgrEventSink_();
  uninitKeyEventSink_();

  if (pComposition_) {
    pComposition_->Release();
    pComposition_ = nullptr;
  }

  if (ptim_) {
    ptim_->Release();
    ptim_ = nullptr;
  }
  tid_ = TF_CLIENTID_NULL;

  return S_OK;
}

STDAPI McBopomofoTIP::OnSetFocus(BOOL fForeground) {
  shiftToggleKeyPending_ = false;
  if (isProcessDisabled_()) {
    return S_OK;
  }
  if (!fForeground) {
    resetServerState_();
  }
  // Some applications create a separate TSF document/thread for dialogs
  // such as "Save As". The open/close compartment is already correct, but
  // Windows may keep displaying the previous language-bar icon unless every
  // focus transition explicitly republishes the item state.
  RefreshLangBar();
  return S_OK;
}

STDAPI McBopomofoTIP::OnTestKeyDown(ITfContext* pic, WPARAM wParam,
                                    LPARAM lParam, BOOL* pfEaten) {
  UNREFERENCED_PARAMETER(pic);
  UNREFERENCED_PARAMETER(lParam);
  if (pfEaten == nullptr) {
    return E_INVALIDARG;
  }
  if (isProcessDisabled_()) {
    *pfEaten = FALSE;
    return S_OK;
  }

  BYTE keyboardState[256];
  GetKeyboardState(keyboardState);

  if (IsLeftShiftKeyEvent(wParam, lParam)) {
    *pfEaten = TRUE;
    return S_OK;
  }

  // Any key pressed while Left Shift is held makes it a normal chord rather
  // than a standalone mode toggle.
  if (shiftToggleKeyPending_) {
    shiftToggleKeyPending_ = false;
  }

  if (wParam == VK_SPACE && IsShiftPressed(keyboardState)) {
    *pfEaten = TRUE;
    return S_OK;
  }

  if (IsConversionToggleHotkey(wParam, keyboardState)) {
    *pfEaten = TRUE;
    return S_OK;
  }

  if (!IsOpen()) {
    wchar_t printable = 0;
    *pfEaten = !IsHalfWidthOutputEnabled() &&
                       GetPrintableCharacter(wParam, lParam, keyboardState,
                                             &printable)
                   ? TRUE
                   : FALSE;
    return S_OK;
  }

  if (IsStandaloneModifierKey(wParam)) {
    *pfEaten = FALSE;
    return S_OK;
  }

  // If we have active composing buffer or candidates, let server handle all
  // keys
  if (!lastState_.composingBuffer.empty() || !lastState_.candidates.empty()) {
    *pfEaten = TRUE;
    return S_OK;
  }

  // If the composition buffer is empty, do not swallow control or editing
  // keys, allowing them to pass through to the host application safely.
  if (IsHostEditingKey(wParam)) {
    *pfEaten = FALSE;
    return S_OK;
  }

  if (IsServerHandledShortcutKey(wParam, keyboardState)) {
    *pfEaten = TRUE;
    return S_OK;
  }

  // Let the IME see ordinary keys even if they do not map to printable ASCII
  // through the current keyboard layout. Otherwise TSF can skip OnKeyDown and
  // the host application receives the raw key directly.
  if (IsCtrlPressed(keyboardState) || IsAltPressed(keyboardState)) {
    *pfEaten = FALSE;
    return S_OK;
  }

  *pfEaten = TRUE;

  return S_OK;
}

//
STDAPI McBopomofoTIP::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                                BOOL* pfEaten) {
  if (pfEaten == nullptr) {
    return E_INVALIDARG;
  }
  if (isProcessDisabled_()) {
    *pfEaten = FALSE;
    return S_OK;
  }

  BYTE keyboardState[256];
  GetKeyboardState(keyboardState);

  if (wParam == VK_SPACE && IsShiftPressed(keyboardState)) {
    ToggleHalfWidthPunctuationForTip(this);
    *pfEaten = TRUE;
    return S_OK;
  }

  if (IsConversionToggleHotkey(wParam, keyboardState)) {
    ToggleChineseConversionForTip(this);
    *pfEaten = TRUE;
    return S_OK;
  }

  if (handleStandaloneShiftKeyDown_(wParam, lParam, keyboardState)) {
    *pfEaten = FALSE;
    return S_OK;
  }

  if (!IsOpen()) {
    wchar_t printable = 0;
    if (!IsHalfWidthOutputEnabled() &&
        GetPrintableCharacter(wParam, lParam, keyboardState, &printable)) {
      McBopomofo::IPC::StateUpdatePayload state;
      state.consumed = true;
      state.commitString =
          McBopomofo::Utf16ToUtf8(ToFullWidth(printable));
      applyStateToContext_(pic, state, "Full-width English ");
      *pfEaten = TRUE;
      return S_OK;
    }
    *pfEaten = FALSE;
    return S_OK;
  }

  BOOL eaten = FALSE;
  OnTestKeyDown(pic, wParam, lParam, &eaten);

  if (!eaten) {
    *pfEaten = FALSE;
    return S_OK;
  }

  McBopomofo::IPC::KeyEventPayload req;
  HWND contextHwnd = GetContextWindow(pic);
  const char rightShiftPunctuation =
      RightShiftPunctuationKey(wParam, lParam, keyboardState);
  req.vk = (unsigned int)wParam;
  // KeyKey-compatible right-Shift punctuation is routed through the engine's
  // punctuation channel so it participates in the active composition.
  req.shift =
      rightShiftPunctuation == '\0' && IsShiftPressed(keyboardState);
  req.ctrl =
      rightShiftPunctuation != '\0' || IsCtrlPressed(keyboardState);
  RECT keyLayout = {0};
  if (GetKeyDownLayout(pic, tid_, &keyLayout)) {
    req.hasCoords = true;
    req.ownerHwnd =
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(contextHwnd));
    req.anchorLeft = static_cast<int>(keyLayout.left);
    req.anchorTop = static_cast<int>(keyLayout.top);
    req.anchorRight = static_cast<int>(keyLayout.right);
    req.anchorBottom = static_cast<int>(keyLayout.bottom);
  }

  GetKeyboardState(keyboardState);
  WCHAR chars[2] = {0};
  if (ToUnicode((UINT)wParam, (lParam >> 16) & 0xFF, keyboardState, chars, 2,
                0) == 1) {
    req.ascii = (chars[0] >= 32 && chars[0] <= 126) ? chars[0] : 0;
  } else {
    req.ascii = 0;
  }
  if (rightShiftPunctuation != '\0') {
    req.ascii = static_cast<unsigned int>(rightShiftPunctuation);
  }

  McBopomofo::IPC::NamedPipeClient pipe(McBopomofo::IPC::PIPE_NAME);
  std::string response;

  std::string payload = McBopomofo::IPC::SerializeKeyEvent(req);
  // LogMessage("Sending IPC request: %s", payload.c_str());

  if (pipe.Call(payload, response)) {
    // LogMessage("Received IPC response: %s", response.c_str());
    if (McBopomofo::IPC::DeserializeStateUpdate(response, lastState_)) {
      const bool hasCommit = !lastState_.commitString.empty();
      *pfEaten = (lastState_.consumed || hasCommit) ? TRUE : FALSE;
      // LogMessage(
      //     "State deserialized. Consumed: %d, CommitStr: '%s', CompStr: '%s'",
      //     lastState_.consumed, lastState_.commitString.c_str(),
      //     lastState_.composingBuffer.c_str());

      if (lastState_.consumed || hasCommit) {
        applyStateToContext_(pic, lastState_, "");
      }
    } else {
      // LogMessage("Failed to deserialize state update");
      *pfEaten = FALSE;
    }
  } else {
    // LogMessage("IPC Call failed");
    *pfEaten = FALSE;
  }

  return S_OK;
}

STDAPI McBopomofoTIP::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                                  BOOL* pfEaten) {
  UNREFERENCED_PARAMETER(pic);
  if (pfEaten == nullptr) {
    return E_INVALIDARG;
  }
  *pfEaten =
      IsLeftShiftKeyEvent(wParam, lParam) && shiftToggleKeyPending_ ? TRUE
                                                                   : FALSE;
  return S_OK;
}

STDAPI McBopomofoTIP::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                              BOOL* pfEaten) {
  UNREFERENCED_PARAMETER(pic);
  if (pfEaten == nullptr) {
    return E_INVALIDARG;
  }
  if (isProcessDisabled_()) {
    shiftToggleKeyPending_ = false;
    *pfEaten = FALSE;
    return S_OK;
  }

  BYTE keyboardState[256];
  GetKeyboardState(keyboardState);
  if (handleStandaloneShiftKeyUp_(wParam, lParam, keyboardState)) {
    *pfEaten = TRUE;
    return S_OK;
  }

  *pfEaten = FALSE;
  return S_OK;
}

STDAPI McBopomofoTIP::OnPreservedKey(ITfContext* pic, REFGUID rguid,
                                     BOOL* pfEaten) {
  UNREFERENCED_PARAMETER(pic);
  UNREFERENCED_PARAMETER(rguid);
  if (pfEaten == nullptr) {
    return E_INVALIDARG;
  }

  *pfEaten = FALSE;
  return S_OK;
}

STDAPI McBopomofoTIP::OnChange(REFGUID rguid) {
  if (!IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
    return S_OK;
  }

  const bool isOpen = IsOpen();
  // LogMessage("GUID_COMPARTMENT_KEYBOARD_OPENCLOSE changed: %s",
  //            isOpen ? "OPEN" : "CLOSED");
  if (!isOpen) {
    resetServerState_();
  }

  RefreshLangBar();
  return S_OK;
}

STDAPI McBopomofoTIP::OnCompositionTerminated(TfEditCookie ecWrite,
                                              ITfComposition* pComposition) {
  UNREFERENCED_PARAMETER(ecWrite);
  if (pComposition_ == pComposition) {
    pComposition_->Release();
    pComposition_ = nullptr;
  }
  return S_OK;
}

#include "DisplayAttributeInfo.h"

STDAPI McBopomofoTIP::EnumDisplayAttributeInfo(
    IEnumTfDisplayAttributeInfo** ppEnum) {
  if (ppEnum == nullptr) return E_INVALIDARG;
  *ppEnum = new CEnumDisplayAttributeInfo();
  return (*ppEnum != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDAPI McBopomofoTIP::GetDisplayAttributeInfo(
    REFGUID guidInfo, ITfDisplayAttributeInfo** ppInfo) {
  if (ppInfo == nullptr) return E_INVALIDARG;
  *ppInfo = nullptr;

  if (IsEqualGUID(guidInfo, c_guidDisplayAttributeInput)) {
    TF_DISPLAYATTRIBUTE da = SolidInputDisplayAttribute();
    *ppInfo = new CDisplayAttributeInfo(c_guidDisplayAttributeInput, da,
                                        L"KeyKey 41 Solid Underline Input");
  } else if (IsEqualGUID(guidInfo, c_guidDisplayAttributeDotted)) {
    TF_DISPLAYATTRIBUTE da = DottedInputDisplayAttribute();
    *ppInfo = new CDisplayAttributeInfo(c_guidDisplayAttributeDotted, da,
                                        L"KeyKey 41 Dotted Input");
  } else if (IsEqualGUID(guidInfo, c_guidDisplayAttributeMarked)) {
    TF_DISPLAYATTRIBUTE da;
    ZeroMemory(&da, sizeof(da));
    da.lsStyle = TF_LS_SOLID;
    da.crText.type = TF_CT_SYSCOLOR;
    da.crText.nIndex = COLOR_HIGHLIGHTTEXT;
    da.crBk.type = TF_CT_SYSCOLOR;
    da.crBk.nIndex = COLOR_HIGHLIGHT;
    *ppInfo = new CDisplayAttributeInfo(c_guidDisplayAttributeMarked, da,
                                        L"Win-McBopomofo Marked");
  }

  return (*ppInfo != nullptr) ? S_OK : E_INVALIDARG;
}

STDAPI McBopomofoTIP::OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) {
  UNREFERENCED_PARAMETER(pDocMgr);
  return S_OK;
}

STDAPI McBopomofoTIP::OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) {
  UNREFERENCED_PARAMETER(pDocMgr);
  return S_OK;
}

STDAPI McBopomofoTIP::OnSetFocus(ITfDocumentMgr* pDocMgrFocus,
                                 ITfDocumentMgr* pDocMgrPrevFocus) {
  UNREFERENCED_PARAMETER(pDocMgrPrevFocus);
  shiftToggleKeyPending_ = false;
  if (pDocMgrFocus == nullptr) {
    resetServerState_();
  }
  RefreshLangBar();
  return S_OK;
}

STDAPI McBopomofoTIP::OnPushContext(ITfContext* pic) {
  UNREFERENCED_PARAMETER(pic);
  RefreshLangBar();
  return S_OK;
}

STDAPI McBopomofoTIP::OnPopContext(ITfContext* pic) {
  UNREFERENCED_PARAMETER(pic);
  RefreshLangBar();
  return S_OK;
}

STDAPI McBopomofoTIP::OnSetThreadFocus() {
  shiftToggleKeyPending_ = false;
  RefreshLangBar();
  return S_OK;
}

STDAPI McBopomofoTIP::OnKillThreadFocus() {
  shiftToggleKeyPending_ = false;
  resetServerState_();
  return S_OK;
}

bool McBopomofoTIP::IsOpen() {
  DWORD value = 1;
  if (ReadDWORDCompartmentValue(ptim_, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                &value)) {
    return value != 0;
  }
  return true;
}

void McBopomofoTIP::OpenSymbolTable() {
  if (!IsOpen()) {
    ToggleOpenClose();
  }

  ITfContext* context = nullptr;
  if (!GetFocusedContext(ptim_, &context)) {
    return;
  }

  const UINT scanCode = MapVirtualKeyW(VK_OEM_3, MAPVK_VK_TO_VSC);
  const LPARAM keyData = static_cast<LPARAM>(scanCode << 16);
  BOOL eaten = FALSE;
  OnKeyDown(context, VK_OEM_3, keyData, &eaten);
  context->Release();
}

void McBopomofoTIP::applyStateToContext_(
    ITfContext* context, const McBopomofo::IPC::StateUpdatePayload& state,
    const char* logPrefix) {
  UNREFERENCED_PARAMETER(logPrefix);
  if (!context) {
    // LogMessage("%sRequestEditSession skipped: null context", logPrefix);
    return;
  }

  CStateEditSession* pEditSession = new CStateEditSession(context, this, state);
  HRESULT hr = E_FAIL;
  context->RequestEditSession(tid_, pEditSession, TF_ES_SYNC | TF_ES_READWRITE,
                              &hr);
  // LogMessage("%sRequestEditSession returned: 0x%08X", logPrefix, hr);
  pEditSession->Release();
}

void McBopomofoTIP::resetServerState_() {
  // LogMessage("Sending RESET command to server");
  McBopomofo::IPC::NamedPipeClient pipe(McBopomofo::IPC::PIPE_NAME);
  std::string response;
  if (pipe.Call(McBopomofo::IPC::SerializeReset(), response)) {
    // LogMessage("Reset response received");
    McBopomofo::IPC::StateUpdatePayload state;
    if (McBopomofo::IPC::DeserializeStateUpdate(response, state)) {
      lastState_ = state;
      // LogMessage("Reset state: CommitStr='%s', CompStr='%s'",
      //            state.commitString.c_str(), state.composingBuffer.c_str());

      ITfContext* pContext = nullptr;
      if (GetFocusedContext(ptim_, &pContext)) {
        applyStateToContext_(pContext, state, "Reset ");
        pContext->Release();
      } else {
        // LogMessage("Reset could not acquire focused context for edit
        // session");
      }
    }
  }
}

void McBopomofoTIP::RefreshLangBar() {
  if (pModeIconButton_) {
    // LogMessage("Refreshing mode icon button");
    pModeIconButton_->Update();
  }
  if (pSwitchLangButton_) {
    // LogMessage("Refreshing switch lang button");
    pSwitchLangButton_->Update();
  }
  if (pFullHalfButton_) {
    // LogMessage("Refreshing full/half punctuation button");
    pFullHalfButton_->Update();
  }
  if (pSymbolTableButton_) {
    pSymbolTableButton_->Update();
  }
  if (pSettingsButton_) {
    // LogMessage("Refreshing settings button");
    pSettingsButton_->Update();
  }
}

void McBopomofoTIP::updateProcessDisabledState_() {
  processDisabled_ = false;

  McBopomofo::IPC::ProcessDisabledQueryPayload query;
  query.processName = CurrentProcessNameUtf8();
  if (query.processName.empty()) {
    return;
  }

  McBopomofo::IPC::NamedPipeClient pipe(McBopomofo::IPC::PIPE_NAME);
  std::string response;
  if (!pipe.Call(McBopomofo::IPC::SerializeProcessDisabledQuery(query),
                 response)) {
    // LogMessage("IS_PROCESS_DISABLED IPC Call failed, fallback to enabled");
    return;
  }

  McBopomofo::IPC::ProcessDisabledResponsePayload payload;
  if (!McBopomofo::IPC::DeserializeProcessDisabledResponse(response, payload)) {
    // LogMessage("IS_PROCESS_DISABLED response deserialize failed, fallback to
    // enabled");
    return;
  }

  processDisabled_ = payload.disabled;
}

bool McBopomofoTIP::shouldToggleOpenCloseWithShift_() const {
  McBopomofo::IPC::NamedPipeClient pipe(McBopomofo::IPC::PIPE_NAME);
  std::string response;
  if (!pipe.Call(McBopomofo::IPC::SerializeGetSettings(), response)) {
    // LogMessage("GET_SETTINGS IPC Call failed, fallback to enabled");
    return true;
  }

  McBopomofo::IPC::ClientSettingsPayload payload;
  if (!McBopomofo::IPC::DeserializeClientSettings(response, payload)) {
    // LogMessage("GET_SETTINGS response deserialize failed, fallback to
    // enabled");
    return true;
  }

  return payload.shiftToggleOpenClose;
}

bool McBopomofoTIP::handleStandaloneShiftKeyDown_(
    WPARAM wParam, LPARAM lParam, const BYTE keyboardState[256]) {
  if (IsLeftShiftKeyEvent(wParam, lParam) &&
      IsOnlyShiftKeyEvent(wParam, keyboardState)) {
    shiftToggleKeyPending_ = true;
    return true;
  }

  shiftToggleKeyPending_ = false;
  return false;
}

bool McBopomofoTIP::handleStandaloneShiftKeyUp_(WPARAM wParam,
                                                LPARAM lParam,
                                                const BYTE keyboardState[256]) {
  const bool shouldToggle = IsLeftShiftKeyEvent(wParam, lParam) &&
                            IsOnlyShiftKeyEvent(wParam, keyboardState) &&
                            shiftToggleKeyPending_ &&
                            shouldToggleOpenCloseWithShift_();
  shiftToggleKeyPending_ = false;
  if (!shouldToggle) {
    return false;
  }

  ToggleOpenClose();
  return true;
}

void McBopomofoTIP::ToggleOpenClose() {
  if (ptim_) {
    bool currentOpen = IsOpen();
    // LogMessage("ToggleOpenClose: current state = %s, toggling to %s",
    //            currentOpen ? "OPEN" : "CLOSED",
    //            currentOpen ? "CLOSED" : "OPEN");

    ITfCompartmentMgr* pCompMgr = nullptr;
    if (SUCCEEDED(
            ptim_->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr))) {
      ITfCompartment* pComp = nullptr;
      if (SUCCEEDED(pCompMgr->GetCompartment(
              GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pComp))) {
        VARIANT var;
        var.vt = VT_I4;
        var.lVal = currentOpen ? 0 : 1;
        pComp->SetValue(tid_, &var);
        pComp->Release();

        // LogMessage("Compartment value set to: %d", var.lVal);
        RefreshLangBar();
      }
      pCompMgr->Release();
    }
  }
}
