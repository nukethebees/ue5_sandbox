#pragma once

#include <ioj/math_types.h>

namespace ioj {
struct LinearTravelState {
    Vector3d start{};
    Vector3d end{};
    double pause_remaining{};
    bool towards_end{true};
};

struct LinearTravelStep {
    Vector3d location{};
    bool arrived{};
};

// Positions must be finite; speed, elapsed time and pause duration must be finite and nonnegative.
[[nodiscard]] auto advance_linear_travel(LinearTravelState& state,
                                         Vector3d location,
                                         double speed,
                                         double elapsed_seconds,
                                         double pause_seconds = 0.) noexcept -> LinearTravelStep;
}
