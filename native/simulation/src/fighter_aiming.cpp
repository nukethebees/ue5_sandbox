#include "sandbox/simulation/fighter_aiming.h"
#include "sandbox/core/generated/array_math_kernels.h"

#include "sandbox/simulation/fighter_navigation.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void update_movement_aiming(Vectors3fView const aim_directions,
                            Vectors3fConstView const movement_directions,
                            float const turn_fraction) noexcept {
    assert(aim_directions.num() == movement_directions.num());
    ml::lerp_1d_in_place(aim_directions.xs, movement_directions.xs, turn_fraction);
    ml::lerp_1d_in_place(aim_directions.ys, movement_directions.ys, turn_fraction);
    ml::lerp_1d_in_place(aim_directions.zs, movement_directions.zs, turn_fraction);
}

void update_attack_aiming(Vectors3fView const aim_directions,
                          Vectors3fConstView const movement_directions,
                          Vectors3fConstView const desired_aiming_directions,
                          std::span<std::int8_t const> const avoidance_choices,
                          float const turn_fraction) noexcept {
    auto const count{aim_directions.num()};
    assert(movement_directions.num() == count);
    assert(desired_aiming_directions.num() == count);
    assert(avoidance_choices.size() == static_cast<std::size_t>(count));

    for (std::int32_t index{}; index < count; ++index) {
        auto const choice{avoidance_choices[static_cast<std::size_t>(index)]};
        auto const desired_direction{is_avoidance_direction_choice(choice)
                                         ? movement_directions[index]
                                         : desired_aiming_directions[index]};
        auto const current_direction{aim_directions[index]};
        aim_directions.set(
            index, current_direction + (desired_direction - current_direction) * turn_fraction);
    }
}
}
