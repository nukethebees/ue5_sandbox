#pragma once

#include <ioj/rotator3d.h>
#include <ioj/sim/rotator_types.h>

#include <sandbox/core/quaternion4d.h>

namespace ioj::sim {
auto to_quaternion(Rotator3d rotation) noexcept -> ml::Quaternion4d;
auto to_rotator(ml::Quaternion4d q) noexcept -> Rotator3d;
inline auto to_float(Rotator3d const rotation) noexcept -> Rotator3f {
    return {static_cast<float>(rotation.pitch),
            static_cast<float>(rotation.yaw),
            static_cast<float>(rotation.roll)};
}
}
