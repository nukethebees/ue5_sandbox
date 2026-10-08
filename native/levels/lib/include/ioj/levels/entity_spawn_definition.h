#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/rotator3d.h>

#include <sandbox/core/vector3d.h>

#include <string>

namespace ioj::levels {
struct EntitySpawnDefinition {
    EntityId id{};
    std::string archetype{};
    TeamId team{};
    ml::Vector3d position{};
    ioj::Rotator3d rotation{};
    double spawn_time_seconds{};
};
}
