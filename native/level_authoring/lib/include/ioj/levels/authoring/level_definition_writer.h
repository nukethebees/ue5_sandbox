#pragma once
#include <ioj/levels/authoring/level_inputs.h>
#include <ioj/levels/level_definition.h>

#include <string>

namespace ioj::levels::authoring {
[[nodiscard]] auto make_level_input(LevelDefinition const& level) -> LevelInput;
[[nodiscard]] auto emit_level_source(LevelInput const& level) -> std::string;
}
