#pragma once

#include <ioj/quaternion.h>

namespace ioj {
// Preserve authored Euler angles in degrees, using Unreal's pitch/yaw/roll convention.
struct Rotator3d {
    double pitch{};
    double yaw{};
    double roll{};

    auto operator==(Rotator3d const&) const -> bool = default;
};
auto to_quaternion(Rotator3d rotation) noexcept -> Quaternion4d;
auto to_rotator(Quaternion4d q) noexcept -> Rotator3d;
}
