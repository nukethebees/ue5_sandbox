#include <SpaceGame/persistence/SpaceSaveGame.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>

#include <CQTest.h>

TEST_CLASS(LevelProgress, "Sandbox.UnitTests")
{
    TEST_METHOD(SummarisesOnlyTheRequestedAuthoredLevel)
    {
        FName const level_id{TEXT("border-skirmish")};
        TArray<FScoreRecord> const records{
            {.date = FDateTime{2026, 8, 20},
             .level_name = level_id,
             .end_state = ETestMissionState::Failed,
             .kills = 12,
             .time_seconds = 90.0f},
            {.date = FDateTime{2026, 8, 21},
             .level_name = level_id,
             .end_state = ETestMissionState::Succeeded,
             .kills = 8,
             .time_seconds = 75.0f},
            {.date = FDateTime{2026, 8, 22},
             .level_name = level_id,
             .end_state = ETestMissionState::Succeeded,
             .kills = 10,
             .time_seconds = 60.0f},
            {.date = FDateTime{2026, 8, 23},
             .level_name = TEXT("another-level"),
             .end_state = ETestMissionState::Succeeded,
             .kills = 100,
             .time_seconds = 1.0f},
        };

        auto const summary{ml::ioj::summarize_level_progress(level_id, records)};
        TestRunner->TestTrue(TEXT("Completed level is reported"),
                             summary.state == ml::ioj::ELevelProgressState::Completed);
        TestRunner->TestEqual(TEXT("Every matching attempt is counted"), summary.attempt_count, 3);
        TestRunner->TestEqual(TEXT("Successful attempts are counted"), summary.completion_count, 2);
        TestRunner->TestEqual(TEXT("Best kill count is retained"), summary.best_kills, 12);
        TestRunner->TestEqual(
            TEXT("Fastest completion is retained"), summary.best_completion_time_seconds, 60.0f);
        TestRunner->TestTrue(TEXT("Latest matching attempt is retained"),
                             summary.last_played_at == FDateTime{2026, 8, 22});
    }

    TEST_METHOD(DistinguishesAttemptedAndUnknownLevels)
    {
        TArray<FScoreRecord> const records{{.level_name = TEXT("failed-level"),
                                            .end_state = ETestMissionState::Failed,
                                            .kills = 3}};

        auto const attempted{
            ml::ioj::summarize_level_progress(FName{TEXT("failed-level")}, records)};
        TestRunner->TestTrue(TEXT("Failed level is attempted"),
                             attempted.state == ml::ioj::ELevelProgressState::Attempted);
        TestRunner->TestEqual(
            TEXT("Failed level has no completions"), attempted.completion_count, 0);
        TestRunner->TestEqual(TEXT("Failed level has no completion time"),
                              attempted.best_completion_time_seconds,
                              -1.0f);

        auto const unknown{
            ml::ioj::summarize_level_progress(FName{TEXT("unknown-level")}, records)};
        TestRunner->TestTrue(TEXT("Unknown level is not attempted"),
                             unknown.state == ml::ioj::ELevelProgressState::NotAttempted);
        TestRunner->TestEqual(TEXT("Unknown level has no attempts"), unknown.attempt_count, 0);
    }
};
