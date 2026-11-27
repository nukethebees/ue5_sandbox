#pragma once

#include <ioj/settings/settings.h>

#include <expected>
#include <string>
#include <vector>

namespace ioj::levels {
class LevelDefinition;
}
namespace ioj::sim {
using LevelSimulationBuildErrors = std::vector<std::string>;
using LevelSimulationValidationResult = std::expected<void, LevelSimulationBuildErrors>;

auto validate_playable_level(levels::LevelDefinition const& definition)
    -> LevelSimulationValidationResult;
auto validate_entity_geometry(settings::Settings const& settings)
    -> LevelSimulationValidationResult;
}
