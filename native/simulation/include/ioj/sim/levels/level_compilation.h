#pragma once

#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/levels/level_definition.h>
#include <ioj/sim/sim_config.h>

#include <expected>
#include <string>
#include <vector>

struct SimClock;

namespace ioj::sim::levels {
using LevelCompilationErrors = std::vector<std::string>;
using LevelCompilationResult = std::expected<CompiledLevelEvents, LevelCompilationErrors>;

[[nodiscard]] auto compile_level(LevelDefinition const& definition,
                                 SimClock const& clock,
                                 CapitalShipSimConfig const& capital_config,
                                 TurretSimConfig const& turret_config) -> LevelCompilationResult;
} // namespace ioj::sim::levels
