#pragma once

#include <SpaceGameSimulation/levels/LevelEventSchedule.h>
#include <SpaceGameSimulation/levels/LevelInitialisationData.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>
#include <SpaceGameSimulation/simulation/SimulationClock.h>

#include <expected>

namespace ml {
struct SPACEGAMESIMULATION_API FLevelInitialSpawnEvents {
    FLevelCapitalSpawnEvents capital_spawns{};
    FLevelTurretSpawnEvents turret_spawns{};
};

struct SPACEGAMESIMULATION_API FCompiledLevelEvents {
    FLevelInitialisationData initialisation{};
    FLevelInitialSpawnEvents initial_spawns{};
    FLevelEventSchedule schedule{};
};

}
