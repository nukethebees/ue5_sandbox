#pragma once

#include "ioj/sim/rotator_types.h"
#include "ioj/sim/vector_types.h"
#include <ioj/rotator3d.h>

namespace ioj::sim {
inline auto to_float(Rotator3d const rotation) noexcept -> Rotator3f {
    return {static_cast<float>(rotation.pitch),
            static_cast<float>(rotation.yaw),
            static_cast<float>(rotation.roll)};
}

[[nodiscard]] auto forward_direction(Rotator3f rotation) noexcept -> Vector3f;
[[nodiscard]] auto to_quaternion(Rotator3f rotation) noexcept -> Quaternion4f;
[[nodiscard]] auto direction_to_rotation(Vector3f direction) noexcept -> Rotator3f;
}
