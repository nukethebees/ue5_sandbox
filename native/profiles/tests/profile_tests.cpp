#include "test_support.h"
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/progression.h>
#include <ioj/profiles/save_browser.h>

#include <gtest/gtest.h>

#include <chrono>
#include <unordered_map>

namespace ioj::profiles {

struct FakeProfileStorage {
    bool index_exists{};
    bool index_load_fails{};
    bool index_save_fails{};
    bool results_save_fails{};
    std::int32_t index_save_count{};
    ProfileIndexData index{};
    std::unordered_map<std::string, ProfileResultsData> results{};
    auto make() -> ProfileStorage {
        return {.with_exclusive_access =
                    [](std::function<bool()> const& operation) { return operation(); },
                .load_index =
                    [this](ProfileIndexData& output) {
                        if (index_load_fails) {
                            return LoadResult::failed;
                        }
                        if (!index_exists) {
                            return LoadResult::not_found;
                        }
                        output = index;
                        return LoadResult::succeeded;
                    },
                .save_index =
                    [this](ProfileIndexData const& value) {
                        ++index_save_count;
                        if (index_save_fails) {
                            return false;
                        }
                        index = value;
                        index_exists = true;
                        return true;
                    },
                .load_results =
                    [this](std::string const& id, ProfileResultsData& output) {
                        auto found{results.find(id)};
                        if (found == results.end()) {
                            return LoadResult::not_found;
                        }
                        output = found->second;
                        return LoadResult::succeeded;
                    },
                .save_results =
                    [this](std::string const& id, ProfileResultsData const& value) {
                        if (results_save_fails) {
                            return false;
                        }
                        results.insert_or_assign(id, value);
                        return true;
                    }};
    }
};
namespace save_profile_manager_test {
auto make_record(Timestamp date, std::string level, std::int32_t kills) -> ScoreRecord {
    return {.date = date,
            .level_name = std::move(level),
            .outcome = LevelOutcome::Victory,
            .kills = kills,
            .time_seconds = 60.f};
}
}
void check_debug_settings(ProfileManager const& manager,
                          ProfileDebugSettings expected,
                          char const* description) {
    EXPECT_EQ(manager.unlock_all_missions(), expected.unlock_all_missions) << description;
    EXPECT_EQ(manager.start_levels_paused(), expected.start_levels_paused) << description;
}

TEST(ProfileManager, CreatesEmptyDefaultProfile) {
    FakeProfileStorage storage;
    ProfileManager manager{storage.make()};
    ASSERT_TRUE(manager.initialise());
    ASSERT_EQ(manager.profiles().size(), 1u);
    EXPECT_EQ(manager.profiles().front().profile_id, manager.active_profile_id());
    EXPECT_TRUE(manager.active_records().empty());
}
TEST(ProfileManager, CreatesAndActivatesUniquelyNamedProfiles) {

    FakeProfileStorage storage{};
    ProfileManager manager{storage.make()};
    EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";

    auto const empty{manager.create_profile("   ")};
    EXPECT_EQ(empty.result, CreateProfileResult::empty_name) << "Whitespace-only name is rejected";

    auto const created{manager.create_profile("  Commander  ")};
    EXPECT_EQ(created.result, CreateProfileResult::succeeded) << "Profile is created";
    EXPECT_EQ(manager.active_profile_id(), created.profile_id) << "New profile is activated";

    auto const duplicate{manager.create_profile("commander")};
    EXPECT_EQ(duplicate.result, CreateProfileResult::duplicate_name)
        << "Names are unique without regard to case";
}

TEST(ProfileManager, RoutesResultsAndResetsOnlyTheTestProfile) {

    FakeProfileStorage storage{};
    ProfileManager manager{storage.make()};
    EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";
    auto const default_profile_id{manager.active_profile_id()};
    auto const default_record{
        save_profile_manager_test::make_record(test_date(2026, 8, 20), "DefaultBattle", 3)};
    EXPECT_TRUE(manager.append_score_record(default_record)) << "Default result is appended";

    std::vector<ScoreRecord> const fixture{
        save_profile_manager_test::make_record(test_date(2026, 8, 21), "TestBattleOne", 5),
        save_profile_manager_test::make_record(test_date(2026, 8, 22), "TestBattleTwo", 7),
    };
    EXPECT_TRUE(manager.reset_test_profile(fixture)) << "Test profile is reset";
    auto const test_profile_id{manager.active_profile_id()};
    EXPECT_TRUE(test_profile_id != default_profile_id) << "Test profile differs from default";

    std::vector<ScoreRecord> default_records{};
    std::vector<ScoreRecord> test_records{};
    EXPECT_TRUE(manager.load_profile_records(default_profile_id, default_records))
        << "Default profile still loads";
    EXPECT_TRUE(manager.load_profile_records(test_profile_id, test_records))
        << "Test profile loads";
    EXPECT_EQ(default_records.size(), 1) << "Default outcome is preserved";
    EXPECT_EQ(test_records.size(), 2) << "Fixture outcomes replace test results";

    EXPECT_TRUE(manager.reset_test_profile(fixture)) << "Repeated reset succeeds";
    EXPECT_TRUE(manager.active_profile_id() == test_profile_id)
        << "Reset test profile remains active";
    EXPECT_EQ(manager.active_records().size(), 2) << "Reset does not duplicate fixture outcomes";

    storage.results.erase(default_profile_id);
    EXPECT_FALSE(manager.activate_profile(default_profile_id))
        << "Activation fails when profile results are unavailable";
    EXPECT_EQ(manager.active_profile_id(), test_profile_id)
        << "Failed activation preserves the active profile";

    storage.results.insert_or_assign(default_profile_id,
                                     ProfileResultsData{.score_records = {default_record}});
    EXPECT_TRUE(manager.activate_profile(default_profile_id))
        << "Available profile can be activated";
    EXPECT_EQ(manager.active_profile_id(), default_profile_id)
        << "Activation switches the active profile";
}

TEST(ProfileManager, FailedCompletionSaveDoesNotClaimPersistence) {

    FakeProfileStorage storage{};
    ProfileManager manager{storage.make()};
    EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";

    storage.results_save_fails = true;
    auto const record{
        save_profile_manager_test::make_record(test_date(2026, 9, 4), "border-skirmish", 6)};
    auto const failed{manager.append_score_record(record)};
    EXPECT_FALSE(failed.persisted) << "Failed storage reports that completion was not persisted";
    EXPECT_FALSE(failed.new_best_time) << "Unsaved results cannot claim a new best";
    EXPECT_EQ(manager.active_records().size(), 0) << "Failed completion is rolled back in memory";
}

TEST(ProfileManager, LevelCompletionPersistsAcrossManagerReload) {

    FakeProfileStorage storage{};
    std::string profile_id;
    {
        ProfileManager manager{storage.make()};
        EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";
        profile_id = manager.active_profile_id();
        auto const record{
            save_profile_manager_test::make_record(test_date(2026, 9, 5), "asteroid-field", 4)};
        EXPECT_TRUE(manager.append_score_record(record)) << "Completion is saved";
    }

    ProfileManager reloaded{storage.make()};
    EXPECT_TRUE(reloaded.initialise()) << "Profile manager reloads";
    EXPECT_EQ(reloaded.active_profile_id(), profile_id) << "Active profile is restored";
    auto const progress{summarize_level_progress(levels::LevelId{std::string{"asteroid-field"}},
                                                 reloaded.active_records())};
    EXPECT_TRUE(progress.state() == LevelProgressState::Completed)
        << "Reloaded level remains completed";
}

TEST(ProfileManager, ConcurrentManagersRebaseBeforeAppending) {

    FakeProfileStorage storage{};
    ProfileManager first{storage.make()};
    ProfileManager second{storage.make()};
    EXPECT_TRUE(first.initialise()) << "First manager initialises";
    EXPECT_TRUE(second.initialise()) << "Second manager initialises";

    auto const first_record{
        save_profile_manager_test::make_record(test_date(2026, 9, 10), "first-editor", 2)};
    auto const second_record{
        save_profile_manager_test::make_record(test_date(2026, 9, 11), "second-editor", 3)};
    auto const first_save{first.append_score_record(first_record)};
    ASSERT_TRUE(first_save);
    EXPECT_TRUE(first_save.new_best_time);
    auto const second_save{second.append_score_record(second_record)};
    ASSERT_TRUE(second_save);
    EXPECT_TRUE(second_save.new_best_time);

    auto faster{first_record};
    faster.time_seconds = 50.f;
    auto const faster_save{first.append_score_record(faster)};
    ASSERT_TRUE(faster_save);
    EXPECT_TRUE(faster_save.new_best_time);
    auto stale_attempt{first_record};
    stale_attempt.time_seconds = 55.f;
    auto const stale_save{second.append_score_record(stale_attempt)};
    ASSERT_TRUE(stale_save);
    EXPECT_FALSE(stale_save.new_best_time)
        << "A stale manager compares with the latest successful time";

    ProfileManager reloaded{storage.make()};
    EXPECT_TRUE(reloaded.initialise()) << "Combined state reloads";
    auto const records{reloaded.active_records()};
    ASSERT_EQ(records.size(), 4u);
    EXPECT_EQ(records[0], first_record);
    EXPECT_EQ(records[1], second_record);
    EXPECT_EQ(records[2], faster);
    EXPECT_EQ(records[3], stale_attempt);
}

TEST(ProfileManager, ConcurrentManagersPreserveNewProfiles) {

    FakeProfileStorage storage{};
    ProfileManager first{storage.make()};
    ProfileManager second{storage.make()};
    EXPECT_TRUE(first.initialise()) << "First manager initialises";
    EXPECT_TRUE(second.initialise()) << "Second manager initialises";

    EXPECT_EQ(first.create_profile("Alpha").result, CreateProfileResult::succeeded)
        << "First profile is created";
    EXPECT_EQ(second.create_profile("Bravo").result, CreateProfileResult::succeeded)
        << "Second profile is created from stale state";

    ProfileManager reloaded{storage.make()};
    EXPECT_TRUE(reloaded.initialise()) << "Combined profiles reload";
    EXPECT_EQ(reloaded.profiles().size(), 3) << "Default and both new profiles remain";
}

TEST(ProfileManager, DebugSettingsPersistPerProfileAndSurviveReload) {

    FakeProfileStorage storage{};
    std::string first_profile_id;
    std::string second_profile_id;
    {
        ProfileManager manager{storage.make()};
        EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";
        first_profile_id = manager.active_profile_id();
        check_debug_settings(manager, {}, "New profile defaults");
        EXPECT_TRUE(manager.set_unlock_all_missions(true)) << "Debug setting saves";
        EXPECT_TRUE(manager.set_start_levels_paused(true)) << "Paused launch setting saves";
        auto const record{
            save_profile_manager_test::make_record(test_date(2026, 9, 6), "asteroid-field", 3)};
        EXPECT_TRUE(manager.append_score_record(record))
            << "Score record saves with debug setting enabled";
        check_debug_settings(manager, {true, true}, "Metadata updates preserve settings");

        auto const created{manager.create_profile("Second")};
        second_profile_id = created.profile_id;
        EXPECT_EQ(created.result, CreateProfileResult::succeeded) << "Second profile is created";
        check_debug_settings(manager, {}, "Second profile has independent defaults");
        EXPECT_TRUE(manager.activate_profile(first_profile_id)) << "First profile reactivates";
        check_debug_settings(manager, {true, true}, "First profile settings are restored");
    }

    ProfileManager reloaded{storage.make()};
    EXPECT_TRUE(reloaded.initialise()) << "Profile manager reloads";
    check_debug_settings(reloaded, {true, true}, "Active profile settings survive reload");
    EXPECT_TRUE(reloaded.activate_profile(second_profile_id)) << "Second profile activates";
    check_debug_settings(reloaded, {}, "Second profile remains independent");
}

TEST(ProfileManager, TestProfileResetRestoresDebugDefaults) {

    FakeProfileStorage storage{};
    ProfileManager manager{storage.make()};
    EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";
    EXPECT_TRUE(manager.reset_test_profile({})) << "Test profile is created";
    EXPECT_TRUE(manager.set_unlock_all_missions(true)) << "Test profile debug setting saves";
    EXPECT_TRUE(manager.set_start_levels_paused(true)) << "Test profile paused launch saves";
    check_debug_settings(manager, {true, true}, "Test profile settings are enabled");

    EXPECT_TRUE(manager.reset_test_profile({})) << "Test profile resets again";
    check_debug_settings(manager, {}, "Reset test profile restores defaults");
}

TEST(ProfileManager, FailedDebugSettingsSaveRollsBack) {

    FakeProfileStorage storage{};
    ProfileManager manager{storage.make()};
    EXPECT_TRUE(manager.initialise()) << "Profile manager initialises";

    storage.index_save_fails = true;
    EXPECT_FALSE(manager.set_unlock_all_missions(true)) << "Failed settings save is reported";
    EXPECT_FALSE(manager.unlock_all_missions()) << "Failed settings save is rolled back";

    EXPECT_FALSE(manager.set_start_levels_paused(true)) << "Failed paused launch save is reported";
    EXPECT_FALSE(manager.start_levels_paused()) << "Failed paused launch save is rolled back";
}

TEST(ProfileManager, RejectsUnsupportedProfileVersions) {
    for (std::int32_t version{}; version <= ProfileIndexData::current_save_version + 1; ++version) {
        if (version == ProfileIndexData::current_save_version) {
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
        ProfileManager manager{storage.make()};
        EXPECT_FALSE(manager.initialise());
        EXPECT_EQ(storage.index_save_count, 0) << "Rejected index is not rewritten";
        EXPECT_FALSE(manager.has_active_profile());
        EXPECT_TRUE(manager.profiles().empty());
    }

    for (std::int32_t version{}; version <= ProfileResultsData::current_save_version + 1;
         ++version) {
        if (version == ProfileResultsData::current_save_version) {
            continue;
        }
        SCOPED_TRACE(version);
        FakeProfileStorage storage{
            .index_exists = true,
            .index = {.active_profile_id = "unsupported",
                      .profiles = {{.profile_id = "unsupported", .display_name = "Unsupported"}}},
            .results = {{"unsupported", ProfileResultsData{.save_version = version}}},
        };
        ProfileManager manager{storage.make()};
        EXPECT_FALSE(manager.initialise());
        EXPECT_EQ(storage.index_save_count, 0) << "Rejected results do not rewrite the index";
        EXPECT_FALSE(manager.has_active_profile());
        EXPECT_TRUE(manager.profiles().empty());
    }
}

TEST(ProfileManager, RejectsMissingResultsReferencedByAnExistingIndex) {

    FakeProfileStorage storage{
        .index_exists = true,
        .index = {.active_profile_id = "missing",
                  .profiles = {{.profile_id = "missing", .display_name = "Missing"}}},
    };
    ProfileManager manager{storage.make()};

    EXPECT_FALSE(manager.initialise()) << "Missing results fail closed";
    EXPECT_FALSE(manager.has_active_profile());
    EXPECT_TRUE(manager.active_profile_id().empty());
    EXPECT_TRUE(manager.profiles().empty());
    EXPECT_FALSE(manager.set_unlock_all_missions(true));
    EXPECT_FALSE(storage.results.contains("missing"))
        << "Missing results are not replaced with an empty file";
}

}
