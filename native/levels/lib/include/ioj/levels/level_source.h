#pragma once

#include <ioj/levels/level_definition.h>

#include <string>
#include <variant>

namespace ioj::levels {
using LevelSource = std::variant<LevelDefinition, std::string>;
}
