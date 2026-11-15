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
}

namespace ioj::sim::capital_ships {
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
} // namespace ioj::sim::capital_ships
