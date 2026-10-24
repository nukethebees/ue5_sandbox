#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/sim_tick.h>

#include <span>

namespace ioj::sim {
class EntityLedger;
struct SpatialQueryManager;
struct MissionInputs {
    SimTick tick{};
    EntityLedger const& ledger;
    // Stable authored entity slots; unspawned slots contain the null EntityId sentinel.
    std::span<EntityId const> entities;
    // Position rules require both services, with all live authored entities spatially indexed.
    SpatialQueryManager const* spatial{};
    FrameMemoryResource* scratch{};
};
}
