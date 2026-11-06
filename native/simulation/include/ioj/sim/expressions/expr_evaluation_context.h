#pragma once

#include <ioj/sim/sim_tick.h>

#include <span>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct EntityId;
class EntityLedger;
struct SpatialQueryManager;

struct ExprEvaluationContext {
    SimTick tick{};
    EntityLedger const& ledger;
    std::span<EntityId const> entities;
    SpatialQueryManager const* spatial{};
    FrameMemoryResource* scratch{};
};
}
