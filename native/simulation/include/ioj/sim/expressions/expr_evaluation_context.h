#pragma once

#include <cstdint>
#include <span>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct EntityId;
class EntityLedger;
struct SpatialQueryManager;
class Timers;
class Latches;

struct ExprEvaluationContext {
    EntityLedger const& ledger;
    Timers const& timers;
    std::span<EntityId const> entities;
    Latches const& latches;
    std::span<std::uint8_t const> signals;
    SpatialQueryManager const* spatial{};
    FrameMemoryResource* scratch{};
};
}
