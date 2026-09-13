#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/capital_spawn_data.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/rotators3f.h"
#include "sandbox/simulation/vectors3f.h"

namespace ml::simulation::capitals {
struct SpawnInitializationView {
    Vectors3fView locations;
    Rotators3fView rotations;
    std::span<float> remaining_spawn_times;
    std::span<float> spawn_cooldowns;
    std::span<std::byte> teams;
    std::span<std::int32_t> healths;
    std::span<FRegistryEntityHandle> targets;
};

void initialize_spawned_ships(SpawnInitializationView ships, CapitalSpawnDataConstView spawns);
void collect_ships_ready_to_spawn_fighters(std::span<float const> remaining_times,
                                           std::span<FRegistryEntityHandle const> targets,
                                           FrameArray<std::int32_t>& indices);
}
