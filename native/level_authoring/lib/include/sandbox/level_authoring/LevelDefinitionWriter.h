#pragma once

#include <ioj/sim/levels/level_definition.h>

#include <expected>
#include <string>

namespace ml::level_authoring {
[[nodiscard]] auto emit_initial_level_source(::ioj::sim::levels::LevelDefinition const& definition)
    -> std::expected<std::string, std::string>;
} // namespace ml::level_authoring
