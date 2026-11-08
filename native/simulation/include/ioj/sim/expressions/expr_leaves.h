#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/expressions/kill_count.h>
#include <ioj/sim/expressions/latch_handle.h>
#include <ioj/sim/expressions/leaf_data_handle.h>
#include <ioj/sim/expressions/leaf_handle.h>
#include <ioj/sim/expressions/signal_handle.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/level_entity_ref.h>
#include <ioj/vector.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ioj::sim {
struct ExprLeaves {

    [[nodiscard]] auto num_leaves() const noexcept -> std::size_t { return results.size(); }

    // Shared
    std::vector<std::uint8_t> results;
    std::vector<LeafDataHandle> data_handles;
    EnumArray<std::vector<LeafHandle>, LeafKind> runs;

    // EntityDestroyed
    std::vector<LevelEntityRef> destroyed_entities;

    // EntityReachedPosition
    std::vector<LevelEntityRef> reached_entities;
    std::vector<Vector3d> positions;
    std::vector<double> radius_squared;

    // TimeElapsed
    std::vector<TimerHandle> timers;

    std::vector<LatchHandle> true_latches;
    std::vector<LatchHandle> false_latches;
    std::vector<SignalHandle> signals;

    // EntityKillsReached
    std::vector<IndexSpan> kill_sources;
    std::vector<KillCount> kill_targets;
    std::vector<LevelEntityRef> kill_entities;
};
}
