#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/vector3d.h>

#include <vector>

namespace ioj::levels {
struct LevelCameraDefinition {
    std::vector<EntityId> target_entity_ids{};
    ioj::Vector3d offset_direction{};
    double distance{};
};
}
