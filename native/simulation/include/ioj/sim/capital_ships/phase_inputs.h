#pragma once

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
struct EntityDeathInfo;
struct SimClock;
}

namespace ioj::sim::capital_ships {
struct ResolutionInputs {
    CombatEvents const& combat_events;
    SimClock const& clock;
    FrameMemoryResource& scratch_resource;
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
    EntityDeathInfo& entity_death_info;
};

struct ResolutionCommitInputs {
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
};
} // namespace ioj::sim::capital_ships
