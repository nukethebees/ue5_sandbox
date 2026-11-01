#pragma once

#include <ioj/levels/mission_outcome.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/levels/team.h>
#include <ioj/sim/missions/objective_id.h>

namespace ioj::sim {
struct MissionTeamResult {
    ObjectiveId objective{};
    Team team{};
    ObjectiveOutcome outcome{};
    MissionOutcome result{};
};
}
