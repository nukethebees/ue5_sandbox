#include <SpaceGame/persistence/SaveGameProfileAdapter.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>

#include <CQTest.h>

TEST_CLASS(SaveGameProfileAdapter, "Sandbox.UnitTests")
{
    TEST_METHOD(BuildsLightweightProfileSummary)
    {
        TArray<FScoreRecord> const records{
            {.date = FDateTime{2026, 4, 3}, .kills = 7, .time_seconds = 30.f},
            {.date = FDateTime{2026, 4, 1}, .kills = 5, .time_seconds = 12.5f},
        };

        auto const summary{
            ml::ioj::save_profile::make_summary(TEXT("profile"), TEXT("Pilot"), records)};

        TestRunner->TestEqual(
            TEXT("Profile ID remains intact"), summary.profile_id, FString{TEXT("profile")});
        TestRunner->TestEqual(
            TEXT("Display name remains intact"), summary.display_name, FString{TEXT("Pilot")});
        TestRunner->TestTrue(TEXT("Created date uses earliest result"),
                             summary.created_at == FDateTime{2026, 4, 1});
        TestRunner->TestTrue(TEXT("Last played date uses latest result"),
                             summary.last_played_at == FDateTime{2026, 4, 3});
        TestRunner->TestTrue(TEXT("Durations are accumulated"),
                             summary.total_simulation_duration_seconds == 42.5f);
        TestRunner->TestEqual(TEXT("Kills are accumulated"), summary.total_kills, 12);
        TestRunner->TestEqual(TEXT("Outcome count is retained"), summary.outcome_count, 2);
    }

    TEST_METHOD(AdaptsTypedMissionResultsIntoReportOutcomes)
    {
        TArray<FScoreRecord> const records{{.date = FDateTime{2026, 4, 3, 12, 30},
                                            .level_name = TEXT("test_level_42"),
                                            .mission_mode = ETestMissionMode::KillEnemiesWithinTime,
                                            .end_state = ETestMissionState::Failed,
                                            .fail_reason = ETestMissionFailReason::TimeElapsed,
                                            .kills = 8,
                                            .time_seconds = 91.f,
                                            .target_kills = 10,
                                            .target_completion_time = 90.f}};

        auto const report{ml::ioj::save_profile::make_report(TEXT("profile"), records)};

        TestRunner->TestEqual(TEXT("Report has one outcome"), report.outcomes.Num(), 1);
        auto const& outcome{report.outcomes[0]};
        TestRunner->TestEqual(TEXT("Level name is formatted for display"),
                              outcome.display_name,
                              FString{TEXT("Test Level 42")});
        TestRunner->TestEqual(TEXT("Kills remain typed"), outcome.kills, 8);
        TestRunner->TestEqual(
            TEXT("End state is displayed"), outcome.result, FString{TEXT("Failed")});
        TestRunner->TestEqual(
            TEXT("Mission details become display statistics"), outcome.statistics.Num(), 4);
        TestRunner->TestEqual(TEXT("Failure reason is included"),
                              outcome.statistics.Last().value,
                              FString{TEXT("TimeElapsed")});
    }
};
