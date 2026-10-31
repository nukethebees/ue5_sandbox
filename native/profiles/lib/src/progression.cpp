#include <ioj/profiles/progression.h>

#include <algorithm>

namespace ioj::profiles {
auto level_completion_indicator_state(LevelProgressSummary const& progress,
                                      std::optional<float> const par_time_seconds)
    -> CompletionIndicatorState {
    if (progress.state() != LevelProgressState::Completed) {
        return CompletionIndicatorState::Incomplete;
    }
    if (par_time_seconds && progress.best_completion_time_seconds >= 0.f &&
        progress.best_completion_time_seconds <= *par_time_seconds) {
        return CompletionIndicatorState::ParAchieved;
    }
    return CompletionIndicatorState::Completed;
}
auto summarize_level_progress(levels::LevelId const& level_id, std::span<ScoreRecord const> records)
    -> LevelProgressSummary {
    LevelProgressSummary summary;
    for (auto const& record : records) {
        if (record.level_name != level_id.value) {
            continue;
        }
        ++summary.attempt_count;
        summary.best_kills = std::max(summary.best_kills, record.kills);
        summary.last_played_at = std::max(summary.last_played_at, record.date);
        if (record.outcome != TeamOutcome::Victory) {
            continue;
        }
        ++summary.completion_count;
        if (summary.best_completion_time_seconds < 0.f ||
            record.time_seconds < summary.best_completion_time_seconds) {
            summary.best_completion_time_seconds = record.time_seconds;
        }
    }
    return summary;
}
auto evaluate_unlock(levels::LevelDefinition const& definition,
                     std::function<bool(levels::LevelId const&)> const& completed) -> UnlockStatus {
    UnlockStatus status;
    for (auto const& id : definition.unlock_level_ids()) {
        auto const satisfied{completed(id)};
        status.criteria.push_back({id, satisfied});
    }
    return status;
}
}
