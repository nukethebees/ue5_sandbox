#include "ioj/sim/frame_laser_spawn_requests.h"

#include <algorithm>

namespace ioj::sim::lasers {
auto FrameSpawnRequests::get_const_view() const -> lasers::SpawnRequestsConstView {
    return {locations.get_const_view(),
            rotations.get_const_view(),
            base_velocities.get_const_view(),
            damages.view(),
            speeds.view(),
            max_distances.view(),
            instigator_ids.view(),
            sources.view()};
}
FrameSpawnRequests::FrameSpawnRequests(ml::FrameScratch& scratch)
    : locations{scratch}
    , rotations{scratch}
    , base_velocities{scratch}
    , damages{&scratch}
    , speeds{&scratch}
    , max_distances{&scratch}
    , instigator_ids{&scratch}
    , sources{&scratch} {}

void FrameSpawnRequests::reserve(std::int32_t const count) {
    locations.reserve(count);
    rotations.reserve(count);
    base_velocities.reserve(count);
    damages.reserve(count);
    speeds.reserve(count);
    max_distances.reserve(count);
    instigator_ids.reserve(count);
    sources.reserve(count);
}
void FrameSpawnRequests::set_num(std::int32_t const count) {
    locations.set_num(count);
    rotations.set_num(count);
    base_velocities.set_num(count);
    damages.set_num(count);
    speeds.set_num(count);
    max_distances.set_num(count);
    instigator_ids.set_num(count);
    sources.set_num(count);
}
void FrameSpawnRequests::add(Vector3f const location,
                             Rotator3f const rotation,
                             Vector3f const base_velocity,
                             std::int32_t const damage,
                             float const speed,
                             float const max_distance,
                             EntityUniqueId const instigator_id,
                             LaserSource const source) {
    locations.add(location);
    rotations.add(rotation);
    base_velocities.add(base_velocity);
    damages.add(damage);
    speeds.add(speed);
    max_distances.add(max_distance);
    instigator_ids.add(instigator_id);
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
} // namespace lasers
