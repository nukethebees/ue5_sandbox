#pragma once

#include <ioj/sim/column_math.h>
#include <ioj/sim/laser_soa.h>

namespace ioj::sim::tests {
inline void add_laser_spawn(lasers::SingleAllocationLaserSpawnRequests& requests,
                            Vector3f const location,
                            Rotator3f const rotation,
                            Vector3f const velocity,
                            std::int32_t const damage,
                            float const speed,
                            float const distance,
                            EntityUniqueId const instigator,
                            LaserSource const source) {
    auto const row{requests.num()};
    requests.add_uninitialised(1);
    auto const view{requests.get_view()};
    set_vector(view.view_locations(), row, location);
    set_rotation(view.view_rotations(), row, rotation);
    set_vector(view.view_base_velocities(), row, velocity);
    view.damages()[row] = damage;
    view.speeds()[row] = speed;
    view.max_distances()[row] = distance;
    view.instigator_ids()[row] = instigator;
    view.sources()[row] = source;
}
}
