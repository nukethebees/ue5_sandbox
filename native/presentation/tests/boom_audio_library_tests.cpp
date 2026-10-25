#include <ioj/presentation/boom_audio_library.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace ioj::presentation::audio_tests {
namespace {
class BoomAudioLibrary : public testing::Test {
  protected:
    void SetUp() override { std::filesystem::create_directories(root); }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    std::filesystem::path root{
        std::filesystem::temp_directory_path() /
        ("boom-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
};
}

TEST_F(BoomAudioLibrary, ReportsUnsetAndMissingRootsWithoutPackChecks) {
    for (auto const& path : {std::filesystem::path{}, root / "missing"}) {
        auto const result{resolve_boom_audio_library(path)};
        EXPECT_FALSE(result.root.valid);
        EXPECT_EQ(result.root.path, path);
        EXPECT_TRUE(result.directories.empty());
        EXPECT_TRUE(result.menu_ambience_source.path.empty());
        EXPECT_TRUE(result.menu_button_pressed_source.path.empty());
        EXPECT_TRUE(result.player_ship_ambience_source.path.empty());
    }
}

TEST_F(BoomAudioLibrary, ResolvesAvailablePackDirectoriesRelativeToTheRoot) {
    auto const pack{root / "sci-fi_ds_2220mb"};
    std::filesystem::create_directory(pack);
    for (auto const* filename : {"AMBDsgn_Ambience Computer Room Low 02_B00M_SFDS.wav",
                                 "SCICmpt_Computer Beep High 01_B00M_SFDS.wav",
                                 "AMBDsgn_Ambience Spaceship High 02_B00M_SFDS.wav"}) {
        ASSERT_TRUE(std::ofstream{pack / filename} << "wave fixture");
    }
    auto const result{resolve_boom_audio_library(root)};
    EXPECT_TRUE(result.root.valid);
    ASSERT_EQ(result.directories.size(), 1u);
    EXPECT_EQ(result.directories[0].relative_path, "sci-fi_ds_2220mb");
    EXPECT_EQ(result.directories[0].path, pack);
    EXPECT_TRUE(result.directories[0].valid);
    auto const check{[&](AudioSource const& source, char const* filename) {
        EXPECT_TRUE(source.valid);
        EXPECT_EQ(source.relative_path, std::filesystem::path{"sci-fi_ds_2220mb"} / filename);
        EXPECT_EQ(source.path, pack / filename);
    }};
    check(result.menu_ambience_source, "AMBDsgn_Ambience Computer Room Low 02_B00M_SFDS.wav");
    check(result.menu_button_pressed_source, "SCICmpt_Computer Beep High 01_B00M_SFDS.wav");
    check(result.player_ship_ambience_source, "AMBDsgn_Ambience Spaceship High 02_B00M_SFDS.wav");
}

TEST_F(BoomAudioLibrary, ReportsMissingPackAndSources) {
    auto const result{resolve_boom_audio_library(root)};
    ASSERT_EQ(result.directories.size(), 1u);
    EXPECT_FALSE(result.directories[0].valid);
    EXPECT_FALSE(result.menu_ambience_source.valid);
    EXPECT_FALSE(result.menu_button_pressed_source.valid);
    EXPECT_FALSE(result.player_ship_ambience_source.valid);
}

TEST_F(BoomAudioLibrary, ReportsNonDirectoryPack) {
    ASSERT_TRUE(std::ofstream{root / "sci-fi_ds_2220mb"} << "not a directory");
    auto const result{resolve_boom_audio_library(root)};
    ASSERT_EQ(result.directories.size(), 1u);
    EXPECT_FALSE(result.directories[0].valid);
}

TEST_F(BoomAudioLibrary, ReportsMissingSourcesWithResolvedPaths) {
    std::filesystem::create_directory(root / "sci-fi_ds_2220mb");
    auto const result{resolve_boom_audio_library(root)};
    for (auto const* source : {&result.menu_ambience_source,
                               &result.menu_button_pressed_source,
                               &result.player_ship_ambience_source}) {
        EXPECT_FALSE(source->valid);
        EXPECT_FALSE(source->path.empty());
        EXPECT_EQ(source->path, root / source->relative_path);
    }
}
}
