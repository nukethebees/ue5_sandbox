#pragma once

#include <ioj/levels/identifiers.h>

#include <sandbox/core/vector3d.h>

#include <vector>

namespace ioj::levels {
struct LevelCameraDefinition {
    std::vector<EntityId> target_entity_ids{};
    ml::Vector3d offset_direction{};
    double distance{};
};
}
