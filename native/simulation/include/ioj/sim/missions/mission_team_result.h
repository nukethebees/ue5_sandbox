#pragma once

#include <ioj/levels/objective_outcome.h>
#include <ioj/levels/team.h>
#include <ioj/levels/team_outcome.h>
#include <ioj/sim/missions/objective_id.h>

namespace ioj::sim {
struct MissionTeamResult {
    ObjectiveId objective{};
    Team team{};
    ObjectiveOutcome outcome{};
    TeamOutcome result{};
};
}
