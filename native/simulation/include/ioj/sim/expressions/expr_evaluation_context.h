#pragma once

#include <span>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct EntityId;
class EntityLedger;
struct SpatialQueryManager;
class Timers;

struct ExprEvaluationContext {
    EntityLedger const& ledger;
    Timers const& timers;
    std::span<EntityId const> entities;
    SpatialQueryManager const* spatial{};
    FrameMemoryResource* scratch{};
};
}
