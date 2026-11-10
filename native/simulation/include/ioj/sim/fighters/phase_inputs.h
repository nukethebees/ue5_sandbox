#pragma once

#include <ioj/frame_array.h>
#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/sim_tick.h>

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
class EntityLedger;
struct EntityDeathInfo;
struct SimClock;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::tasks {
class TaskManager;
}

namespace ioj::sim::fighters {
struct NavigationCounters;

struct PreparationInputs {
    EntityLedger& ledger;
    std::span<Team const> participating_teams;
};

struct ThinkingInputs {
    SimClock const& clock;
    tasks::TaskManager const& task_manager;
    float collision_radius;
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
    FrameArray<std::uint32_t>& pending_fire_indices;
    NavigationCounters& navigation_counters;
};

struct ActionInputs {
    float dt;
    lasers::Sim& laser_simulation;
    FrameMemoryResource& scratch_resource;
    std::span<std::uint32_t const> pending_fire_indices;
    FrameArray<EntityId>& overlap_candidates;
};

struct ResolutionInputs {
    CombatEvents const& combat_events;
    EntityLedger& ledger;
    SimTick tick;
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
    EntityDeathInfo& entity_death_info;
};

struct ResolutionCommitInputs {
    std::pmr::vector<std::uint32_t>& local_indices_to_remove;
};
} // namespace ioj::sim::fighters
