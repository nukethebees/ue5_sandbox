#include "SpaceGame/persistence/SaveGameProfileAdapter.h"

#include "SpaceGame/persistence/SpaceSaveGame.h"

#include <SandboxGameShared/core/levels/levels.h>
#include <SandboxGameShared/utilities/enums.h>

namespace ml::ioj::save_profile {
namespace profile_adapter {
void add_statistic(FLevelOutcomeSummary& outcome, FString label, FString value) {
    outcome.statistics.Add({MoveTemp(label), MoveTemp(value)});
}

auto make_outcome_id(FScoreRecord const& record, int32 const index) -> FString {
    return FString::Printf(TEXT("%s_%d"), *record.level_name.ToString(), index);
}
}

auto make_summary(FString profile_id, FString display_name, TConstArrayView<FScoreRecord> records)
    -> FSaveProfileSummary {
    FSaveProfileSummary summary{
        .profile_id = MoveTemp(profile_id),
        .display_name = MoveTemp(display_name),
        .outcome_count = records.Num(),
    };
    if (records.IsEmpty()) {
        return summary;
    }

    summary.created_at = records[0].date;
    summary.last_played_at = records[0].date;
    for (FScoreRecord const& record : records) {
        summary.created_at = FMath::Min(summary.created_at, record.date);
        summary.last_played_at = FMath::Max(summary.last_played_at, record.date);
        summary.total_simulation_duration_seconds += record.time_seconds;
        summary.total_kills += record.kills;
    }
    return summary;
}

auto make_report(FString profile_id, TConstArrayView<FScoreRecord> records) -> FSaveProfileReport {
    FSaveProfileReport report{.profile_id = MoveTemp(profile_id)};
    auto const n_records{records.Num()};
    report.outcomes.Reserve(n_records);
    for (int32 i{0}; i < n_records; ++i) {
        auto const& record{records[i]};
        auto& outcome{report.outcomes.AddDefaulted_GetRef()};
        outcome.outcome_id = profile_adapter::make_outcome_id(record, i);
        outcome.display_name = ml::format_level_display_name(record.level_name);
        outcome.completed_at = record.date;
        outcome.simulation_duration_seconds = record.time_seconds;
        outcome.kills = record.kills;
        outcome.result = ml::to_string_without_type_prefix(record.end_state);

        profile_adapter::add_statistic(
            outcome, TEXT("Mission mode"), ml::to_string_without_type_prefix(record.mission_mode));
        switch (record.mission_mode) {
            case ETestMissionMode::None: {
                break;
            }
            case ETestMissionMode::SurviveTime: {
                profile_adapter::add_statistic(
                    outcome,
                    TEXT("Target survival time"),
                    FString::Printf(TEXT("%.1f s"), record.target_completion_time));
                break;
            }
            case ETestMissionMode::KillEnemies: {
                profile_adapter::add_statistic(
                    outcome, TEXT("Target kills"), FString::FromInt(record.target_kills));
                break;
            }
            case ETestMissionMode::KillEnemiesWithinTime: {
                profile_adapter::add_statistic(
                    outcome, TEXT("Target kills"), FString::FromInt(record.target_kills));
                profile_adapter::add_statistic(
                    outcome,
                    TEXT("Target completion time"),
                    FString::Printf(TEXT("%.1f s"), record.target_completion_time));
                break;
            }
        }

        if (record.end_state == ETestMissionState::Failed) {
            profile_adapter::add_statistic(outcome,
                                           TEXT("Failure reason"),
                                           ml::to_string_without_type_prefix(record.fail_reason));
        }
    }
    return report;
}
}
