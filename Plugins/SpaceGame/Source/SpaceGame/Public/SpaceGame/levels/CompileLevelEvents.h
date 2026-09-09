#pragma once
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGameSimulation/levels/CompiledLevelEvents.h>
namespace ml {
using FLevelEventCompilationResult = std::expected<FCompiledLevelEvents, FLevelStartErrors>;
SPACEGAME_API auto compile_level_events(FLevelDefinition const& definition,
                                        FSimulationClock const& clock,
                                        FCapitalSimulationConfig const& capital_config,
                                        FTurretSimulationConfig const& turret_config)
    -> FLevelEventCompilationResult;
}
