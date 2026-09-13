#pragma once

#include "sandbox/simulation/rotators3f.h"
#include "sandbox/simulation/vectors3f.h"

#include <cstdint>
#include <span>

namespace ml::simulation::lasers {
void initialise_spawns(Vectors3fView locations,
                       Vectors3fView velocities,
                       Rotators3fConstView rotations,
                       Vectors3fConstView base_velocities,
                       std::span<float const> speeds,
                       std::span<float const> max_distances,
                       std::span<float> lifetimes_remaining,
                       std::span<float> initial_lifetimes,
                       std::span<float> spawn_times,
                       std::int32_t output_offset,
                       float tick_period,
                       float simulation_time,
                       float fixed_spawn_offset) noexcept;
} // namespace ml::simulation::lasers
