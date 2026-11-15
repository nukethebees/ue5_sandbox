#pragma once

#include <ioj/sim/sim_tick.h>

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class DamageQueue;
class EntityLedger;
struct EntityDeathInfo;
struct SimClock;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::turrets {
struct ThinkingInputs {
    SimClock const& clock;
    SpatialQueryManager const& spatial_query_manager;
    lasers::Sim& laser_simulation;
    FrameMemoryResource& scratch_resource;
};

struct ResolutionInputs {
    DamageQueue const& damage_queue;
    EntityLedger& ledger;
    SimTick tick;
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
    EntityDeathInfo& entity_death_info;
};

struct ResolutionCommitInputs {
    SimClock const& clock;
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
};
} // namespace ioj::sim::turrets
