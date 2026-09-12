#include "sandbox/simulation/frame_laser_spawn_requests.h"

#include <algorithm>

namespace ml::simulation::lasers {
FrameSpawnRequests::FrameSpawnRequests(std::pmr::memory_resource* const resource)
    : locations{resource}
    , rotations{resource}
    , base_velocities{resource}
    , damages{resource}
    , speeds{resource}
    , max_distances{resource}
    , instigator_handles{resource}
    , sources{resource} {}

void FrameSpawnRequests::reserve(std::int32_t const count) {
    locations.reserve(count);
    rotations.reserve(count);
    base_velocities.reserve(count);
    damages.reserve(count);
    speeds.reserve(count);
    max_distances.reserve(count);
    instigator_handles.reserve(count);
    sources.reserve(count);
}
void FrameSpawnRequests::set_num(std::int32_t const count) {
    locations.set_num(count);
    rotations.set_num(count);
    base_velocities.set_num(count);
    damages.set_num(count);
    speeds.set_num(count);
    max_distances.set_num(count);
    instigator_handles.set_num(count);
    sources.set_num(count);
}
void FrameSpawnRequests::add(Vector3f const location,
                             Rotator3f const rotation,
                             Vector3f const base_velocity,
                             std::int32_t const damage,
                             float const speed,
                             float const max_distance,
                             FRegistryEntityHandle const instigator_handle,
                             LaserSource const source) {
    locations.add(location);
    rotations.add(rotation);
    base_velocities.add(base_velocity);
    damages.add(damage);
    speeds.add(speed);
    max_distances.add(max_distance);
    instigator_handles.add(instigator_handle);
    sources.add(source);
}
void FrameSpawnRequests::set_damages(std::int32_t const value) {
    std::ranges::fill(damages, value);
}
void FrameSpawnRequests::set_speeds(float const value) {
    std::ranges::fill(speeds, value);
}
void FrameSpawnRequests::set_max_distances(float const value) {
    std::ranges::fill(max_distances, value);
}
auto FrameSpawnRequests::num() const noexcept -> std::int32_t {
    return locations.num();
}
} // namespace ml::simulation::lasers
