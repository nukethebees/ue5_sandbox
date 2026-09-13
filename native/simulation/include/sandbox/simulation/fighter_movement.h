#pragma once

#include "sandbox/simulation/vectors3f.h"

#include <span>

namespace ml::simulation::fighters {
struct MovementView {
    Vectors3fView locations;
    Vectors3fConstView directions;
    Vectors3fView velocities;
    std::span<float> move_distances;
    std::span<float const> speeds;
};

void move(MovementView fighters, float delta_time) noexcept;
} // namespace ml::simulation::fighters
