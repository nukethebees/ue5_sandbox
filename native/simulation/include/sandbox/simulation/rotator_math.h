#pragma once

#include "sandbox/core/math_types.h"
#include "sandbox/simulation/rotator_types.h"

namespace ml::simulation {
[[nodiscard]] auto forward_direction(Rotator3f rotation) noexcept -> Vector3f;
}
