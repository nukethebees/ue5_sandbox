#pragma once

#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/levels/level_definition.h>
#include <ioj/sim/sim_config.h>

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::sim {
struct SimClock;
}

namespace ioj::sim::levels {
using LevelCompilationErrors = std::vector<std::string>;
using LevelCompilationResult = std::expected<CompiledLevelEvents, LevelCompilationErrors>;

[[nodiscard]] auto to_simulation_team(std::string_view id) noexcept -> Team;

[[nodiscard]] auto compile_level(LevelDefinition const& definition,
                                 SimClock const& clock,
                                 CapitalShipSimConfig const& capital_config,
                                 TurretSimConfig const& turret_config) -> LevelCompilationResult;
} // namespace ioj::sim::levels
