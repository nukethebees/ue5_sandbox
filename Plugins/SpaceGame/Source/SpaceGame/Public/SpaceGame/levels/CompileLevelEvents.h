#pragma once
#include <expected>
#include <sandbox/simulation/levels/CompiledLevelEvents.h>
#include <sandbox/simulation/simulation/LevelSimulationConfig.h>
#include <sandbox/simulation/simulation/SimulationClock.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>
namespace ml {
using FLevelEventCompilationResult = std::expected<FCompiledLevelEvents, FLevelStartErrors>;
SPACEGAME_API auto compile_level_events(FLevelDefinition const& definition,
                                        FSimulationClock const& clock,
                                        FCapitalSimulationConfig const& capital_config,
                                        FTurretSimulationConfig const& turret_config)
    -> FLevelEventCompilationResult;
}
