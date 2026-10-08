#pragma once
#include <ioj/levels/level_definition.h>
#include <ioj/s7/ast.h>

namespace ioj::levels::authoring {
using LevelDefinitionReadResult = std::expected<LevelDefinition, Diagnostics>;
// Parse structure and types only. Call validate_level before publishing to consumers.
[[nodiscard]] auto parse_level(s7::Ast const& ast) -> LevelDefinitionReadResult;
}
