#pragma once

#include "ioj/math_types.h"
#include "ioj/rotator_types.h"
#include <ioj/rotator3d.h>

namespace ioj {

[[nodiscard]] auto forward_direction(Rotator3f rotation) noexcept -> Vector3f;
[[nodiscard]] auto to_quaternion(Rotator3f rotation) noexcept -> Quaternion4f;
[[nodiscard]] auto direction_to_rotation(Vector3f direction) noexcept -> Rotator3f;
}
