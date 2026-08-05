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

#include "DisplayAttributeInfo.h"

const GUID c_guidDisplayAttributeInput = {
    0x4b688cd4,
    0xcfb6,
    0x4767,
    {0xad, 0x80, 0x4d, 0x56, 0x20, 0x86, 0xfc, 0x3b}};

const GUID c_guidDisplayAttributeDotted = {
    0x64026a46,
    0xa120,
    0x4b5f,
    {0xa1, 0x87, 0xb3, 0x9e, 0xd5, 0x16, 0x41, 0x9c}};

TF_DISPLAYATTRIBUTE SolidInputDisplayAttribute() {
  TF_DISPLAYATTRIBUTE da = {};
  da.crText.type = TF_CT_NONE;
  da.lsStyle = TF_LS_SOLID;
  da.fBoldLine = FALSE;
  return da;
}

TF_DISPLAYATTRIBUTE DottedInputDisplayAttribute() {
  TF_DISPLAYATTRIBUTE da = {};
  da.crText.type = TF_CT_NONE;
  da.lsStyle = TF_LS_DOT;
  da.fBoldLine = FALSE;
  return da;
}

namespace {

const GUID kColorAttributeGuids[] = {
    {0xa77746af, 0xe89e, 0x4591,
     {0xaf, 0xa2, 0x58, 0xa8, 0xee, 0xa8, 0x18, 0x15}},  // System
    c_guidDisplayAttributeInput,                         // Purple
    {0xd7610ed1, 0x0afe, 0x42a0,
     {0xb8, 0x90, 0xcd, 0x93, 0x52, 0xca, 0x38, 0xb4}},  // Blue
    {0x73767165, 0x6ee0, 0x4af1,
     {0xb0, 0xba, 0x85, 0xc8, 0x8c, 0x2b, 0xcf, 0xc6}},  // Black
    {0xd4f71971, 0x4604, 0x4778,
     {0x8e, 0xc6, 0xc7, 0xb1, 0x29, 0xa7, 0xa3, 0xc8}},  // White
    {0x1f60256c, 0xd136, 0x4f8d,
     {0xac, 0xb9, 0x8f, 0xf4, 0xfd, 0xc0, 0x9b, 0xc9}},  // Gray
    {0xc79e4ad8, 0xfefb, 0x4d94,
     {0xb7, 0xc0, 0xae, 0xc3, 0xd1, 0x1c, 0xe1, 0x64}},  // Red
    {0x4f0b866b, 0xd231, 0x464b,
     {0xa5, 0xef, 0x8b, 0xe8, 0xad, 0xdb, 0x8c, 0x58}},  // Green
};

constexpr uint32_t kColorValues[] = {
    0xffffffffu, 0xB45DB7, 0x0078D7, 0x000000,
    0xFFFFFF,   0x808080, 0xC62828, 0x2E7D32,
};

constexpr int kColorAttributeCount = 8;
constexpr int kDottedAttributeIndex = kColorAttributeCount;
constexpr int kMarkedAttributeIndex = kColorAttributeCount + 1;
constexpr int kAttributeCount = kColorAttributeCount + 2;

COLORREF RgbValueToColorRef(uint32_t rgb) {
  return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

}  // namespace

const GUID& CompositionDisplayAttributeGuidForColor(uint32_t rgb) {
  for (int i = 0; i < kColorAttributeCount; ++i) {
    if (kColorValues[i] == rgb) return kColorAttributeGuids[i];
  }
  return c_guidDisplayAttributeInput;
}

const GUID c_guidDisplayAttributeMarked = {
    0xd82c4a26,
    0xe0cc,
    0x43bb,
    {0x8c, 0xf2, 0xbb, 0xb, 0xbf, 0xf4, 0xfe, 0x70}};

// ----------------------------------------------------------------------------
// CDisplayAttributeInfo
// ----------------------------------------------------------------------------
CDisplayAttributeInfo::CDisplayAttributeInfo(REFGUID guid,
                                             TF_DISPLAYATTRIBUTE da,
                                             const WCHAR* desc)
    : cRef_(1), guid_(guid), da_(da), daDefault_(da), desc_(desc) {}

CDisplayAttributeInfo::~CDisplayAttributeInfo() {}

STDAPI CDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
  if (ppvObj == nullptr) return E_INVALIDARG;
  *ppvObj = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
    *ppvObj = static_cast<ITfDisplayAttributeInfo*>(this);
  }
  if (*ppvObj) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) CDisplayAttributeInfo::AddRef(void) {
  return InterlockedIncrement(&cRef_);
}

STDAPI_(ULONG) CDisplayAttributeInfo::Release(void) {
  LONG cr = InterlockedDecrement(&cRef_);
  if (cr == 0) delete this;
  return cr;
}

