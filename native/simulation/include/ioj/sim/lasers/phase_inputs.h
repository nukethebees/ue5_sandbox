#pragma once

#include <ioj/sim/sim_tick.h>

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
struct SimClock;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
using LaserIndex = std::uint32_t;

struct PreparationInputs {
    SimClock const& clock;
    CombatEvents& combat_events;
};

struct ActionInputs {
    float dt;
    CombatEvents& combat_events;
    SimTick tick;
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
    std::pmr::vector<LaserIndex>& pending_removals;
};

struct ResolutionCommitInputs {
    std::pmr::vector<LaserIndex>& pending_removals;
    SimClock const& clock;
};
} // namespace ioj::sim::lasers
