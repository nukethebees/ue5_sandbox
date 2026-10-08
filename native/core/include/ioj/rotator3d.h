#pragma once

#include <sandbox/core/quaternion4d.h>

namespace ioj {
// Preserve authored Euler angles in degrees, using Unreal's pitch/yaw/roll convention.
struct Rotator3d {
    double pitch{};
    double yaw{};
    double roll{};
};
auto to_quaternion(Rotator3d rotation) noexcept -> ml::Quaternion4d;
auto to_rotator(ml::Quaternion4d q) noexcept -> Rotator3d;
}
