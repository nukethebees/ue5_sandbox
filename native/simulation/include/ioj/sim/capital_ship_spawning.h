#pragma once

#include "ioj/sim/capital_spawn_data.h"
#include "ioj/sim/entity_handle.h"
#include "ioj/sim/rotators3f.h"
#include "ioj/sim/vectors3f.h"
#include "sandbox/core/frame_array.h"

namespace ioj::sim::capitals {
struct SpawnInitializationView {
    Vectors3fView locations;
    Rotators3fView rotations;
    std::span<float> remaining_spawn_times;
    std::span<float> spawn_cooldowns;
    std::span<std::byte> teams;
    std::span<std::int32_t> healths;
    std::span<RegistryEntityHandle> targets;
};

void initialize_spawned_ships(SpawnInitializationView ships, CapitalSpawnDataConstView spawns);
void collect_ships_ready_to_spawn_fighters(std::span<float const> remaining_times,
                                           std::span<RegistryEntityHandle const> targets,
                                           ml::FrameArray<std::int32_t>& indices);
}
