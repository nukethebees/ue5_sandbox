#pragma once

#include <ioj/levels/team.h>
#include <ioj/math_types.h>
#include <ioj/rotator_types.h>
#include <ioj/sim/levels/level_entity_index.h>

#include <cstdint>
#include <optional>
#include <span>

namespace ioj::sim {
struct LevelSimInitData;

struct InitialCapital {
    LevelEntityIndex entity;
    LevelEntityIndex target{invalid_level_entity_index};
    Vector3f location;
    Rotator3f rotation;
    Team team;
    std::optional<std::int32_t> health;
    std::optional<float> initial_spawn_delay;
    std::optional<float> spawn_cooldown;
};

struct InitialTurret {
    LevelEntityIndex entity;
    Vector3f location;
    Rotator3f rotation;
    Team team;
    std::optional<std::int32_t> health;
    std::optional<std::int32_t> laser_damage;
};

struct InitialSpinner {
    LevelEntityIndex entity;
    Vector3f location;
    float yaw;
    std::int32_t initial_fire_point;
};

// Populate an empty schedule, preserving input order within each entity type.
// Entity indices are assigned by the host and must belong to data.level_events.initialisation.
void initialise_spawns(LevelSimInitData& data,
                       std::span<InitialCapital const> capitals,
                       std::span<InitialTurret const> turrets,
                       std::span<InitialSpinner const> spinners);
}
