#pragma once

#include <span>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct EntityId;
class EntityLedger;
class EntityLookupTables;
struct SpatialQueryManager;
class Timers;
class Latches;

struct ExprEvaluationContext {
    EntityLedger const& ledger;
    Timers const& timers;
    std::span<EntityId const> entities;
    Latches const& latches;
    SpatialQueryManager const& spatial;
    EntityLookupTables const& lookups;
    FrameMemoryResource& scratch;
};
}
