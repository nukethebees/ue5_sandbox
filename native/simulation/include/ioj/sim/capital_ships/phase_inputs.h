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
struct EntityTables;
struct EntityDeathInfo;
struct EntityFrameChanges;
struct SimClock;
}

namespace ioj::sim::capital_ships {
struct ResolutionInputs {
    EntityTables& tables;
    DamageQueue const& damage_queue;
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
    EntityDeathInfo& entity_death_info;
};

struct ResolutionCommitInputs {
    EntityTables& tables;
    EntityFrameChanges& changes;
    SimClock const& clock;
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
};
} // namespace ioj::sim::capital_ships
