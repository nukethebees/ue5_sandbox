#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/profile_id.h>
#include <ioj/profiles/profile_manager.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <fstream>
#include <thread>

namespace ioj::profiles {
namespace storage_tests {
struct Directory {
    Directory()
        : path{std::filesystem::temp_directory_path() / ("native-profiles-" + make_profile_id())} {
        std::filesystem::create_directories(path);
    }
    ~Directory() { std::filesystem::remove_all(path); }
    std::filesystem::path path;
};
}
TEST(ProfileStorage, RecoversPreviousGenerationIncludingDebugSettings) {
    storage_tests::Directory directory;
    auto const root{directory.path / std::filesystem::path{u8"profiles-\u03bb-\u4fdd\u5b58"}};
    auto storage{make_file_profile_storage(root)};
    ProfileIndexData first{.active_profile_id = "default_profile",
                           .profiles = {{.profile_id = "default_profile",
                                         .display_name = "Default Profile",
                                         .debug_settings = {.unlock_all_missions = true,
                                                            .start_levels_paused = true}}}};
    auto second{first};
    second.profiles[0].display_name = "Updated";
    ASSERT_TRUE(storage.with_exclusive_access([&] { return storage.save_index(first); }));
    ASSERT_TRUE(storage.save_index(second));
    auto const primary{root / "profiles.json"};
    std::ifstream input{primary};
    std::string bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    input.close();
    auto const changed{bytes.find("Updated")};
    ASSERT_NE(changed, std::string::npos);
    bytes[changed] = 'X';
    {
        std::ofstream corrupt{primary, std::ios::trunc};
        corrupt << bytes;
    }
    ProfileIndexData recovered;
    ASSERT_EQ(storage.load_index(recovered), LoadResult::succeeded);
    EXPECT_EQ(recovered, first);
    {
        std::ofstream corrupt{primary, std::ios::trunc};
        corrupt << "corrupt";
    }
    ASSERT_EQ(storage.load_index(recovered), LoadResult::succeeded);
    EXPECT_EQ(recovered, first);
    ASSERT_TRUE(storage.save_index(recovered));
    ProfileIndexData reloaded;
    ASSERT_EQ(storage.load_index(reloaded), LoadResult::succeeded);
    EXPECT_EQ(reloaded, first);
}
TEST(ProfileStorage, FailedReplacementPreservesPublishedRecordsAndRollsBackManager) {
    storage_tests::Directory directory;
    auto const root{directory.path / "profiles"};
    ProfileManager manager{make_file_profile_storage(root)};
    ASSERT_TRUE(manager.initialise());
    ScoreRecord const original{.date = 1,
                               .level_name = "mission",
                               .end_reason = sim::RunEndReason::DurationReached,
                               .kills = 7};
    ASSERT_TRUE(manager.append_score_record(original));
    auto const backup{root / "profile_default_profile.json.bak"};
    std::filesystem::remove(backup);
    std::filesystem::create_directory(backup);
    EXPECT_FALSE(manager.append_score_record({.date = 2, .level_name = "mission", .kills = 8}));
    ASSERT_EQ(manager.active_records().size(), 1u);
    EXPECT_EQ(manager.active_records()[0], original);
    auto storage{make_file_profile_storage(root)};
    ProfileResultsData published;
    ASSERT_EQ(storage.load_results("default_profile", published), LoadResult::succeeded);
    ASSERT_EQ(published.score_records.size(), 1u);
    EXPECT_EQ(published.score_records[0], original);
    for (auto const& entry : std::filesystem::directory_iterator{root}) {
        EXPECT_EQ(entry.path().filename().string().find(".tmp."), std::string::npos);
    }
}
TEST(ProfileStorage, MergeDeduplicatesPayloadsAndRebuildsMetadataBeforePublication) {
    storage_tests::Directory directory;
    std::array const sources{directory.path / "one", directory.path / "two"};
    ScoreRecord const first{.date = 1, .level_name = "mission", .kills = 7, .time_seconds = 3.f};
    ScoreRecord const second{.date = 2, .level_name = "mission", .kills = 4, .time_seconds = 5.f};
    for (auto const& source : sources) {
        ProfileManager manager{make_file_profile_storage(source)};
        ASSERT_TRUE(manager.initialise());
        ASSERT_TRUE(manager.append_score_record(first));
    }
    ProfileManager second_source{make_file_profile_storage(sources[1])};
    ASSERT_TRUE(second_source.initialise());
    ASSERT_TRUE(second_source.append_score_record(second));
    ASSERT_TRUE(second_source.set_unlock_all_missions(true));
    auto const destination{directory.path / "merged"};
    auto const merged{merge_profile_files(sources, {}, destination)};
    ASSERT_TRUE(merged);
    EXPECT_EQ(merged->source_record_count, 3u);
    EXPECT_EQ(merged->unique_record_count, 2u);
    ProfileManager result{make_file_profile_storage(destination)};
    ASSERT_TRUE(result.initialise());
    ASSERT_EQ(result.active_records().size(), 2u);
    EXPECT_EQ(result.active_records()[0], first);
    EXPECT_EQ(result.active_records()[1], second);
    EXPECT_EQ(result.profiles()[0].total_kills, 11);
    EXPECT_EQ(result.profiles()[0].total_simulation_duration_seconds, 8.f);
    EXPECT_TRUE(result.unlock_all_missions());
    EXPECT_FALSE(merge_profile_files(sources, {}, destination));
}
TEST(ProfileStorage, CloneIsPublishedOnceAndRemainsIsolated) {
    storage_tests::Directory directory;
    auto const shared_root{directory.path / "Shared"};
    auto const experiment_root{directory.path / "Experiment"};
    ProfileManager shared{make_file_profile_storage(shared_root)};
    ASSERT_TRUE(shared.initialise());
    ASSERT_TRUE(shared.append_score_record({.date = 1, .level_name = "mission", .kills = 7}));
    std::string error;
    ASSERT_TRUE(clone_profile_files(shared_root, experiment_root, error)) << error;
    ProfileManager experiment{make_file_profile_storage(experiment_root)};
    ASSERT_TRUE(experiment.initialise());
    ASSERT_TRUE(experiment.append_score_record({.date = 2, .level_name = "mission", .kills = 3}));
    ASSERT_TRUE(clone_profile_files(shared_root, experiment_root, error));
    ProfileManager reloaded_shared{make_file_profile_storage(shared_root)};
    ProfileManager reloaded_experiment{make_file_profile_storage(experiment_root)};
    ASSERT_TRUE(reloaded_shared.initialise());
    ASSERT_TRUE(reloaded_experiment.initialise());
    ASSERT_EQ(reloaded_shared.active_records().size(), 1u);
    ASSERT_EQ(reloaded_experiment.active_records().size(), 2u);
    EXPECT_EQ(reloaded_shared.active_records()[0].kills, 7);
    EXPECT_EQ(reloaded_experiment.active_records()[1].kills, 3);
}
TEST(ProfileStorage, ConcurrentIndependentManagersKeepEveryResult) {
    storage_tests::Directory directory;
    auto const root{directory.path / "Shared"};
    ProfileManager initial{make_file_profile_storage(root)};
    ASSERT_TRUE(initial.initialise());
    std::atomic<bool> succeeded{true};
    std::vector<std::thread> workers;
    for (int producer{}; producer < 4; ++producer) {
        workers.emplace_back([&, producer] {
            ProfileManager manager{make_file_profile_storage(root)};
            if (!manager.initialise()) {
                succeeded = false;
                return;
            }
            for (int row{}; row < 4; ++row) {
                if (!manager.append_score_record({.date = producer * 4 + row + 1,
                                                  .level_name = "mission",
                                                  .kills = producer * 4 + row})) {
                    succeeded = false;
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    ASSERT_TRUE(succeeded);
    ProfileManager reloaded{make_file_profile_storage(root)};
    ASSERT_TRUE(reloaded.initialise());
    auto const records{reloaded.active_records()};
    ASSERT_EQ(records.size(), 16u);
    std::array<bool, 16> seen{};
    for (auto const& record : records) {
        ASSERT_GE(record.kills, 0);
        ASSERT_LT(record.kills, 16);
        EXPECT_FALSE(seen[record.kills]);
        seen[record.kills] = true;
    }
}
TEST(ProfileStorage, RejectsUnsafeIdentifiersAndIncompleteSourcesWithoutPublication) {
    storage_tests::Directory directory;
    auto storage{make_file_profile_storage(directory.path / "source")};
    EXPECT_FALSE(storage.save_results("../escaped", {}));
    EXPECT_FALSE(storage.save_results("invalid",
                                      {.score_records = {{.level_name = "mission", .kills = -1}}}));
    EXPECT_FALSE(std::filesystem::exists(directory.path / "source/profile_invalid.json"));
    ProfileResultsData ignored;
    EXPECT_EQ(storage.load_results("../escaped", ignored), LoadResult::failed);
    ASSERT_TRUE(
        storage.save_index({.active_profile_id = "missing",
                            .profiles = {{.profile_id = "missing", .display_name = "Missing"}}}));
    std::string error;
    EXPECT_FALSE(
        clone_profile_files(directory.path / "source", directory.path / "destination", error));
    EXPECT_FALSE(std::filesystem::exists(directory.path / "destination"));
}
TEST(ProfileStorage, SelectsAndValidatesDevelopmentRoots) {
    auto const shared{resolve_profile_storage({.editor = true}, "development", "local")};
    ASSERT_TRUE(shared);
    EXPECT_EQ(shared->root, "development/Shared");
    auto const local{
        resolve_profile_storage({.editor = true, .unattended = true}, "development", "local")};
    ASSERT_TRUE(local);
    EXPECT_EQ(local->mode, StorageMode::local);
    auto const experiment{resolve_profile_storage(
        {.editor = true, .experiment = "Migration_Test", .clone_shared = true},
        "development",
        "local")};
    ASSERT_TRUE(experiment);
    EXPECT_EQ(experiment->root, "development/Experiments/migration_test");
    EXPECT_TRUE(experiment->clone_shared);
    EXPECT_FALSE(resolve_profile_storage(
        {.editor = true, .local = true, .shared = true}, "development", "local"));
    EXPECT_FALSE(resolve_profile_storage(
        {.editor = true, .experiment = "../shared"}, "development", "local"));
    EXPECT_FALSE(
        resolve_profile_storage({.editor = true, .clone_shared = true}, "development", "local"));
}
}
