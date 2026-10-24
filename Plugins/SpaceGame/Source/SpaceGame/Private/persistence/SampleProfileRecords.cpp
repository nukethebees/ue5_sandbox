#include "SampleProfileRecords.h"

#include "SpaceGame/persistence/SpaceSaveGame.h"

namespace ioj::detail {
auto make_sample_profile_records() -> TArray<FScoreRecord> {
    return {
        {.date = FDateTime{2026, 8, 20, 19, 5},
         .level_name = TEXT("VegaPatrol"),
         .outcome = ETeamOutcome::Victory,
         .kills = 18,
         .time_seconds = 3180.f},
        {.date = FDateTime{2026, 8, 24, 21, 15},
         .level_name = TEXT("ConvoyEscort"),
         .outcome = ETeamOutcome::Victory,
         .kills = 24,
         .time_seconds = 4946.f},
        {.date = FDateTime{2026, 8, 25, 9, 5},
         .level_name = TEXT("FleetInterception"),
         .outcome = ETeamOutcome::Defeat,
         .kills = 7,
         .time_seconds = 972.f},
        {.date = FDateTime{2026, 8, 27, 20, 30},
         .level_name = TEXT("VegaDefence"),
         .outcome = ETeamOutcome::Victory,
         .kills = 22,
         .time_seconds = 4354.f},
    };
}
}
