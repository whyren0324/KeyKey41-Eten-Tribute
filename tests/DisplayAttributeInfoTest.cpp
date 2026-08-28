#include <gtest/gtest.h>

#include <array>

#include "DisplayAttributeInfo.h"

TEST(DisplayAttributeInfoTest, SharedDirectLookupStylesAreStable) {
  const TF_DISPLAYATTRIBUTE solid = SolidInputDisplayAttribute();
  EXPECT_EQ(solid.crText.type, TF_CT_NONE);
  EXPECT_EQ(solid.lsStyle, TF_LS_SOLID);
  EXPECT_FALSE(solid.fBoldLine);

  const TF_DISPLAYATTRIBUTE dotted = DottedInputDisplayAttribute();
  EXPECT_EQ(dotted.crText.type, TF_CT_NONE);
  EXPECT_EQ(dotted.lsStyle, TF_LS_DOT);
  EXPECT_FALSE(dotted.fBoldLine);
}

TEST(DisplayAttributeInfoTest, EverySelectableColorHasStableDistinctGuid) {
  constexpr std::array<uint32_t, 8> colors = {
      0xffffffffu, 0xB45DB7, 0x0078D7, 0x000000,
      0xFFFFFF,   0x808080, 0xC62828, 0x2E7D32,
  };

  for (size_t i = 0; i < colors.size(); ++i) {
    const GUID& first = CompositionDisplayAttributeGuidForColor(colors[i]);
    const GUID& second = CompositionDisplayAttributeGuidForColor(colors[i]);
    EXPECT_TRUE(IsEqualGUID(first, second));
    for (size_t j = i + 1; j < colors.size(); ++j) {
      EXPECT_FALSE(IsEqualGUID(
          first, CompositionDisplayAttributeGuidForColor(colors[j])));
    }
  }
}

TEST(DisplayAttributeInfoTest, EnumeratorPublishesSolidDottedAndMarked) {
  CEnumDisplayAttributeInfo enumerator;
  std::array<ITfDisplayAttributeInfo*, 10> infos = {};
  ULONG fetched = 0;
  EXPECT_EQ(enumerator.Next(static_cast<ULONG>(infos.size()), infos.data(),
                            &fetched),
            S_OK);
  ASSERT_EQ(fetched, infos.size());

  TF_DISPLAYATTRIBUTE purple = {};
  ASSERT_EQ(infos[1]->GetAttributeInfo(&purple), S_OK);
  EXPECT_EQ(purple.crText.type, TF_CT_NONE);
  EXPECT_EQ(purple.lsStyle, TF_LS_SOLID);
  EXPECT_FALSE(purple.fBoldLine);

  TF_DISPLAYATTRIBUTE dotted = {};
  ASSERT_EQ(infos[8]->GetAttributeInfo(&dotted), S_OK);
  EXPECT_EQ(dotted.lsStyle, TF_LS_DOT);

  for (auto* info : infos) info->Release();
}
