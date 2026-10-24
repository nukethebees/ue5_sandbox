#pragma once

#include <ioj/levels/authoring/level_inputs.h>
#include <ioj/levels/authoring/level_parser.h>

namespace ioj::levels::authoring::detail {
auto process_level(LevelInput input) -> LevelDefinitionReadResult;
}
