#pragma once

#include <ioj/levels/entity_archetype.h>
#include <ioj/levels/identifiers.h>
#include <ioj/levels/team.h>
#include <ioj/rotator3d.h>
#include <ioj/vector_value_math.h>

#include <cstdint>
#include <optional>

namespace ioj::levels {
struct EntitySpawnOverrides {
    auto empty() const noexcept -> bool {
        return !health && !laser_damage && !initial_spawn_delay && !spawn_cooldown &&
               target_entity_id.empty();
    }

    std::optional<std::int32_t> health{};
    // Apply damage only to static turrets; the remaining fields require a capital ship.
    std::optional<std::int32_t> laser_damage{};
    std::optional<float> initial_spawn_delay{};
    std::optional<float> spawn_cooldown{};
    // Name an enemy capital that spawns at or before this ship; empty selects automatically.
    EntityId target_entity_id{};
};

struct EntitySpawnDefinition {
    EntityId id{};
    EntityArchetype archetype{EntityArchetype::PlayerFighter};
    Team team{};
    Vector3d position{};
    Rotator3d rotation{};
    double spawn_time_seconds{};
    EntitySpawnOverrides overrides{};
};
}
