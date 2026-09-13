#pragma once

#include "sandbox/core/tick_countdown.h"
#include "sandbox/simulation/vectors3f.h"

namespace ml::simulation::fighters {
struct AttackPreparationView {
    Vectors3fConstView locations;
    Vectors3fConstView target_locations;
    Vectors3fConstView target_velocities;
    std::span<float> intercept_times;
    Vectors3fView desired_aiming_directions;
    Vectors3fView target_directions;
    Vectors3fView desired_move_locations;
    TickCountdownView<std::int16_t> reposition_countdowns;
};

struct AttackPreparationParameters {
    float projectile_speed;
    float desired_distance;
    float inner_distance;
    float outer_distance;
    float squared_normal_tolerance;
};

void prepare_attack(AttackPreparationView fighters, AttackPreparationParameters parameters);
}
