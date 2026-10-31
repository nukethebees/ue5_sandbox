#pragma once

#include <ioj/levels/level_definition.h>
#include <ioj/profiles/completion_indicator_state.h>
#include <ioj/profiles/level_progress_summary.h>
#include <ioj/profiles/score_record.h>
#include <ioj/profiles/unlock_status.h>

#include <functional>
#include <span>

namespace ioj::profiles {

auto summarize_level_progress(levels::LevelId const& level_id, std::span<ScoreRecord const> records)
    -> LevelProgressSummary;
auto level_completion_indicator_state(LevelProgressSummary const& progress,
                                      std::optional<float> par_time_seconds)
    -> CompletionIndicatorState;
auto evaluate_unlock(levels::LevelDefinition const& definition,
                     std::function<bool(levels::LevelId const&)> const& completed) -> UnlockStatus;
}
