#pragma once

#include "ioj/sim/vectors3f.h"

#include <span>

namespace ioj::sim::fighters {
struct MovementView {
    Vectors3fView locations;
    Vectors3fConstView directions;
    Vectors3fView velocities;
    std::span<float> move_distances;
    std::span<float const> speeds;
};

void move(MovementView fighters, float delta_time) noexcept;
void prepare_movement(Vectors3fView directions,
                      std::span<float> distances,
                      Vectors3fConstView locations,
                      Vectors3fConstView destinations) noexcept;
} // namespace ioj::sim::fighters
