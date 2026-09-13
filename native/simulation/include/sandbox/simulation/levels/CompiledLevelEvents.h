#pragma once

#include <sandbox/simulation/levels/LevelEventSchedule.h>
#include <sandbox/simulation/levels/LevelInitialisationData.h>
#include <sandbox/simulation/simulation/SimulationClock.h>

#include <expected>

namespace ml {
struct FLevelInitialSpawnEvents {
    FLevelCapitalSpawnEvents capital_spawns{};
    FLevelTurretSpawnEvents turret_spawns{};
};

struct FCompiledLevelEvents {
    FLevelInitialisationData initialisation{};
    FLevelInitialSpawnEvents initial_spawns{};
    FLevelEventSchedule schedule{};
};

}
