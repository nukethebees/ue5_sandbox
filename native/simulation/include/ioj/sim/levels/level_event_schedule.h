#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/level_event_types.h>
#include <ioj/sim/levels/level_mission_events.h>
#include <ioj/sim/levels/level_runtime_events.h>
#include <ioj/sim/sim_tick.h>

namespace ioj::sim {
struct LevelEventSchedule {
    LevelSpawnGroups spawn_groups{};
    SingleAllocationLevelCapitalSpawnEvents capital_spawns{};
    SingleAllocationLevelTurretSpawnEvents turret_spawns{};
    LevelMissionEvents mission_events{};
    std::vector<SimTick> execution_ticks{};
    std::vector<LevelEventGroupCounts> event_group_counts{};

    auto add_spawn_group(EntityType type, std::int32_t offset, std::int32_t count) -> bool;
    auto add_mission_group(LevelMissionEventType type, std::span<std::int32_t const> values)
        -> bool;
};
}
