#pragma once

#include <ioj/sim/sim_tick.h>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
class EntityLedger;
struct SimClock;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::fighters {
struct PreparationInputs {
    EntityLedger& ledger;
};

struct ThinkingInputs {
    SimClock const& clock;
    float collision_radius;
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
};

struct ActionInputs {
    float dt;
    lasers::Sim& laser_simulation;
    FrameMemoryResource& scratch_resource;
};

struct ResolutionInputs {
    CombatEvents const& combat_events;
    EntityLedger& ledger;
    SimTick tick;
    FrameMemoryResource& scratch_resource;
};
} // namespace ioj::sim::fighters
