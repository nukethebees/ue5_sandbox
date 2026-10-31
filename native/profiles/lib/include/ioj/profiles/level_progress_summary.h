#pragma once

#include <ioj/profiles/level_progress_state.h>
#include <ioj/profiles/timestamp.h>

namespace ioj::profiles {
struct LevelProgressSummary {
    auto state() const -> LevelProgressState {
        return completion_count > 0 ? LevelProgressState::Completed
             : attempt_count > 0    ? LevelProgressState::Attempted
                                    : LevelProgressState::NotAttempted;
    }

    std::int32_t attempt_count{};
    std::int32_t completion_count{};
    Timestamp last_played_at{};
    float best_completion_time_seconds{-1.f};
    std::int32_t best_kills{};
};
}
