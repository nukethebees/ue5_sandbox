#pragma once

#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/levels/LevelEventSchedule.h>
#include <SpaceGame/levels/LevelInitialisationData.h>
#include <SpaceGame/simulation/LevelSimulationConfig.h>
#include <SpaceGame/simulation/SimulationClock.h>

namespace ml {
struct SPACEGAME_API FCompiledLevelEvents {
    FLevelInitialisationData initialisation{};
    FLevelEventSchedule schedule{};
};

SPACEGAME_API auto compile_level_events(FLevelDefinition const& definition,
                                        FSimulationClock const& clock,
                                        FCapitalSimulationConfig const& capital_config,
                                        FTurretSimulationConfig const& turret_config)
    -> FCompiledLevelEvents;
}
