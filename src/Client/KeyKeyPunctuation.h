#pragma once

#include <windows.h>

namespace McBopomofo {

// Returns the ASCII lookup key used by the engine for KeyKey-compatible
// right-Shift punctuation. Physical scan codes keep the bracket pair stable
// across Traditional and Simplified Chinese Windows keyboard layouts.
inline char KeyKeyRightShiftPunctuation(WPARAM virtualKey, UINT scanCode,
                                        bool rightShiftPressed) {
  if (!rightShiftPressed) {
    return '\0';
  }
  if (scanCode == 0x1A) {
    return '[';
  }
  if (scanCode == 0x1B) {
    return ']';
  }
  switch (virtualKey) {
    case VK_OEM_COMMA:
      return ',';
    case VK_OEM_PERIOD:
      return '.';
    case VK_OEM_4:
      return '[';
    case VK_OEM_6:
      return ']';
    default:
      return '\0';
  }
}

}  // namespace McBopomofo
