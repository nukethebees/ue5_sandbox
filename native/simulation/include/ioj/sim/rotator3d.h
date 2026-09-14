#pragma once

#include <ioj/sim/rotator_types.h>
#include <sandbox/core/quaternion4d.h>

namespace ioj::sim {
struct Rotator3d {
    double pitch{};
    double yaw{};
    double roll{};
};
auto to_quaternion(Rotator3d rotation) noexcept -> ml::Quaternion4d;
auto to_rotator(ml::Quaternion4d rotation) noexcept -> Rotator3d;
inline auto to_float(Rotator3d const rotation) noexcept -> Rotator3f {
    return {static_cast<float>(rotation.pitch),
            static_cast<float>(rotation.yaw),
            static_cast<float>(rotation.roll)};
}
}
