#pragma once

#include <windows.h>

namespace McBopomofo {

inline bool MatchesConversionHotkey(int configuredKey, int modifiers,
                                    WPARAM virtualKey, bool ctrl, bool shift,
                                    bool alt) {
  if (modifiers < 1 || modifiers > 7) {
    return false;
  }
  return static_cast<int>(virtualKey) == configuredKey &&
         ctrl == ((modifiers & 1) != 0) &&
         shift == ((modifiers & 2) != 0) &&
         alt == ((modifiers & 4) != 0);
}

}  // namespace McBopomofo
