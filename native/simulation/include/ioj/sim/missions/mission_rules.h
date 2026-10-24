#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/mission_types.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/condition_type.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/vector.h>

#include <vector>

namespace ioj::sim {
struct MissionRules {
    EnumArray<ConditionType, std::vector<std::uint32_t>> leaves;

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
