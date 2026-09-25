#include "ioj/sim/frame_laser_spawn_requests.h"

#include <algorithm>

namespace ioj::sim::lasers {
FrameSpawnRequests::FrameSpawnRequests(ml::FrameScratch& scratch)
    : locations_{scratch}
    , rotations_{scratch}
    , base_velocities_{scratch}
    , damages_{&scratch}
    , speeds_{&scratch}
    , max_distances_{&scratch}
    , instigator_ids_{&scratch}
    , sources_{&scratch} {}

void FrameSpawnRequests::reserve(std::int32_t const count) {
    locations_.reserve(count);
    rotations_.reserve(count);
    base_velocities_.reserve(count);
    damages_.reserve(count);
    speeds_.reserve(count);
    max_distances_.reserve(count);
    instigator_ids_.reserve(count);
    sources_.reserve(count);
}
void FrameSpawnRequests::set_num(std::int32_t const count) {
    locations_.set_num(count);
    rotations_.set_num(count);
    base_velocities_.set_num(count);
    damages_.set_num(count);
    speeds_.set_num(count);
    max_distances_.set_num(count);
    instigator_ids_.set_num(count);
    sources_.set_num(count);
}
void FrameSpawnRequests::add(Vector3f const location,
                             Rotator3f const rotation,
                             Vector3f const base_velocity,
                             std::int32_t const damage,
                             float const speed,
                             float const max_distance,
                             EntityUniqueId const instigator_id,
                             LaserSource const source) {
    locations_.add(location);
    rotations_.add(rotation);
    base_velocities_.add(base_velocity);
    damages_.add(damage);
    speeds_.add(speed);
    max_distances_.add(max_distance);
    instigator_ids_.add(instigator_id);
    sources_.add(source);
}
void FrameSpawnRequests::set_damages(std::int32_t const value) {
    std::ranges::fill(damages_, value);
}
void FrameSpawnRequests::set_speeds(float const value) {
    std::ranges::fill(speeds_, value);
}
void FrameSpawnRequests::set_max_distances(float const value) {
    std::ranges::fill(max_distances_, value);
}
auto FrameSpawnRequests::num() const noexcept -> std::int32_t {
    return locations_.num();
}
} // namespace lasers
