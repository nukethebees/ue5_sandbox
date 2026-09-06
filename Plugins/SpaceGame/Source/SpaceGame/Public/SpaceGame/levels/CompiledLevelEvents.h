#pragma once

#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/levels/LevelEventSchedule.h>
#include <SpaceGame/levels/LevelInitialisationData.h>
#include <SpaceGame/levels/LevelStartErrors.h>
#include <SpaceGame/simulation/LevelSimulationConfig.h>
#include <SpaceGame/simulation/SimulationClock.h>

#include <expected>

namespace ml {
struct SPACEGAME_API FLevelInitialSpawnEvents {
    FLevelCapitalSpawnEvents capital_spawns{};
    FLevelTurretSpawnEvents turret_spawns{};
};

struct SPACEGAME_API FCompiledLevelEvents {
    FLevelInitialisationData initialisation{};
    FLevelInitialSpawnEvents initial_spawns{};
    FLevelEventSchedule schedule{};
};

using FLevelEventCompilationResult = std::expected<FCompiledLevelEvents, FLevelStartErrors>;

SPACEGAME_API auto compile_level_events(FLevelDefinition const& definition,
                                        FSimulationClock const& clock,
                                        FCapitalSimulationConfig const& capital_config,
                                        FTurretSimulationConfig const& turret_config)
    -> FLevelEventCompilationResult;
}
