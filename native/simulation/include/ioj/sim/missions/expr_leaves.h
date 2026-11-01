#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/expr_result.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/expr_leaf_handle.h>
#include <ioj/sim/missions/leaf_data_handle.h>
#include <ioj/sim/missions/leaf_kind.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/vector.h>

#include <vector>

namespace ioj::sim {
struct ExprLeaves {
    // Shared
    std::vector<ExprResult> results;
    std::vector<ObjectiveId> owners;
    std::vector<LeafDataHandle> data_handles;
    EnumArray<LeafKind, std::vector<ExprLeafHandle>> leaf_handles;
    std::vector<LeafDataHandle> active;

    // EntityDestroyed
    std::vector<std::uint32_t> destroyed_entities;

    // EntityReachedPosition
    std::vector<std::uint32_t> reached_entities;
    std::vector<Vector3d> positions;
    std::vector<double> radius_squared;

    // TimeElapsed
    std::vector<SimTick> durations;

    // ObjectiveResolved
    std::vector<ObjectiveId> observed_objectives;
    std::vector<ObjectiveOutcome> observed_outcomes;

    // EntityKillsReached
    std::vector<IndexSpan> kill_sources;
    std::vector<std::uint32_t> kill_targets;
    std::vector<std::uint32_t> kill_entities;
};
}
