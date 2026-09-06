#include <SpaceGame/persistence/SaveProfileManager.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>

#include <CQTest.h>
#include <Kismet/GameplayStatics.h>

namespace save_profile_manager_test {
struct FFakeProfileStorage {
    bool index_exists{};
    bool index_load_fails{};
    bool index_save_fails{};
    bool results_save_fails{};
    bool legacy_exists{};
    int32 index_save_count{};
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
                    ++index_save_count;
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
                    if (results_save_fails) {
                        return false;
                    }
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

    TEST_METHOD(FailedCompletionSaveDoesNotClaimPersistence)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        ml::ioj::FSaveProfileManager manager{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());

        storage.results_save_fails = true;
        auto const record{save_profile_manager_test::make_record(
            FDateTime{2026, 9, 4}, TEXT("border-skirmish"), 6)};
        TestRunner->TestFalse(TEXT("Failed storage reports that completion was not persisted"),
                              manager.append_score_record(record));
        TestRunner->TestEqual(TEXT("Failed completion is rolled back in memory"),
                              manager.get_active_records().Num(),
                              0);
    }

    TEST_METHOD(LevelCompletionPersistsAcrossManagerReload)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        FString profile_id;
        {
            ml::ioj::FSaveProfileManager manager{storage.make()};
            TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());
            profile_id = manager.get_active_profile_id();
            auto const record{save_profile_manager_test::make_record(
                FDateTime{2026, 9, 5}, TEXT("asteroid-field"), 4)};
            TestRunner->TestTrue(TEXT("Completion is saved"), manager.append_score_record(record));
        }

        ml::ioj::FSaveProfileManager reloaded{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager reloads"), reloaded.initialise());
        TestRunner->TestEqual(
            TEXT("Active profile is restored"), reloaded.get_active_profile_id(), profile_id);
        auto const progress{ml::ioj::summarize_level_progress(
            ml::FLevelId{FName{TEXT("asteroid-field")}}, reloaded.get_active_records())};
        TestRunner->TestTrue(TEXT("Reloaded level remains completed"),
                             progress.state == ml::ioj::ELevelProgressState::Completed);
    }

    TEST_METHOD(MigratesProfileIndexAndPreservesExistingData)
    {
        auto const record{save_profile_manager_test::make_record(
            FDateTime{2026, 9, 5}, TEXT("asteroid-field"), 4)};
        save_profile_manager_test::FFakeProfileStorage storage{
            .index_exists = true,
            .index = {.save_version = 1,
                      .active_profile_id = TEXT("alpha"),
                      .profiles = {{.profile_id = TEXT("alpha"),
                                    .display_name = TEXT("Alpha"),
                                    .created_at = FDateTime{2026, 8, 1},
                                    .last_played_at = record.date,
                                    .total_simulation_duration_seconds = record.time_seconds,
                                    .total_kills = record.kills,
                                    .outcome_count = 1,
                                    .debug_settings = {.unlock_all_missions = true}},
                                   {.profile_id = TEXT("bravo"),
                                    .display_name = TEXT("Bravo"),
                                    .created_at = FDateTime{2026, 8, 2}}}},
            .results = {{TEXT("alpha"), FSaveProfileResultsData{.score_records = {record}}}},
        };
        ml::ioj::FSaveProfileManager manager{storage.make()};

        TestRunner->TestTrue(TEXT("Version one profile index migrates"), manager.initialise());
        TestRunner->TestEqual(TEXT("Profile index is upgraded"),
                              storage.index.save_version,
                              FSaveProfileIndexData::current_save_version);
        TestRunner->TestEqual(TEXT("Active profile is preserved"),
                              manager.get_active_profile_id(),
                              FString{TEXT("alpha")});
        auto const profiles{manager.get_profiles()};
        TestRunner->TestEqual(TEXT("All profiles are preserved"), profiles.Num(), 2);
        TestRunner->TestEqual(
            TEXT("Profile name is preserved"), profiles[0].display_name, FString{TEXT("Alpha")});
        TestRunner->TestEqual(TEXT("Outcome count is preserved"), profiles[0].outcome_count, 1);
        TestRunner->TestFalse(TEXT("Migrated debug setting defaults off"),
                              manager.unlock_all_missions());
        TestRunner->TestFalse(TEXT("Every migrated profile defaults off"),
                              profiles[1].debug_settings.unlock_all_missions);
        TestRunner->TestFalse(TEXT("Start-paused setting defaults off"),
                              manager.start_levels_paused());
        TestRunner->TestEqual(
            TEXT("Historical result is preserved"), manager.get_active_records().Num(), 1);

        save_profile_manager_test::FFakeProfileStorage version_two_storage{
            .index_exists = true,
            .index = {.save_version = 2,
                      .active_profile_id = TEXT("alpha"),
                      .profiles = {{.profile_id = TEXT("alpha"),
                                    .display_name = TEXT("Alpha"),
                                    .debug_settings = {.unlock_all_missions = true,
                                                       .start_levels_paused = true}}}},
            .results = {{TEXT("alpha"), FSaveProfileResultsData{}}},
        };
        ml::ioj::FSaveProfileManager version_two_manager{version_two_storage.make()};
        TestRunner->TestTrue(TEXT("Version two profile index migrates"),
                             version_two_manager.initialise());
        TestRunner->TestTrue(TEXT("Existing debug settings are preserved"),
                             version_two_manager.unlock_all_missions());
        TestRunner->TestFalse(TEXT("New debug setting is initialized off"),
                              version_two_manager.start_levels_paused());
    }

    TEST_METHOD(DebugSettingsPersistPerProfileAndSurviveReload)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        FString first_profile_id;
        FString second_profile_id;
        {
            ml::ioj::FSaveProfileManager manager{storage.make()};
            TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());
            first_profile_id = manager.get_active_profile_id();
            TestRunner->TestFalse(TEXT("New profile defaults off"), manager.unlock_all_missions());
            TestRunner->TestFalse(TEXT("Paused launch defaults off"),
                                  manager.start_levels_paused());
            TestRunner->TestTrue(TEXT("Debug setting saves"),
                                 manager.set_unlock_all_missions(true));
            TestRunner->TestTrue(TEXT("Paused launch setting saves"),
                                 manager.set_start_levels_paused(true));
            auto const record{save_profile_manager_test::make_record(
                FDateTime{2026, 9, 6}, TEXT("asteroid-field"), 3)};
            TestRunner->TestTrue(TEXT("Score record saves with debug setting enabled"),
                                 manager.append_score_record(record));
            TestRunner->TestTrue(TEXT("Metadata updates preserve debug settings"),
                                 manager.unlock_all_missions());
            TestRunner->TestTrue(TEXT("Metadata updates preserve paused launch"),
                                 manager.start_levels_paused());

            auto const created{manager.create_profile(TEXT("Second"))};
            second_profile_id = created.profile_id;
            TestRunner->TestEqual(TEXT("Second profile is created"),
                                  created.result,
                                  ml::ioj::ECreateSaveProfileResult::succeeded);
            TestRunner->TestFalse(TEXT("Second profile has independent default"),
                                  manager.unlock_all_missions());
            TestRunner->TestFalse(TEXT("Second profile paused launch defaults off"),
                                  manager.start_levels_paused());
            TestRunner->TestTrue(TEXT("First profile reactivates"),
                                 manager.activate_profile(first_profile_id));
            TestRunner->TestTrue(TEXT("First profile setting is restored"),
                                 manager.unlock_all_missions());
            TestRunner->TestTrue(TEXT("First profile paused launch is restored"),
                                 manager.start_levels_paused());
        }

        ml::ioj::FSaveProfileManager reloaded{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager reloads"), reloaded.initialise());
        TestRunner->TestTrue(TEXT("Active profile setting survives reload"),
                             reloaded.unlock_all_missions());
        TestRunner->TestTrue(TEXT("Paused launch survives reload"), reloaded.start_levels_paused());
        TestRunner->TestTrue(TEXT("Second profile activates"),
                             reloaded.activate_profile(second_profile_id));
        TestRunner->TestFalse(TEXT("Second profile remains independent"),
                              reloaded.unlock_all_missions());
        TestRunner->TestFalse(TEXT("Second profile paused launch remains independent"),
                              reloaded.start_levels_paused());
    }

    TEST_METHOD(TestProfileResetRestoresDebugDefaults)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        ml::ioj::FSaveProfileManager manager{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());
        TestRunner->TestTrue(TEXT("Test profile is created"), manager.reset_test_profile({}));
        TestRunner->TestTrue(TEXT("Test profile debug setting saves"),
                             manager.set_unlock_all_missions(true));
        TestRunner->TestTrue(TEXT("Test profile paused launch saves"),
                             manager.set_start_levels_paused(true));
        TestRunner->TestTrue(TEXT("Test profile debug setting is enabled"),
                             manager.unlock_all_missions());
        TestRunner->TestTrue(TEXT("Test profile paused launch is enabled"),
                             manager.start_levels_paused());

        TestRunner->TestTrue(TEXT("Test profile resets again"), manager.reset_test_profile({}));
        TestRunner->TestFalse(TEXT("Reset test profile restores debug default"),
                              manager.unlock_all_missions());
        TestRunner->TestFalse(TEXT("Reset test profile restores paused launch default"),
                              manager.start_levels_paused());
    }

    TEST_METHOD(FailedDebugSettingsSaveRollsBack)
    {
        save_profile_manager_test::FFakeProfileStorage storage{};
        ml::ioj::FSaveProfileManager manager{storage.make()};
        TestRunner->TestTrue(TEXT("Profile manager initialises"), manager.initialise());

        storage.index_save_fails = true;
        TestRunner->TestFalse(TEXT("Failed settings save is reported"),
                              manager.set_unlock_all_missions(true));
        TestRunner->TestFalse(TEXT("Failed settings save is rolled back"),
                              manager.unlock_all_missions());

        TestRunner->TestFalse(TEXT("Failed paused launch save is reported"),
                              manager.set_start_levels_paused(true));
        TestRunner->TestFalse(TEXT("Failed paused launch save is rolled back"),
                              manager.start_levels_paused());
    }

    TEST_METHOD(RejectsUnsupportedProfileVersions)
    {
        save_profile_manager_test::FFakeProfileStorage future_index{
            .index_exists = true,
            .index = {.save_version = FSaveProfileIndexData::current_save_version + 1,
                      .active_profile_id = TEXT("future"),
                      .profiles = {{.profile_id = TEXT("future"), .display_name = TEXT("Future")}}},
            .results = {{TEXT("future"), FSaveProfileResultsData{}}},
        };
        ml::ioj::FSaveProfileManager index_manager{future_index.make()};
        TestRunner->TestFalse(TEXT("Future index version is rejected"), index_manager.initialise());
        TestRunner->TestEqual(
            TEXT("Rejected index is not rewritten"), future_index.index_save_count, 0);

        save_profile_manager_test::FFakeProfileStorage future_results{
            .index_exists = true,
            .index = {.active_profile_id = TEXT("future"),
                      .profiles = {{.profile_id = TEXT("future"), .display_name = TEXT("Future")}}},
            .results = {{TEXT("future"),
                         FSaveProfileResultsData{
                             .save_version = FSaveProfileResultsData::current_save_version + 1}}},
        };
        ml::ioj::FSaveProfileManager results_manager{future_results.make()};
        TestRunner->TestFalse(TEXT("Future results version is rejected"),
                              results_manager.initialise());
        TestRunner->TestEqual(
            TEXT("Rejected results do not rewrite the index"), future_results.index_save_count, 0);
    }

    TEST_METHOD(ProfileDebugSettingsRoundTripThroughUnrealSerialization)
    {
        auto* const save{NewObject<USpaceSaveProfileIndexSaveGame>()};
        if (!TestRunner->TestNotNull(TEXT("Profile index save object is created"), save)) {
            return;
        }
        save->data.active_profile_id = TEXT("enabled");
        save->data.profiles = {
            {.profile_id = TEXT("enabled"),
             .display_name = TEXT("Enabled"),
             .debug_settings = {.unlock_all_missions = true, .start_levels_paused = true}},
            {.profile_id = TEXT("disabled"),
             .display_name = TEXT("Disabled"),
             .debug_settings = {.unlock_all_missions = false}},
        };

        TArray<uint8> bytes;
        if (!TestRunner->TestTrue(TEXT("Profile index serializes"),
                                  UGameplayStatics::SaveGameToMemory(save, bytes))) {
            return;
        }
        auto* const loaded{
            Cast<USpaceSaveProfileIndexSaveGame>(UGameplayStatics::LoadGameFromMemory(bytes))};
        if (!TestRunner->TestNotNull(TEXT("Profile index deserializes"), loaded)) {
            return;
        }

        TestRunner->TestEqual(TEXT("Schema version round-trips"),
                              loaded->data.save_version,
                              FSaveProfileIndexData::current_save_version);
        TestRunner->TestEqual(TEXT("Both profiles round-trip"), loaded->data.profiles.Num(), 2);
        TestRunner->TestTrue(TEXT("Enabled profile setting round-trips"),
                             loaded->data.profiles[0].debug_settings.unlock_all_missions);
        TestRunner->TestTrue(TEXT("Enabled paused launch round-trips"),
                             loaded->data.profiles[0].debug_settings.start_levels_paused);
        TestRunner->TestFalse(TEXT("Disabled profile setting round-trips"),
                              loaded->data.profiles[1].debug_settings.unlock_all_missions);
        TestRunner->TestFalse(TEXT("Disabled paused launch round-trips"),
                              loaded->data.profiles[1].debug_settings.start_levels_paused);
    }
};