STDAPI CDisplayAttributeInfo::GetGUID(GUID* pguid) {
  if (pguid == nullptr) return E_INVALIDARG;
  *pguid = guid_;
  return S_OK;
}

STDAPI CDisplayAttributeInfo::GetDescription(BSTR* pbstrDesc) {
  if (pbstrDesc == nullptr) return E_INVALIDARG;
  *pbstrDesc = SysAllocString(desc_);
  return (*pbstrDesc != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDAPI CDisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) {
  if (pda == nullptr) return E_INVALIDARG;
  *pda = da_;
  return S_OK;
}

STDAPI CDisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) {
  if (pda == nullptr) return E_INVALIDARG;
  da_ = *pda;
  return S_OK;
}

STDAPI CDisplayAttributeInfo::Reset() {
  da_ = daDefault_;
  return S_OK;
}

// ----------------------------------------------------------------------------
// CEnumDisplayAttributeInfo
// ----------------------------------------------------------------------------
CEnumDisplayAttributeInfo::CEnumDisplayAttributeInfo() : cRef_(1), index_(0) {}
CEnumDisplayAttributeInfo::~CEnumDisplayAttributeInfo() {}

STDAPI CEnumDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
  if (ppvObj == nullptr) return E_INVALIDARG;
  *ppvObj = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
    *ppvObj = static_cast<IEnumTfDisplayAttributeInfo*>(this);
  }
  if (*ppvObj) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) CEnumDisplayAttributeInfo::AddRef(void) {
  return InterlockedIncrement(&cRef_);
}

STDAPI_(ULONG) CEnumDisplayAttributeInfo::Release(void) {
  LONG cr = InterlockedDecrement(&cRef_);
  if (cr == 0) delete this;
  return cr;
}

STDAPI CEnumDisplayAttributeInfo::Clone(IEnumTfDisplayAttributeInfo** ppEnum) {
  if (ppEnum == nullptr) return E_INVALIDARG;
  *ppEnum = new CEnumDisplayAttributeInfo();
  if (*ppEnum == nullptr) return E_OUTOFMEMORY;
  ((CEnumDisplayAttributeInfo*)*ppEnum)->index_ = index_;
  return S_OK;
}

STDAPI CEnumDisplayAttributeInfo::Next(ULONG ulCount,
                                       ITfDisplayAttributeInfo** rgInfo,
                                       ULONG* pcFetched) {
  if (pcFetched) *pcFetched = 0;
  if (ulCount == 0 || rgInfo == nullptr) return E_INVALIDARG;

  ULONG fetched = 0;
  while (fetched < ulCount && index_ < kAttributeCount) {
    if (index_ < kColorAttributeCount) {
      // Many TSF hosts ignore composition text colors.  A solid underline is
      // consistently rendered by Win32, UWP and Chromium/Electron text hosts,
      // and remains visually distinct from the Microsoft-style dotted mode.
      TF_DISPLAYATTRIBUTE da = SolidInputDisplayAttribute();
      rgInfo[fetched] = new CDisplayAttributeInfo(
          kColorAttributeGuids[index_], da, L"KeyKey 41 Solid Underline Input");
    } else if (index_ == kDottedAttributeIndex) {
      TF_DISPLAYATTRIBUTE da = DottedInputDisplayAttribute();
      rgInfo[fetched] = new CDisplayAttributeInfo(
          c_guidDisplayAttributeDotted, da, L"KeyKey 41 Dotted Input");
    } else if (index_ == kMarkedAttributeIndex) {
      TF_DISPLAYATTRIBUTE da;
      ZeroMemory(&da, sizeof(da));
      da.lsStyle = TF_LS_NONE;
      da.crText.type = TF_CT_SYSCOLOR;
      da.crText.nIndex = COLOR_HIGHLIGHTTEXT;
      da.crBk.type = TF_CT_SYSCOLOR;
      da.crBk.nIndex = COLOR_HIGHLIGHT;
      rgInfo[fetched] = new CDisplayAttributeInfo(c_guidDisplayAttributeMarked,
                                                  da, L"Win-McBopomofo Marked");
    }
    if (rgInfo[fetched] == nullptr) return E_OUTOFMEMORY;
    index_++;
    fetched++;
  }

  if (pcFetched) *pcFetched = fetched;
  return (fetched == ulCount) ? S_OK : S_FALSE;
}

STDAPI CEnumDisplayAttributeInfo::Reset() {
  index_ = 0;
  return S_OK;
}

STDAPI CEnumDisplayAttributeInfo::Skip(ULONG ulCount) {
  index_ += ulCount;
  if (index_ > kAttributeCount) index_ = kAttributeCount;
  return S_OK;
}
