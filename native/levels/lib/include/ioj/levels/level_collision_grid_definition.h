#pragma once

#include <sandbox/core/vector3d.h>

#include <optional>

namespace ioj::levels {
struct LevelCollisionGridDefinition {
    std::optional<ml::Vector3d> level_size{};
    std::optional<ml::Vector3d> cell_size{};
};
}
