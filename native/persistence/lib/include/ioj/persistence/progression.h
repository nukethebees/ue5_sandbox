#pragma once

#include <ioj/levels/level_definition.h>
#include <ioj/persistence/profile.h>

#include <functional>
#include <span>

namespace ioj::persistence {
enum class LevelProgressState : std::uint8_t { NotAttempted, Attempted, Completed };
enum class CompletionIndicatorState : std::uint8_t { Incomplete, Completed, ParAchieved };

struct LevelProgressSummary {
    LevelProgressState state{LevelProgressState::NotAttempted};
    std::int32_t attempt_count{};
    std::int32_t completion_count{};
    Timestamp last_played_at{};
    float best_completion_time_seconds{-1.f};
    std::int32_t best_kills{};
};

struct UnlockCriterionStatus {
    levels::LevelId level_id{};
    bool satisfied{};
};

struct UnlockStatus {
    bool unlocked{true};
    std::vector<UnlockCriterionStatus> criteria{};
};

auto summarize_level_progress(levels::LevelId const& level_id, std::span<ScoreRecord const> records)
    -> LevelProgressSummary;
auto level_completion_indicator_state(LevelProgressSummary const& progress,
                                      std::optional<float> par_time_seconds)
    -> CompletionIndicatorState;
auto evaluate_unlock(levels::LevelDefinition const& definition,
                     std::function<bool(levels::LevelId const&)> const& completed,
                     bool force_unlocked = false) -> UnlockStatus;
}
