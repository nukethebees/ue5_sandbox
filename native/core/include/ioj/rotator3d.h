#pragma once

#include <ioj/quaternion.h>

namespace ioj {
// Preserve authored Euler angles in degrees, using Unreal's pitch/yaw/roll convention.
struct Rotator3d {
    double pitch{};
    double yaw{};
    double roll{};
};
auto to_quaternion(Rotator3d rotation) noexcept -> ioj::Quaternion4d;
auto to_rotator(ioj::Quaternion4d q) noexcept -> Rotator3d;
}
