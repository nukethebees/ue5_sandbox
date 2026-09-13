#pragma once

#include "sandbox/simulation/fighter_spawn_queue.h"
#include "sandbox/simulation/fighter_types.h"

namespace ml::simulation::fighters {
struct SpawnInitializationView {
    std::span<CapitalShipFighterTask> tasks;
    Vectors3fView locations;
    Vectors3fView desired_move_locations;
    Vectors3fView aim_directions;
    std::span<float> speeds;
    std::span<std::byte> teams;
    std::span<std::int32_t> healths;
    std::span<FRegistryEntityHandle> parents;
    std::span<FRegistryEntityHandle> targets;
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
                                 TestCapitalShipFighterSpawnQueueConstView spawns,
                                 SpawnInitializationParameters parameters);
}
