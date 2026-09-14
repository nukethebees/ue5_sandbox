#pragma once

#include <ioj/sim/levels/level_event_schedule.h>
#include <ioj/sim/levels/level_initialisation_data.h>
#include <ioj/sim/sim_clock.h>

#include <expected>

namespace ioj::sim {
struct LevelInitialSpawnEvents {
    LevelCapitalSpawnEvents capital_spawns{};
    LevelTurretSpawnEvents turret_spawns{};
    LevelSpinnerSpawnEvents spinner_spawns{};
};

struct CompiledLevelEvents {
    LevelInitialisationData initialisation{};
    LevelInitialSpawnEvents initial_spawns{};
    LevelEventSchedule schedule{};
};

}
