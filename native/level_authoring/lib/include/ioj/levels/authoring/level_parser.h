#pragma once
#include <ioj/levels/level_definition.h>
#include <ioj/s7/ast.h>

#include <string_view>

namespace ioj::levels::authoring {
using LevelDefinitionReadResult = std::expected<LevelDefinition, Diagnostics>;
// Parse and verify components before constructing a trusted definition.
// Resolve omitted entity IDs within this level and default omitted rotations to zero.
// The root must be a valid node index in ast.
[[nodiscard]] auto parse_level(s7::Ast const& ast,
                               s7::NodeIndex root,
                               settings::Settings base = {},
                               std::string_view path = "level") -> LevelDefinitionReadResult;
}
