#pragma once

#include <ioj/levels/level_definition.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/sim_config.h>

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::sim {
struct SimClock;
}

namespace ioj::levels {
using LevelCompilationErrors = std::vector<std::string>;
using LevelCompilationResult = std::expected<sim::CompiledLevelEvents, LevelCompilationErrors>;

[[nodiscard]] auto compile_level(LevelDefinition const& definition,
                                 sim::SimClock const& clock,
                                 sim::CapitalShipSimConfig const& capital_config,
                                 sim::TurretSimConfig const& turret_config)
    -> LevelCompilationResult;
} // namespace ioj::levels
