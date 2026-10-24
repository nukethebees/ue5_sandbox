#pragma once
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_event_types.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/sim/sim_tick.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelEventSchedule {
    LevelSpawnGroups spawn_groups{};
    // Execute this prefix during initialisation, before scheduled ticks.
    std::uint32_t initial_spawn_group_count{};
    LevelCapitalSpawnEvents capital_spawns{};
    LevelTurretSpawnEvents turret_spawns{};
    LevelSpinnerSpawnEvents spinner_spawns{};
    std::vector<SimTick> execution_ticks{};
    std::vector<LevelEventGroupCounts> event_group_counts{};

    // Insert payload ranges into the initial prefix; offset and count must be nonnegative.
    void add_initial_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);
    auto add_spawn_group(EntityType type, std::int32_t offset, std::int32_t count) -> bool;

    // Include future capital/turret spawns when configuring team-dependent subsystems.
    auto get_participating_teams(std::optional<Team> player_team = {}) const -> TeamList;
};
}
