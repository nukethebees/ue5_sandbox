#pragma once
#include <ioj/levels/level_definition.h>

#include <string>

namespace ioj::levels::authoring {
[[nodiscard]] auto emit_editor_level_source(LevelDefinition const& level)
    -> std::expected<std::string, Diagnostics>;
}
