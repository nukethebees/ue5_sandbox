#pragma once

#include <ioj/levels/mission_outcome.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/levels/team.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/sim_tick.h>

namespace ioj::sim {
struct TeamMissionCompletion {
    Team team{};
    MissionOutcome outcome{};
    ObjectiveId triggering_objective{};
    ObjectiveOutcome triggering_objective_outcome{};
    SimTick tick{};
};
}
