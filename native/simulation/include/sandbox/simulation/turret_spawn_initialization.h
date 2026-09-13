#pragma once

#include "sandbox/simulation/turret_spawn_data.h"

namespace ml::simulation::turrets {
struct SpawnInitializationView {
    Vectors3fView locations;
    Vectors3fView fire_point_locations;
    std::span<std::byte> teams;
    std::span<std::int32_t> healths;
    std::span<std::int32_t> laser_damages;
    std::span<std::int16_t> refresh_remaining_ticks;
    std::span<std::int16_t> refresh_periods;
};

void initialize_spawned_turrets(SpawnInitializationView turrets,
                                TurretSpawnDataConstView spawns,
                                Vector3f fire_point_offset,
                                std::int16_t refresh_period,
                                std::int32_t& next_offset);
}
