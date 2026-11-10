#pragma once

#include <ioj/levels/level_outcome.h>
#include <ioj/levels/team.h>
#include <ioj/profiles/timestamp.h>
#include <ioj/sim/levels/run_end_reason.h>

#include <string>

namespace ioj::profiles {
struct ScoreRecord {
    auto operator==(ScoreRecord const&) const -> bool = default;

    Timestamp date{};
    std::string level_name{};
    Team team{Team::White};
    LevelOutcome outcome{LevelOutcome::Defeat};
    sim::RunEndReason end_reason{sim::RunEndReason::LevelCompleted};
    std::int32_t kills{};
    float time_seconds{};
};
}
