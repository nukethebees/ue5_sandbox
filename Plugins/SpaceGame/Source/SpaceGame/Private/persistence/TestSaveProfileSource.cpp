#include "TestSaveProfileSource.h"

#include "SpaceGame/persistence/SpaceSaveGame.h"

namespace ml::ioj::detail {
auto make_test_profile_records() -> TArray<FScoreRecord> {
    return {
        {.date = FDateTime{2026, 8, 20, 19, 5},
         .level_name = TEXT("VegaPatrol"),
         .mission_mode = ETestMissionMode::KillEnemies,
         .end_state = ETestMissionState::Succeeded,
         .kills = 18,
         .time_seconds = 3180.f,
         .target_kills = 18},
        {.date = FDateTime{2026, 8, 24, 21, 15},
         .level_name = TEXT("ConvoyEscort"),
         .mission_mode = ETestMissionMode::SurviveTime,
         .end_state = ETestMissionState::Succeeded,
         .kills = 24,
         .time_seconds = 4946.f,
         .target_completion_time = 4800.f},
        {.date = FDateTime{2026, 8, 25, 9, 5},
         .level_name = TEXT("FleetInterception"),
         .mission_mode = ETestMissionMode::KillEnemiesWithinTime,
         .end_state = ETestMissionState::Failed,
         .fail_reason = ETestMissionFailReason::TimeElapsed,
         .kills = 7,
         .time_seconds = 972.f,
         .target_kills = 12,
         .target_completion_time = 900.f},
        {.date = FDateTime{2026, 8, 27, 20, 30},
         .level_name = TEXT("VegaDefence"),
         .mission_mode = ETestMissionMode::KillEnemiesWithinTime,
         .end_state = ETestMissionState::Succeeded,
         .kills = 22,
         .time_seconds = 4354.f,
         .target_kills = 20,
         .target_completion_time = 4500.f},
    };
}
}
