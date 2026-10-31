#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/profiles/timestamp.h>

#include <string>

namespace ioj::profiles {
struct ScoreRecord {
    auto operator==(ScoreRecord const&) const -> bool = default;

    Timestamp date{};
    std::string level_name{};
    Team team{Team::White};
    TeamOutcome outcome{TeamOutcome::Defeat};
    std::string objective_id{};
    ObjectiveOutcome objective_outcome{ObjectiveOutcome::Failed};
    std::int32_t kills{};
    float time_seconds{};
};
}
