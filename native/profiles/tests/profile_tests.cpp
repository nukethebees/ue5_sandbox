#include "test_support.h"
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/progression.h>

#include <gtest/gtest.h>


namespace ioj::profiles {

namespace save_profile_manager_test {
auto make_record(Timestamp date, std::string level, KillCount kills) -> ScoreRecord {
    return {.date = date,
            .level_name = std::move(level),
            .outcome = LevelOutcome::Victory,
            .kills = kills,
            .time_seconds = 60.f};
}
}

TEST(ProfileManager, CreatesEmptyDefaultProfile) {
    FakeProfileStorage storage;
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager);
    ASSERT_EQ(manager->profiles().size(), 1u);
    EXPECT_EQ(manager->profiles().front().profile_id, manager->active_profile_id());
    EXPECT_TRUE(manager->active_records().empty());
}
TEST(ProfileManager, CreatesAndActivatesUniquelyNamedProfiles) {

    FakeProfileStorage storage{};
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager) << "Profile manager initialises";

    auto const empty{manager->create_profile("   ")};
    EXPECT_EQ(empty.result, CreateProfileResult::empty_name) << "Whitespace-only name is rejected";

    auto const created{manager->create_profile("  Commander  ")};
    EXPECT_EQ(created.result, CreateProfileResult::succeeded) << "Profile is created";
    EXPECT_EQ(manager->active_profile_id(), created.profile_id) << "New profile is activated";

    auto const duplicate{manager->create_profile("commander")};
    EXPECT_EQ(duplicate.result, CreateProfileResult::duplicate_name)
        << "Names are unique without regard to case";
}

TEST(ProfileManager, RoutesResultsAndResetsOnlyTheTestProfile) {

    FakeProfileStorage storage{};
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager) << "Profile manager initialises";
    auto const default_profile_id{manager->active_profile_id()};
    auto const default_record{
        save_profile_manager_test::make_record(test_date(2026, 8, 20), "DefaultBattle", 3)};
    EXPECT_TRUE(manager->append_score_record(default_record)) << "Default result is appended";

    std::vector<ScoreRecord> const fixture{
        save_profile_manager_test::make_record(test_date(2026, 8, 21), "TestBattleOne", 5),
        save_profile_manager_test::make_record(test_date(2026, 8, 22), "TestBattleTwo", 7),
    };
    EXPECT_TRUE(manager->reset_test_profile(fixture)) << "Test profile is reset";
    auto const test_profile_id{manager->active_profile_id()};
    EXPECT_TRUE(test_profile_id != default_profile_id) << "Test profile differs from default";

    std::vector<ScoreRecord> default_records{};
    std::vector<ScoreRecord> test_records{};
    EXPECT_TRUE(manager->load_profile_records(default_profile_id, default_records))
        << "Default profile still loads";
    EXPECT_TRUE(manager->load_profile_records(test_profile_id, test_records))
        << "Test profile loads";
    EXPECT_EQ(default_records.size(), 1) << "Default outcome is preserved";
    EXPECT_EQ(test_records.size(), 2) << "Fixture outcomes replace test results";

    EXPECT_TRUE(manager->reset_test_profile(fixture)) << "Repeated reset succeeds";
    EXPECT_TRUE(manager->active_profile_id() == test_profile_id)
        << "Reset test profile remains active";
    EXPECT_EQ(manager->active_records().size(), 2) << "Reset does not duplicate fixture outcomes";

    storage.results.erase(default_profile_id);
    EXPECT_FALSE(manager->activate_profile(default_profile_id))
        << "Activation fails when profile results are unavailable";
    EXPECT_EQ(manager->active_profile_id(), test_profile_id)
        << "Failed activation preserves the active profile";

    storage.results.insert_or_assign(default_profile_id,
                                     ProfileResultsData{.score_records = {default_record}});
    EXPECT_TRUE(manager->activate_profile(default_profile_id))
        << "Available profile can be activated";
    EXPECT_EQ(manager->active_profile_id(), default_profile_id)
        << "Activation switches the active profile";
}

TEST(ProfileManager, FailedCompletionSaveDoesNotClaimPersistence) {

    FakeProfileStorage storage{};
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager) << "Profile manager initialises";

    storage.results_save_fails = true;
    auto const record{
        save_profile_manager_test::make_record(test_date(2026, 9, 4), "border-skirmish", 6)};
    auto const failed{manager->append_score_record(record)};
    EXPECT_FALSE(failed.persisted) << "Failed storage reports that completion was not persisted";
    EXPECT_FALSE(failed.new_best_time) << "Unsaved results cannot claim a new best";
    EXPECT_EQ(manager->active_records().size(), 0) << "Failed completion is rolled back in memory";
}

