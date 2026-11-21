#pragma once

#include <ioj/duration.h>
#include <ioj/levels/level_definition.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace ioj::sim {
using LevelSimulationBuildErrors = std::vector<std::string>;
using LevelSimulationValidationResult = std::expected<void, LevelSimulationBuildErrors>;

using LevelSimulationBuildResult = std::expected<LevelSimInitData, LevelSimulationBuildErrors>;

[[nodiscard]] auto validate_level_simulation_init_data(LevelSimInitData const& data)
    -> LevelSimulationValidationResult;

struct LevelRunTiming {
    std::optional<PositiveFinite<double>> time_scale;
    std::optional<Duration> duration;
};

[[nodiscard]] auto make_level_simulation_init_data(levels::LevelDefinition const& definition,
                                                   LevelSimInitData inputs,
                                                   settings::Settings const& settings,
                                                   LevelRunTiming timing)
    -> LevelSimulationBuildResult;
[[nodiscard]] auto make_level_simulation_init_data(levels::LevelDefinition const& definition,
                                                   LevelSimInitData inputs,
                                                   LevelRunTiming timing)
    -> LevelSimulationBuildResult;
}
