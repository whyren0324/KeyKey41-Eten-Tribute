#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "UserOverrideModel.h"

using McBopomofo::UserOverrideModel;
namespace {
class SelectionLearningTest : public testing::Test {
 protected:
  std::filesystem::path dir;
  void SetUp() override {
    dir = std::filesystem::temp_directory_path() /
        ("keykey-learning-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
  }
  void TearDown() override { std::filesystem::remove_all(dir); }
};

TEST_F(SelectionLearningTest, RestartPreservesFrequencyAndLongPhrases) {
  auto file = dir / "learning.txt";
  UserOverrideModel original(100, 90 * 86400);
  original.observe("reading:test", "long phrase with spaces", 100);
  original.observe("reading:test", "long phrase with spaces", 101);
  original.observe("reading:test", "other", 102);
  ASSERT_TRUE(original.save(file));
  UserOverrideModel restored(100, 90 * 86400);
  ASSERT_TRUE(restored.load(file));
  EXPECT_EQ(restored.suggest("reading:test", 102 + 86400).candidate,
            "long phrase with spaces");
  restored.observe("reading:test", "other", 103);
  restored.observe("reading:test", "other", 104);
  ASSERT_TRUE(restored.save(file));
  UserOverrideModel next(100, 90 * 86400);
  ASSERT_TRUE(next.load(file));
  EXPECT_EQ(next.suggest("reading:test", 104).candidate, "other");
}

TEST_F(SelectionLearningTest, InvalidLoadDoesNotDestroyLiveMemory) {
  UserOverrideModel model(100, 86400);
  model.observe("key", "kept", 100);
  auto file = dir / "invalid.txt";
  for (const auto& data : {
      "UNKNOWN\n", "KEYKEY_LEARNING_V1\n\"key\" \"bad\" -1 100 0\n",
      "KEYKEY_LEARNING_V1\n\"key\" \"bad\" 1 nan 0\n",
      "KEYKEY_LEARNING_V1\n\"key\" \"bad\" 1 100 9\n"}) {
    { std::ofstream out(file); out << data; }
    EXPECT_FALSE(model.load(file));
    EXPECT_EQ(model.suggest("key", 100).candidate, "kept");
  }
}

TEST_F(SelectionLearningTest, RestoresEvictionOrderAndReportsWriteFailure) {
  UserOverrideModel model(2, 86400);
  model.observe("a", "A", 100);
  model.observe("b", "B", 101);
  auto file = dir / "learning.txt";
  ASSERT_TRUE(model.save(file));
  UserOverrideModel restored(2, 86400);
  ASSERT_TRUE(restored.load(file));
  restored.observe("c", "C", 102);
  EXPECT_TRUE(restored.suggest("a", 102).empty());
  EXPECT_EQ(restored.suggest("b", 102).candidate, "B");
  EXPECT_FALSE(restored.save(dir / "missing" / "learning.txt"));
}
}
