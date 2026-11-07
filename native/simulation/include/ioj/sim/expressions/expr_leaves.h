#pragma once

#include <ioj/levels/expr_result.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/sim/expressions/kill_count.h>
#include <ioj/sim/expressions/leaf_data_handle.h>
#include <ioj/sim/expressions/leaf_handle.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/vector.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ioj::sim {
struct ExprLeaves {
    void reset_progress() {
        std::ranges::fill(results, ExprResult::Pending);
        active.clear();
    }

    [[nodiscard]] auto num_leaves() const noexcept -> std::size_t { return results.size(); }

    // Shared
    std::vector<ExprResult> results;
    std::vector<LeafDataHandle> data_handles;
    std::vector<LeafHandle> active;

    // EntityDestroyed
    std::vector<LevelEntityIndex> destroyed_entities;

    // EntityReachedPosition
    std::vector<LevelEntityIndex> reached_entities;
    std::vector<Vector3d> positions;
    std::vector<double> radius_squared;

    // TimeElapsed
    std::vector<TimerHandle> timers;

    // ObjectiveResolved
    std::vector<ObjectiveId> observed_objectives;
    std::vector<ObjectiveOutcome> observed_outcomes;

    // EntityKillsReached
    std::vector<IndexSpan> kill_sources;
    std::vector<KillCount> kill_targets;
    std::vector<LevelEntityIndex> kill_entities;
};
}
