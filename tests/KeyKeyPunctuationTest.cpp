#include <gtest/gtest.h>

#include "KeyKeyPunctuation.h"
#include "ConversionHotkey.h"

using McBopomofo::KeyKeyRightShiftPunctuation;

TEST(KeyKeyPunctuationTest, RightShiftSymbolKeys) {
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_COMMA, 0x33, true), ',');
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_PERIOD, 0x34, true), '.');
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_4, 0x1A, true), '[');
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_6, 0x1B, true), ']');
}

TEST(KeyKeyPunctuationTest, PhysicalBracketsSurviveAlternateOemMapping) {
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_4, 0x1A, true), '[');
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_1, 0x1B, true), ']');
}

TEST(KeyKeyPunctuationTest, RequiresRightShift) {
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_COMMA, 0x33, false), '\0');
  EXPECT_EQ(KeyKeyRightShiftPunctuation(VK_OEM_6, 0x1B, false), '\0');
}

TEST(ConversionHotkeyTest, DefaultCtrlF3MatchesExactly) {
  EXPECT_TRUE(
      McBopomofo::MatchesConversionHotkey(VK_F3, 1, VK_F3, true, false, false));
  EXPECT_FALSE(McBopomofo::MatchesConversionHotkey(VK_F3, 1, VK_F3, true,
                                                   true, false));
  EXPECT_FALSE(McBopomofo::MatchesConversionHotkey(VK_F3, 1, VK_F4, true,
                                                   false, false));
}

TEST(ConversionHotkeyTest, SupportsOneToThreeModifiers) {
  EXPECT_TRUE(McBopomofo::MatchesConversionHotkey('S', 3, 'S', true, true,
                                                  false));
  EXPECT_TRUE(McBopomofo::MatchesConversionHotkey('7', 7, '7', true, true,
                                                  true));
  EXPECT_FALSE(McBopomofo::MatchesConversionHotkey('A', 0, 'A', false, false,
                                                   false));
}
