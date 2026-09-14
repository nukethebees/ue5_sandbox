#include "ioj/sim/fighter_attack_preparation.h"

#include "sandbox/core/projectile_intercept.h"
#include "sandbox/core/vector_normalization.h"

#include <cassert>
#include <cstddef>

namespace ioj::sim::fighters {
void prepare_attack(AttackPreparationView const fighters,
                    AttackPreparationParameters const parameters) {
    auto const count{fighters.locations.num()};
    assert(fighters.target_locations.num() == count);
    assert(fighters.target_velocities.num() == count);
    assert(fighters.intercept_times.size() == static_cast<std::size_t>(count));
    assert(fighters.desired_aiming_directions.num() == count);
    assert(fighters.target_directions.num() == count);
    assert(fighters.desired_move_locations.num() == count);
    assert(fighters.reposition_countdowns.num() == static_cast<std::size_t>(count));

    ml::detail::solve_intercept_times_soa_loop::solve_intercept_times(
        fighters.intercept_times.data(),
        {fighters.locations.xs.data(), fighters.locations.ys.data(), fighters.locations.zs.data()},
        {fighters.target_locations.xs.data(),
         fighters.target_locations.ys.data(),
         fighters.target_locations.zs.data()},
        {fighters.target_velocities.xs.data(),
         fighters.target_velocities.ys.data(),
         fighters.target_velocities.zs.data()},
        parameters.projectile_speed,
        count);

    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const location{fighters.locations[index]};
        auto const target_location{fighters.target_locations[index]};
        auto const intercept_location{target_location + fighters.target_velocities[index] *
                                                            fighters.intercept_times[element]};
        fighters.desired_aiming_directions.set(
            index,
            ml::native_math::safe_normal(intercept_location - location,
                                         parameters.squared_normal_tolerance));

        if (!fighters.reposition_countdowns.try_consume(element)) {
            continue;
        }

        auto const target_to_move_distance{
            HMM_LenV3(target_location - fighters.desired_move_locations[index])};
        if (target_to_move_distance >= parameters.inner_distance &&
            target_to_move_distance <= parameters.outer_distance) {
            continue;
        }

        auto const target_direction{ml::native_math::safe_normal(
            target_location - location, parameters.squared_normal_tolerance)};
        fighters.target_directions.set(index, target_direction);
        fighters.desired_move_locations.set(
            index, target_location - target_direction * parameters.desired_distance);
    }
}
}
