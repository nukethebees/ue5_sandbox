#include <SpaceGame/persistence/SaveProfileManager.h>

#include <CQTest.h>

namespace save_profile_manager_test {
struct FFakeProfileStorage {
    bool index_exists{};
    bool index_load_fails{};
    bool index_save_fails{};
    bool legacy_exists{};
    FSaveProfileIndexData index{};
    TMap<FString, FSaveProfileResultsData> results{};
    TArray<FScoreRecord> legacy_records{};

    auto make() -> ml::ioj::FSaveProfileStorage {
        return {
            .load_index =
                [this](FSaveProfileIndexData& output) {
                    if (index_load_fails) {
                        return ml::ioj::ESaveProfileLoadResult::failed;
                    }
                    if (!index_exists) {
                        return ml::ioj::ESaveProfileLoadResult::not_found;
                    }
                    output = index;
                    return ml::ioj::ESaveProfileLoadResult::succeeded;
                },
            .save_index =
                [this](FSaveProfileIndexData const& value) {
                    if (index_save_fails) {
                        return false;
                    }
                    index = value;
                    index_exists = true;
                    return true;
                },
            .load_results =
                [this](FString const& profile_id, FSaveProfileResultsData& output) {
                    auto const* const found{results.Find(profile_id)};
                    if (!found) {
                        return ml::ioj::ESaveProfileLoadResult::not_found;
                    }
                    output = *found;
                    return ml::ioj::ESaveProfileLoadResult::succeeded;
                },
            .save_results =
                [this](FString const& profile_id, FSaveProfileResultsData const& value) {
                    results.Add(profile_id, value);
                    return true;
                },
            .load_legacy_results =
                [this](TArray<FScoreRecord>& output) {
                    if (!legacy_exists) {
                        return ml::ioj::ESaveProfileLoadResult::not_found;
                    }
                    output = legacy_records;
                    return ml::ioj::ESaveProfileLoadResult::succeeded;
                },
        };
    }
};

auto make_record(FDateTime const date, FName const level, int32 const kills) -> FScoreRecord {
    return {.date = date,
            .level_name = level,
            .mission_mode = ETestMissionMode::KillEnemies,
            .end_state = ETestMissionState::Succeeded,
            .kills = kills,
            .time_seconds = 60.f,
            .target_kills = kills};
}
}

TEST_CLASS(SaveProfileManager, "Sandbox.UnitTests")
{
    TEST_METHOD(CreatesDefaultProfileAndImportsLegacyResults)
    {
        save_profile_manager_test::FFakeProfileStorage storage{
            .legacy_exists = true,
            .legacy_records = {save_profile_manager_test::make_record(
                FDateTime{2026, 8, 20}, TEXT("LegacyBattle"), 4)},
        };
        auto const original_legacy{storage.legacy_records};
        ml::ioj::FSaveProfileManager manager{storage.make()};

        TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());
        auto const profiles{manager.get_profiles()};
        TestRunner->TestEqual(TEXT("One default profile is created"), profiles.Num(), 1);
        TestRunner->TestEqual(TEXT("Legacy result is imported"), profiles[0].outcome_count, 1);
        TestRunner->TestEqual(TEXT("Imported profile is active"),
                              manager.get_active_profile_id(),
                              profiles[0].profile_id);
        TestRunner->TestEqual(TEXT("Legacy source remains unchanged"),
                              storage.legacy_records.Num(),
                              original_legacy.Num());
    }

    TEST_METHOD(CreatesAndActivatesUniquelyNamedProfiles)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        ml::ioj::FSaveProfileManager manager{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());

        auto const empty{manager.create_profile(TEXT("   "))};
        TestRunner->TestEqual(TEXT("Whitespace-only name is rejected"),
                              empty.result,
                              ml::ioj::ECreateSaveProfileResult::empty_name);

        auto const created{manager.create_profile(TEXT("  Commander  "))};
        TestRunner->TestEqual(TEXT("Profile is created"),
                              created.result,
                              ml::ioj::ECreateSaveProfileResult::succeeded);
        TestRunner->TestEqual(
            TEXT("New profile is activated"), manager.get_active_profile_id(), created.profile_id);

        auto const duplicate{manager.create_profile(TEXT("commander"))};
        TestRunner->TestEqual(TEXT("Names are unique without regard to case"),
                              duplicate.result,
                              ml::ioj::ECreateSaveProfileResult::duplicate_name);
    }

    TEST_METHOD(RoutesResultsAndResetsOnlyTheTestProfile)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        ml::ioj::FSaveProfileManager manager{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());
        auto const default_profile_id{manager.get_active_profile_id()};
        auto const default_record{save_profile_manager_test::make_record(
            FDateTime{2026, 8, 20}, TEXT("DefaultBattle"), 3)};
        TestRunner->TestTrue(TEXT("Default result is appended"),
                             manager.append_score_record(default_record));

        TArray<FScoreRecord> const fixture{
            save_profile_manager_test::make_record(
                FDateTime{2026, 8, 21}, TEXT("TestBattleOne"), 5),
            save_profile_manager_test::make_record(
                FDateTime{2026, 8, 22}, TEXT("TestBattleTwo"), 7),
        };
        TestRunner->TestTrue(TEXT("Test profile is reset"), manager.reset_test_profile(fixture));
        auto const test_profile_id{manager.get_active_profile_id()};
        TestRunner->TestTrue(TEXT("Test profile differs from default"),
                             test_profile_id != default_profile_id);

        TArray<FScoreRecord> default_records{};
        TArray<FScoreRecord> test_records{};
        TestRunner->TestTrue(TEXT("Default profile still loads"),
                             manager.load_profile_records(default_profile_id, default_records));
        TestRunner->TestTrue(TEXT("Test profile loads"),
                             manager.load_profile_records(test_profile_id, test_records));
        TestRunner->TestEqual(TEXT("Default outcome is preserved"), default_records.Num(), 1);
        TestRunner->TestEqual(TEXT("Fixture outcomes replace test results"), test_records.Num(), 2);

        TestRunner->TestTrue(TEXT("Repeated reset succeeds"), manager.reset_test_profile(fixture));
        TestRunner->TestTrue(TEXT("Reset test profile remains active"),
                             manager.get_active_profile_id() == test_profile_id);
        TestRunner->TestEqual(TEXT("Reset does not duplicate fixture outcomes"),
                              manager.get_active_records().Num(),
                              2);

        storage.results.Remove(default_profile_id);
        TestRunner->TestFalse(TEXT("Activation fails when profile results are unavailable"),
                              manager.activate_profile(default_profile_id));
        TestRunner->TestEqual(TEXT("Failed activation preserves the active profile"),
                              manager.get_active_profile_id(),
                              test_profile_id);

        storage.results.Add(default_profile_id,
                            FSaveProfileResultsData{.score_records = {default_record}});
        TestRunner->TestTrue(TEXT("Available profile can be activated"),
                             manager.activate_profile(default_profile_id));
        TestRunner->TestEqual(TEXT("Activation switches the active profile"),
                              manager.get_active_profile_id(),
                              default_profile_id);
    }
};
