#include "ioj/sim/lasers/frame_laser_spawn_requests.h"

#include <ioj/column_math.h>

#include <algorithm>

namespace ioj::sim::lasers {
FrameSpawnRequests::FrameSpawnRequests(std::pmr::memory_resource* const scratch_resource)
    : storage_{scratch_resource} {}

void FrameSpawnRequests::reserve(std::uint32_t const count) {
    storage_.reserve(count);
}
void FrameSpawnRequests::set_num(std::uint32_t const count) {
    storage_.set_num(count);
}
void FrameSpawnRequests::add(Vector3f const location,
                             Rotator3f const rotation,
                             Vector3f const base_velocity,
                             std::int32_t const damage,
                             float const speed,
                             float const max_distance,
                             EntityUniqueId const instigator_id,
                             LaserSource const source) {
    auto const index{storage_.num()};
    storage_.add_uninitialised(1);
    auto const view{storage_.get_view()};
    view.view_locations().set(index, location);
    set_rotation(view.view_rotations(), index, rotation);
    view.view_base_velocities().set(index, base_velocity);
    view.damages()[index] = damage;
    view.speeds()[index] = speed;
    view.max_distances()[index] = max_distance;
    view.instigator_ids()[index] = instigator_id;
    view.sources()[index] = source;
}
void FrameSpawnRequests::set_damages(std::int32_t const value) {
    std::ranges::fill(damages(), value);
}
void FrameSpawnRequests::set_speeds(float const value) {
    std::ranges::fill(speeds(), value);
}
void FrameSpawnRequests::set_max_distances(float const value) {
    std::ranges::fill(max_distances(), value);
}
auto FrameSpawnRequests::num() const noexcept -> std::uint32_t {
    return storage_.num();
}
} // namespace lasers
