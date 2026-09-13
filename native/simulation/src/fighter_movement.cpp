#include "sandbox/simulation/fighter_movement.h"

#include "sandbox/core/vector_math.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void move(MovementView const fighters, float const delta_time) noexcept {
    assert(delta_time > 0.0f);
    auto const count{fighters.locations.num()};
    auto const size{static_cast<std::size_t>(count)};
    assert(fighters.directions.num() == count);
    assert(fighters.velocities.num() == count);
    assert(fighters.move_distances.size() == size);
    assert(fighters.speeds.size() == size);

    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const max_move_distance{fighters.speeds[element] * delta_time};
        auto const requested_move_distance{fighters.move_distances[element]};
        auto const move_distance{requested_move_distance < max_move_distance
                                     ? requested_move_distance
                                     : max_move_distance};
        fighters.move_distances[element] = move_distance;
        auto const velocity_scale{move_distance / delta_time};
        fighters.velocities.xs[element] = fighters.directions.xs[element] * velocity_scale;
        fighters.velocities.ys[element] = fighters.directions.ys[element] * velocity_scale;
        fighters.velocities.zs[element] = fighters.directions.zs[element] * velocity_scale;
    }

    ml::native_math::add_scaled_product_in_place(fighters.locations.xs.data(),
                                                 fighters.locations.ys.data(),
                                                 fighters.locations.zs.data(),
                                                 fighters.directions.xs.data(),
                                                 fighters.directions.ys.data(),
                                                 fighters.directions.zs.data(),
                                                 fighters.move_distances.data(),
                                                 1.0f,
                                                 count);
}
} // namespace ml::simulation::fighters
