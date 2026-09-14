#pragma once

#include "ioj/sim/fighter_spawn_queue.h"
#include "ioj/sim/fighter_types.h"

namespace ioj::sim::fighters {
struct SpawnInitializationView {
    std::span<FighterTask> tasks;
    Vectors3fView locations;
    Vectors3fView desired_move_locations;
    Vectors3fView aim_directions;
    std::span<float> speeds;
    std::span<std::byte> teams;
    std::span<std::int32_t> healths;
    std::span<RegistryEntityHandle> parents;
    std::span<RegistryEntityHandle> targets;
    std::span<std::uint8_t> navigation_risk_tiers;
    std::span<std::int8_t> avoidance_choices;
    std::span<std::int16_t> navigation_periods;
};

struct SpawnInitializationParameters {
    float speed;
    std::int32_t health;
    std::int16_t navigation_period;
    std::int8_t direct_choice;
};

void initialize_spawned_fighters(SpawnInitializationView fighters,
                                 FighterSpawnQueueConstView spawns,
                                 SpawnInitializationParameters parameters);
}
