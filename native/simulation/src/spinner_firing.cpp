#include "ioj/sim/spinner_firing.h"

#include <cassert>

namespace ioj::sim::spinners {
void fire_lasers(FiringView const spinners,
                 std::span<FirePoint const> const fire_points,
                 FiringParameters const parameters,
                 std::pmr::memory_resource& resource,
                 lasers::FrameSpawnRequests& requests) {
    if (fire_points.empty()) {
        return;
    }

    auto const count{spinners.locations.num()};
    assert(spinners.yaws.size() == static_cast<std::size_t>(count));
    assert(spinners.handles.size() == static_cast<std::size_t>(count));
    assert(spinners.next_fire_point_indices.size() == static_cast<std::size_t>(count));
    assert(spinners.cooldowns.num() == static_cast<std::size_t>(count));
    ml::FrameArray<std::int32_t> ready_indices{&resource};
    ready_indices.reserve(count);
    for (std::int32_t index{}; index < count; ++index) {
        if (spinners.cooldowns.try_consume(static_cast<std::size_t>(index))) {
            ready_indices.add(index);
        }
    }

    auto const ready_count{ready_indices.num()};
    auto const fire_point_count{static_cast<std::int32_t>(fire_points.size())};
    requests.set_num(ready_count);
    for (std::int32_t request_index{}; request_index < ready_count; ++request_index) {
        auto const index{ready_indices[request_index]};
        auto const element{static_cast<std::size_t>(index)};
        auto& next_fire_point{spinners.next_fire_point_indices[element]};
        assert(next_fire_point >= 0 && next_fire_point < fire_point_count);
        auto const& fire_point{fire_points[static_cast<std::size_t>(next_fire_point)]};
        requests.locations.set(request_index, spinners.locations[index] + fire_point.location);
        requests.rotations.set(request_index,
                               {fire_point.rotation.pitch,
                                fire_point.rotation.yaw + spinners.yaws[element],
                                fire_point.rotation.roll});
        requests.base_velocities.set(request_index, HMM_V3(0.f, 0.f, 0.f));
        requests.damages[request_index] = parameters.damage;
        requests.speeds[request_index] = parameters.speed;
        requests.max_distances[request_index] = parameters.maximum_distance;
        requests.instigator_handles[request_index] = spinners.handles[element];
        requests.sources[request_index] = {Team::White, EntityType::TubeSpinner};
        next_fire_point = (next_fire_point + 1) % fire_point_count;
    }
}
}