TEST(ProfileManager, LevelCompletionPersistsAcrossManagerReload) {

    FakeProfileStorage storage{};
    std::string profile_id;
    {
        auto manager{ProfileManager::open(storage.make())};
        ASSERT_TRUE(manager) << "Profile manager initialises";
        profile_id = manager->active_profile_id();
        auto const record{
            save_profile_manager_test::make_record(test_date(2026, 9, 5), "asteroid-field", 4)};
        EXPECT_TRUE(manager->append_score_record(record)) << "Completion is saved";
    }

    auto reloaded{ProfileManager::open(storage.make())};
    ASSERT_TRUE(reloaded) << "Profile manager reloads";
    EXPECT_EQ(reloaded->active_profile_id(), profile_id) << "Active profile is restored";
    auto const progress{summarize_level_progress(levels::LevelId{std::string{"asteroid-field"}},
                                                 reloaded->active_records())};
    EXPECT_TRUE(progress.state() == LevelProgressState::Completed)
        << "Reloaded level remains completed";
}

TEST(ProfileManager, ConcurrentManagersRebaseBeforeAppending) {

    FakeProfileStorage storage{};
    auto first{ProfileManager::open(storage.make())};
    auto second{ProfileManager::open(storage.make())};
    ASSERT_TRUE(first) << "First manager initialises";
    ASSERT_TRUE(second) << "Second manager initialises";

    auto const first_record{
        save_profile_manager_test::make_record(test_date(2026, 9, 10), "first-editor", 2)};
    auto const second_record{
        save_profile_manager_test::make_record(test_date(2026, 9, 11), "second-editor", 3)};
    auto const first_save{first->append_score_record(first_record)};
    ASSERT_TRUE(first_save);
    EXPECT_TRUE(first_save.new_best_time);
    auto const second_save{second->append_score_record(second_record)};
    ASSERT_TRUE(second_save);
    EXPECT_TRUE(second_save.new_best_time);

    auto faster{first_record};
    faster.time_seconds = 50.f;
    auto const faster_save{first->append_score_record(faster)};
    ASSERT_TRUE(faster_save);
    EXPECT_TRUE(faster_save.new_best_time);
    auto stale_attempt{first_record};
    stale_attempt.time_seconds = 55.f;
    auto const stale_save{second->append_score_record(stale_attempt)};
    ASSERT_TRUE(stale_save);
    EXPECT_FALSE(stale_save.new_best_time)
        << "A stale manager compares with the latest successful time";

    auto reloaded{ProfileManager::open(storage.make())};
    ASSERT_TRUE(reloaded) << "Combined state reloads";
    auto const records{reloaded->active_records()};
    ASSERT_EQ(records.size(), 4u);
    EXPECT_EQ(records[0], first_record);
    EXPECT_EQ(records[1], second_record);
    EXPECT_EQ(records[2], faster);
    EXPECT_EQ(records[3], stale_attempt);
}

TEST(ProfileManager, ConcurrentManagersPreserveNewProfiles) {

    FakeProfileStorage storage{};
    auto first{ProfileManager::open(storage.make())};
    auto second{ProfileManager::open(storage.make())};
    ASSERT_TRUE(first) << "First manager initialises";
    ASSERT_TRUE(second) << "Second manager initialises";

    EXPECT_EQ(first->create_profile("Alpha").result, CreateProfileResult::succeeded)
        << "First profile is created";
    EXPECT_EQ(second->create_profile("Bravo").result, CreateProfileResult::succeeded)
        << "Second profile is created from stale state";

    auto reloaded{ProfileManager::open(storage.make())};
    ASSERT_TRUE(reloaded) << "Combined profiles reload";
    EXPECT_EQ(reloaded->profiles().size(), 3) << "Default and both new profiles remain";
}

TEST(ProfileManager, RejectsUnsupportedProfileVersions) {
    for (std::int32_t version{}; version <= current_save_version + 1; ++version) {
        if (version == current_save_version) {
            continue;
        }
        SCOPED_TRACE(version);
        FakeProfileStorage storage{
            .index_exists = true,
            .index = {.save_version = version,
                      .active_profile_id = "unsupported",
                      .profiles = {{.profile_id = "unsupported", .display_name = "Unsupported"}}},
            .results = {{"unsupported", ProfileResultsData{}}},
        };
        auto manager{ProfileManager::open(storage.make())};
        EXPECT_FALSE(manager);
        EXPECT_EQ(storage.index_save_count, 0) << "Rejected index is not rewritten";
    }

    for (std::int32_t version{}; version <= current_save_version + 1; ++version) {
        if (version == current_save_version) {
            continue;
        }
        SCOPED_TRACE(version);
        FakeProfileStorage storage{
            .index_exists = true,
            .index = {.active_profile_id = "unsupported",
                      .profiles = {{.profile_id = "unsupported", .display_name = "Unsupported"}}},
            .results = {{"unsupported", ProfileResultsData{.save_version = version}}},
        };
        auto manager{ProfileManager::open(storage.make())};
        EXPECT_FALSE(manager);
        EXPECT_EQ(storage.index_save_count, 0) << "Rejected results do not rewrite the index";
    }
}

TEST(ProfileManager, RejectsMissingResultsReferencedByAnExistingIndex) {

    FakeProfileStorage storage{
        .index_exists = true,
        .index = {.active_profile_id = "missing",
                  .profiles = {{.profile_id = "missing", .display_name = "Missing"}}},
    };
    auto manager{ProfileManager::open(storage.make())};

    EXPECT_FALSE(manager) << "Missing results fail closed";
    EXPECT_FALSE(storage.results.contains("missing"))
        << "Missing results are not replaced with an empty file";
}

}
