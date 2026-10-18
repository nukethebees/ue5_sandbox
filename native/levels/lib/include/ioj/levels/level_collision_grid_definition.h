#pragma once

#include <ioj/vector_value_math.h>

#include <optional>

namespace ioj::levels {
struct LevelCollisionGridDefinition {
    std::optional<ioj::Vector3d> level_size{};
    std::optional<ioj::Vector3d> cell_size{};
};
}
