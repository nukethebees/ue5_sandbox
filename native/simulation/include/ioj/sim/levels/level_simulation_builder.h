#pragma once

#include <ioj/levels/level_definition.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace ioj::sim {
using LevelSimulationBuildErrors = std::vector<std::string>;
using LevelSimulationBuildResult = std::expected<LevelSimInitData, LevelSimulationBuildErrors>;

[[nodiscard]] auto validate_level_simulation_init_data(LevelSimInitData const& data)
    -> std::expected<void, LevelSimulationBuildErrors>;

// Inputs supply plain collision bounds and weapon/spawn sockets. A declared player does not
// require a presentation object; optional player input supplies its sockets and flight loadout.
[[nodiscard]] auto make_level_simulation_init_data(
    levels::LevelDefinition const& definition,
    LevelSimInitData inputs,
    std::optional<settings::TickSettings> clock_override = std::nullopt)
    -> LevelSimulationBuildResult;
}
