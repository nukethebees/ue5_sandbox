#include "sandbox/simulation/laser_spawn_initialization.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::lasers {
namespace laser_spawn_initialization_detail {
auto forward_direction(Rotator3f const rotation) noexcept -> Vector3f {
    auto const pitch{HMM_AngleDeg(rotation.pitch)};
    auto const yaw{HMM_AngleDeg(rotation.yaw)};
    auto const cos_pitch{HMM_CosF(pitch)};
    return HMM_V3(cos_pitch * HMM_CosF(yaw), cos_pitch * HMM_SinF(yaw), HMM_SinF(pitch));
}
} // namespace laser_spawn_initialization_detail

void initialise_spawns(Vectors3fView const locations,
                       Vectors3fView const velocities,
                       Rotators3fConstView const rotations,
                       Vectors3fConstView const base_velocities,
                       std::span<float const> const speeds,
                       std::span<float const> const max_distances,
                       std::span<float> const lifetimes_remaining,
                       std::span<float> const initial_lifetimes,
                       std::span<float> const spawn_times,
                       std::int32_t const output_offset,
                       float const tick_period,
                       float const simulation_time,
                       float const fixed_spawn_offset) noexcept {
    locations.validate_array_sizes();
    velocities.validate_array_sizes();
    rotations.validate_array_sizes();
    base_velocities.validate_array_sizes();
    assert(locations.num() == velocities.num());
    assert(locations.num() == rotations.num());
    assert(lifetimes_remaining.size() == static_cast<std::size_t>(locations.num()));
    assert(initial_lifetimes.size() == lifetimes_remaining.size());
    assert(spawn_times.size() == lifetimes_remaining.size());
    assert(speeds.size() == max_distances.size());
    assert(base_velocities.num() == static_cast<std::int32_t>(speeds.size()));
    assert(output_offset >= 0);
    assert(speeds.size() <= static_cast<std::size_t>(locations.num() - output_offset));

    auto const spawn_count{static_cast<std::int32_t>(speeds.size())};
    for (std::int32_t spawn_index{}; spawn_index < spawn_count; ++spawn_index) {
        auto const spawn_element{static_cast<std::size_t>(spawn_index)};
        auto const output_index{output_offset + spawn_index};
        auto const output_element{static_cast<std::size_t>(output_index)};
        auto const speed{speeds[spawn_element]};
        auto const lifetime{max_distances[spawn_element] / speed};
        auto const direction{
            laser_spawn_initialization_detail::forward_direction(rotations[output_index])};
        auto const forward_velocity{direction * speed};
        auto const velocity{base_velocities[spawn_index] + forward_velocity};
        auto const spawn_location{locations[output_index] + forward_velocity * tick_period +
                                  direction * fixed_spawn_offset};

        locations.set(output_index, spawn_location);
        velocities.set(output_index, velocity);
        lifetimes_remaining[output_element] = lifetime;
        initial_lifetimes[output_element] = lifetime;
        spawn_times[output_element] = simulation_time;
    }
}
} // namespace ml::simulation::lasers
