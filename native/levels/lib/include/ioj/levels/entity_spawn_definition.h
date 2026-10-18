#pragma once

#include <ioj/levels/entity_archetype.h>
#include <ioj/levels/identifiers.h>
#include <ioj/levels/team.h>
#include <ioj/rotator3d.h>
#include <ioj/vector_value_math.h>

#include <string>

namespace ioj::levels {
struct EntitySpawnDefinition {
    EntityId id{};
    EntityArchetype archetype{EntityArchetype::PlayerFighter};
    Team team{};
    Vector3d position{};
    Rotator3d rotation{};
    double spawn_time_seconds{};
};
}
