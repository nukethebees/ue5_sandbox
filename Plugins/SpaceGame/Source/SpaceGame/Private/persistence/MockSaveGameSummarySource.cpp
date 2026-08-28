#include "MockSaveGameSummarySource.h"

#include <Containers/Map.h>

namespace ml::ioj::detail {
auto make_mock_save_game_browser() -> FSaveGameBrowser {
#if UE_BUILD_SHIPPING
    return {};
#else
    TArray<FSaveProfileSummary> summaries{
        {.profile_id = TEXT("battle_at_vega"),
         .display_name = TEXT("Battle at Vega"),
         .created_at = FDateTime{2026, 8, 20, 18, 10},
         .last_played_at = FDateTime{2026, 8, 27, 20, 30},
         .total_simulation_duration_seconds = 12480.f,
         .total_kills = 64,
         .outcome_count = 3},
        {.profile_id = TEXT("test_run_12"),
         .display_name = TEXT("Test Run 12"),
         .created_at = FDateTime{2026, 8, 26, 13, 30},
         .last_played_at = FDateTime{2026, 8, 26, 14, 12},
         .total_simulation_duration_seconds = 972.f,
         .total_kills = 7,
         .outcome_count = 1},
        {.profile_id = TEXT("fleet_benchmark"),
         .display_name = TEXT("Fleet Benchmark"),
         .created_at = FDateTime{2026, 8, 25, 8, 30},
         .last_played_at = FDateTime{2026, 8, 25, 9, 5},
         .total_simulation_duration_seconds = 1800.f,
         .total_kills = 112,
         .outcome_count = 1},
    };
    TMap<FString, FSaveProfileReport> reports{
        {TEXT("battle_at_vega"),
         {.profile_id = TEXT("battle_at_vega"),
          .outcomes = {{.outcome_id = TEXT("vega_patrol"),
                        .display_name = TEXT("Vega Patrol"),
                        .completed_at = FDateTime{2026, 8, 20, 19, 5},
                        .simulation_duration_seconds = 3180.f,
                        .kills = 18,
                        .result = TEXT("Succeeded"),
                        .statistics = {{TEXT("Friendly ships lost"), TEXT("2")},
                                       {TEXT("Accuracy"), TEXT("73%")}}},
                       {.outcome_id = TEXT("convoy_escort"),
                        .display_name = TEXT("Convoy Escort"),
                        .completed_at = FDateTime{2026, 8, 24, 21, 15},
                        .simulation_duration_seconds = 4946.f,
                        .kills = 24,
                        .result = TEXT("Succeeded"),
                        .statistics = {{TEXT("Transports survived"), TEXT("5 / 6")},
                                       {TEXT("Hull integrity"), TEXT("61%")}}},
                       {.outcome_id = TEXT("vega_defence"),
                        .display_name = TEXT("Vega Defence"),
                        .completed_at = FDateTime{2026, 8, 27, 20, 30},
                        .simulation_duration_seconds = 4354.f,
                        .kills = 22,
                        .result = TEXT("Succeeded"),
                        .statistics = {{TEXT("Defence platforms remaining"), TEXT("4 / 5")},
                                       {TEXT("Capital ships destroyed"), TEXT("2")}}}}}},
        {TEXT("test_run_12"),
         {.profile_id = TEXT("test_run_12"),
          .outcomes = {{.outcome_id = TEXT("fleet_interception"),
                        .display_name = TEXT("Fleet Interception"),
                        .completed_at = FDateTime{2026, 8, 26, 14, 12},
                        .simulation_duration_seconds = 972.f,
                        .kills = 7,
                        .result = TEXT("Failed"),
                        .statistics = {{TEXT("Friendly ships lost"), TEXT("11")}}}}}},
        {TEXT("fleet_benchmark"),
         {.profile_id = TEXT("fleet_benchmark"),
          .outcomes = {{.outcome_id = TEXT("capital_fleet_benchmark"),
                        .display_name = TEXT("Capital Fleet Benchmark"),
                        .completed_at = FDateTime{2026, 8, 25, 9, 5},
                        .simulation_duration_seconds = 1800.f,
                        .kills = 112,
                        .result = TEXT("Succeeded"),
                        .statistics = {{TEXT("Simulation ticks"), TEXT("108000")},
                                       {TEXT("Peak active ships"), TEXT("384")},
                                       {TEXT("Average frame time"), TEXT("8.4 ms")}}}}}},
    };

    return FSaveGameBrowser{
        [summaries = MoveTemp(summaries)] { return summaries; },
        [reports = MoveTemp(reports)](FString const& profile_id) -> TOptional<FSaveProfileReport> {
            auto const* const report{reports.Find(profile_id)};
            return report ? TOptional<FSaveProfileReport>{*report}
                          : TOptional<FSaveProfileReport>{};
        }};
#endif
}
}
