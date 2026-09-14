#pragma once

#include "ioj/sim/rotator_types.h"
#include "sandbox/core/math_types.h"

namespace ioj::sim {
[[nodiscard]] auto forward_direction(Rotator3f rotation) noexcept -> Vector3f;
[[nodiscard]] auto to_quaternion(Rotator3f rotation) noexcept -> Quaternion4f;
[[nodiscard]] auto direction_to_rotation(Vector3f direction) noexcept -> Rotator3f;
}
