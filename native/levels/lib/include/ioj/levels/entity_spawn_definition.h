#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/levels/entity_archetype.h>
#include <ioj/levels/identifiers.h>
#include <ioj/levels/team.h>
#include <ioj/rotator3d.h>
#include <ioj/settings/simulation_constraints.h>
#include <ioj/vector_value_math.h>

#include <cstdint>
#include <optional>

namespace ioj::levels {
struct EntitySpawnOverrides {
    auto empty() const noexcept -> bool {
        return !health && !laser_damage && !initial_spawn_delay && !spawn_cooldown &&
               !initial_fire_point;
    }

    std::optional<settings::PositiveHealth> health{};
    std::optional<NonNegative<std::int32_t>> laser_damage{};
    std::optional<NonNegativeFinite<float>> initial_spawn_delay{};
    std::optional<NonNegativeFinite<float>> spawn_cooldown{};
    std::optional<NonNegative<std::int32_t>> initial_fire_point{};
};

struct EntitySpawnDefinition {
    EntityId id{};
    EntityArchetype archetype{EntityArchetype::PlayerFighter};
    Team team{};
    Vector3d position{};
    Rotator3d rotation{};
    NonNegativeFinite<double> spawn_time_seconds{};
    EntitySpawnOverrides overrides{};
};
}
