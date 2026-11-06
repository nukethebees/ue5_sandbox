#pragma once
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_event_types.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSpawnEvent {
    ExprHandle condition;
    IndexSpan spawn_groups;
};

struct LevelEventSchedule {
    LevelSpawnGroups initial_spawn_groups{};
    LevelSpawnGroups spawn_groups{};
    LevelCapitalSpawnEvents capital_spawns{};
    LevelTurretSpawnEvents turret_spawns{};
    LevelSpinnerSpawnEvents spinner_spawns{};
    std::vector<LevelSpawnEvent> pending_spawns{};

    void add_initial_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);
    void add_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);

    // Include future capital/turret spawns when configuring team-dependent subsystems.
    auto get_participating_teams(std::optional<Team> player_team = {}) const -> TeamList;
};
}
