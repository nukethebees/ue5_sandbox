#pragma once

#include <ioj/linear_travel_state.h>
#include <ioj/linear_travel_step.h>

namespace ioj {
// Positions must be finite; speed, elapsed time and pause duration must be finite and nonnegative.
[[nodiscard]] auto advance_linear_travel(LinearTravelState& state,
                                         Vector3d location,
                                         double speed,
                                         double elapsed_seconds,
                                         double pause_seconds = 0.) noexcept -> LinearTravelStep;
}
