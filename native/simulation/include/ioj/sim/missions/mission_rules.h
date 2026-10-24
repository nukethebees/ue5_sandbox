#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/vector.h>

#include <array>
#include <vector>

namespace ioj::sim {
struct MissionRules {
    std::array<std::vector<std::uint32_t>, 5> leaves;
    std::vector<std::uint32_t> destroyed_entities;
    std::vector<std::uint32_t> reached_entities;
    std::vector<Vector3d> positions;
    std::vector<double> radius_squared;
    std::vector<SimTick> durations;
    std::vector<ObjectiveId> observed_objectives;
    std::vector<ObjectiveOutcome> observed_outcomes;
    std::vector<IndexSpan> kill_sources;
    std::vector<std::uint32_t> kill_targets;
    std::vector<std::uint32_t> kill_entities;
};
}
