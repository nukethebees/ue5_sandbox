#pragma once

#include <sandbox/simulation/levels/LevelDefinition.h>

#include <expected>
#include <string>

namespace ml::level_authoring {
[[nodiscard]] auto emit_initial_level_source(LevelDefinition const& definition)
    -> std::expected<std::string, std::string>;
} // namespace ml::level_authoring
