#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/sim_tick.h>

#include <span>

namespace ioj::sim {
class EntityLedger;
struct SpatialQueryManager;

// Borrow simulation state and scratch storage for one mission evaluation.
struct MissionEvaluationContext {
    SimTick tick{};
    EntityLedger const& ledger;
    std::span<EntityId const> entities;
    SpatialQueryManager const* spatial{};
    FrameMemoryResource* scratch{};
};
}
