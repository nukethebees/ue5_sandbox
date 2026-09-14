#include "ioj/sim/laser_spawn_initialization.h"

namespace ioj::sim::lasers {
namespace laser_spawn_initialization_detail {
auto forward_direction(Rotator3f const rotation) noexcept -> Vector3f {
    auto const pitch{HMM_AngleDeg(rotation.pitch)};
    auto const yaw{HMM_AngleDeg(rotation.yaw)};
    auto const cos_pitch{HMM_CosF(pitch)};
    return HMM_V3(cos_pitch * HMM_CosF(yaw), cos_pitch * HMM_SinF(yaw), HMM_SinF(pitch));
}
} // namespace laser_spawn_initialization_detail

void initialise_spawns(Entities& entities,
                       SpawnRequestsConstView const requests,
                       float const tick_period,
                       float const simulation_time,
                       float const fixed_spawn_offset) {
    requests.validate_array_sizes();
    auto const spawn_count{requests.num()};
    entities.add_defaulted(spawn_count);
    auto const output{entities.get_view().right(spawn_count)};

    for (std::int32_t spawn_index{}; spawn_index < spawn_count; ++spawn_index) {
        auto const speed{requests.speeds[spawn_index]};
        auto const lifetime{requests.max_distances[spawn_index] / speed};
        auto const rotation{requests.rotations[spawn_index]};
        auto const direction{laser_spawn_initialization_detail::forward_direction(rotation)};
        auto const forward_velocity{direction * speed};
        auto const velocity{requests.base_velocities[spawn_index] + forward_velocity};
        auto const spawn_location{requests.locations[spawn_index] + forward_velocity * tick_period +
                                  direction * fixed_spawn_offset};

        output.locations.set(spawn_index, spawn_location);
        output.rotations.set(spawn_index, rotation);
        output.velocities.set(spawn_index, velocity);
        output.sources[spawn_index] = requests.sources[spawn_index];
        output.damages[spawn_index] = requests.damages[spawn_index];
        output.instigator_handles[spawn_index] = requests.instigator_handles[spawn_index];
        output.lifetimes_remaining[spawn_index] = lifetime;
        output.initial_lifetimes[spawn_index] = lifetime;
        output.spawn_times[spawn_index] = simulation_time;
    }
}
} // namespace ioj::sim::lasers
