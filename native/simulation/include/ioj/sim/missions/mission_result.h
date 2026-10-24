#pragma once
#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>

#include <string>

namespace ioj::sim {
struct LevelMissionResult {
    std::string level_id{};
    std::string level_display_name{};
    Team team{};
    TeamOutcome outcome{};
    std::string objective_id{};
    ObjectiveOutcome objective_outcome{};
    float elapsed_seconds{};
    bool save_results{true};
};
}
