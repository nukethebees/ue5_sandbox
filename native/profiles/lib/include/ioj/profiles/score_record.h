#pragma once

#include <ioj/levels/mission_outcome.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/levels/team.h>
#include <ioj/profiles/timestamp.h>

#include <string>

namespace ioj::profiles {
struct ScoreRecord {
    auto operator==(ScoreRecord const&) const -> bool = default;

    Timestamp date{};
    std::string level_name{};
    Team team{Team::White};
    MissionOutcome outcome{MissionOutcome::Defeat};
    std::string objective_id{};
    ObjectiveOutcome objective_outcome{ObjectiveOutcome::Failed};
    std::int32_t kills{};
    float time_seconds{};
};
}
