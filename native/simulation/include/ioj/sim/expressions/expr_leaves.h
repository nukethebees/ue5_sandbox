#pragma once

#include <ioj/enum_range_array.h>
#include <ioj/expr_kind.h>
#include <ioj/kill_count.h>
#include <ioj/sim/expressions/expr_result.h>
#include <ioj/sim/expressions/latch_handle.h>
#include <ioj/sim/expressions/leaf_data_handle.h>
#include <ioj/sim/expressions/leaf_index.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/level_entity_ref.h>
#include <ioj/vector.h>

#include <cstddef>
#include <vector>

namespace ioj::sim {
struct ExprLeaves {

    [[nodiscard]] auto num_leaves() const noexcept -> std::size_t { return results.size(); }

    // Shared
    std::vector<ExprResult> results;
    std::vector<LeafDataHandle> data_handles;
    EnumRangeArray<std::vector<LeafIndex>, expr_kind_leaf_range> leaves_by_kind;

    // EntityDestroyed
    std::vector<LevelEntityRef> destroyed_entities;

    // EntityReachedPosition
    std::vector<LevelEntityRef> reached_entities;
    std::vector<Vector3d> positions;
    std::vector<double> radius_squared;

    // TimerElapsed
    std::vector<TimerHandle> timers;

    std::vector<LatchHandle> true_latches;
    std::vector<LatchHandle> false_latches;

    // EntityKillsReached
    std::vector<IndexSpan> kill_sources;
    std::vector<KillCount> kill_targets;
    std::vector<LevelEntityRef> kill_entities;
};
}
