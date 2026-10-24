#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/sim_tick.h>

namespace ioj::sim {
struct TeamCompletion {
    Team team{};
    TeamOutcome outcome{};
    ObjectiveId objective{};
    ObjectiveOutcome objective_outcome{};
    SimTick tick{};
};
}
