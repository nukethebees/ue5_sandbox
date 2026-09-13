#pragma once

#include <sandbox/simulation/levels/CompiledLevelEvents.h>
#include <sandbox/simulation/levels/LevelDefinition.h>
#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

#include <expected>
#include <string>
#include <vector>

struct FSimulationClock;

namespace ml::level_authoring {
using LevelCompilationErrors = std::vector<std::string>;
using LevelCompilationResult = std::expected<FCompiledLevelEvents, LevelCompilationErrors>;

[[nodiscard]] auto compile_level(LevelDefinition const& definition,
                                 FSimulationClock const& clock,
                                 FCapitalSimulationConfig const& capital_config,
                                 FTurretSimulationConfig const& turret_config)
    -> LevelCompilationResult;
} // namespace ml::level_authoring
