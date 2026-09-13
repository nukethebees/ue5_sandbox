#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sandbox/simulation/levels/LevelEventGroupCounts.h>
#include <sandbox/simulation/levels/LevelMissionEvents.h>
#include <sandbox/simulation/levels/LevelRuntimeEvents.h>
#include <sandbox/simulation/sim_tick.h>

namespace ml {
struct FLevelEventSchedule {
    FLevelSpawnGroups spawn_groups{};
    FLevelCapitalSpawnEvents capital_spawns{};
    FLevelTurretSpawnEvents turret_spawns{};
    FLevelMissionEvents mission_events{};
    std::vector<simulation::SimTick> execution_ticks{};
    std::vector<FLevelEventGroupCounts> event_group_counts{};

    auto add_spawn_group(ml::simulation::EntityType type, std::int32_t offset, std::int32_t count)
        -> bool;
    auto add_mission_group(ml::simulation::LevelMissionEventType type,
                           std::span<std::int32_t const> values) -> bool;
};
}
