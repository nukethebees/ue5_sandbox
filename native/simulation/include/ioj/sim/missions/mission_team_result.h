#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/sim/missions/objective_id.h>

namespace ioj::sim {
struct MissionTeamResult {
    Team team{};
    ObjectiveId objective{};
    ObjectiveOutcome outcome{};
    TeamOutcome result{};
};
}
