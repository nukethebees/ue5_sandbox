#pragma once

namespace ioj {
// Preserve authored Euler angles in degrees, using Unreal's pitch/yaw/roll convention.
struct Rotator3d {
    double pitch{};
    double yaw{};
    double roll{};
};
}
