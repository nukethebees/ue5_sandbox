#pragma once

#include "sandbox/simulation/vectors3f.h"

namespace ml::simulation::fighters {
void update_movement_aiming(Vectors3fView aim_directions,
                            Vectors3fConstView movement_directions,
                            float turn_fraction) noexcept;
void update_attack_aiming(Vectors3fView aim_directions,
                          Vectors3fConstView movement_directions,
                          Vectors3fConstView desired_aiming_directions,
                          std::span<std::int8_t const> avoidance_choices,
                          float turn_fraction) noexcept;
}
