#pragma once
#include <expected>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/sim_config.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>
namespace ml {
using FLevelEventCompilationResult =
    std::expected<::ioj::sim::CompiledLevelEvents, FLevelStartErrors>;
SPACEGAME_API auto compile_level_events(FLevelDefinition const& definition,
                                        ::ioj::sim::SimClock const& clock,
                                        ::ioj::sim::CapitalShipSimConfig const& capital_config,
                                        ::ioj::sim::TurretSimConfig const& turret_config)
    -> FLevelEventCompilationResult;
}
