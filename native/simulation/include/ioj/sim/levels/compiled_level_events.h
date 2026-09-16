#pragma once

#include <ioj/sim/levels/level_event_schedule.h>
#include <ioj/sim/levels/level_initialisation_data.h>
#include <ioj/sim/sim_clock.h>

#include <expected>

namespace ioj::sim {
struct LevelInitialSpawnEvents {
    SingleAllocationLevelCapitalSpawnEvents capital_spawns{};
    SingleAllocationLevelTurretSpawnEvents turret_spawns{};
    SingleAllocationLevelSpinnerSpawnEvents spinner_spawns{};
};

struct CompiledLevelEvents {
    LevelInitialisationData initialisation{};
    LevelInitialSpawnEvents initial_spawns{};
    LevelEventSchedule schedule{};
};

}
