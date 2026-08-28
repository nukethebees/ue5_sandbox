#include "MockSaveGameSummarySource.h"

namespace ml::ioj::detail {
auto discover_mock_save_game_summaries() -> TArray<FSaveGameSummary> {
#if UE_BUILD_SHIPPING
    return {};
#else
    return {
        {.save_id = TEXT("battle_at_vega"),
         .display_name = TEXT("Battle at Vega"),
         .timestamp = FDateTime{2026, 8, 27, 20, 30},
         .scenario_name = TEXT("Vega Defence"),
         .simulation_duration_seconds = 4354.f,
         .score = 8450,
         .result = TEXT("Victory")},
        {.save_id = TEXT("test_run_12"),
         .display_name = TEXT("Test Run 12"),
         .timestamp = FDateTime{2026, 8, 26, 14, 12},
         .scenario_name = TEXT("Fleet Interception"),
         .simulation_duration_seconds = 972.f,
         .score = 3200,
         .result = TEXT("Defeat")},
        {.save_id = TEXT("fleet_benchmark"),
         .display_name = TEXT("Fleet Benchmark"),
         .timestamp = FDateTime{2026, 8, 25, 9, 5},
         .scenario_name = TEXT("Capital Fleet Benchmark"),
         .simulation_duration_seconds = 1800.f,
         .score = 12750,
         .result = TEXT("Complete")},
    };
#endif
}
}
