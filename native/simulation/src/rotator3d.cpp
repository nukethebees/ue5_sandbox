#include <algorithm>
#include <cmath>
#include <ioj/sim/rotator3d.h>
#include <numbers>

namespace ioj::sim {
auto to_quaternion(Rotator3d const rotation) noexcept -> ml::Quaternion4d {
    auto const half_radians{std::numbers::pi / 360.0};
    auto const pitch{std::fmod(rotation.pitch, 360.0) * half_radians};
    auto const yaw{std::fmod(rotation.yaw, 360.0) * half_radians};
    auto const roll{std::fmod(rotation.roll, 360.0) * half_radians};
    auto const sp{std::sin(pitch)};
    auto const cp{std::cos(pitch)};
    auto const sy{std::sin(yaw)};
    auto const cy{std::cos(yaw)};
    auto const sr{std::sin(roll)};
    auto const cr{std::cos(roll)};
    return {cr * sp * sy - sr * cp * cy,
            -cr * sp * cy - sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy};
}
auto to_rotator(ml::Quaternion4d const q) noexcept -> Rotator3d {
    auto const degrees{180.0 / std::numbers::pi};
    auto const singularity{q.z * q.x - q.w * q.y};
    if (std::abs(singularity) > 0.4999995) {
        auto yaw{
            std::fmod((singularity < 0.0 ? -2.0 : 2.0) * std::atan2(q.x, q.w) * degrees, 360.0)};
        if (yaw < 0.0) {
            yaw += 360.0;
        }
        if (yaw > 180.0) {
            yaw -= 360.0;
        }
        return {singularity < 0.0 ? -90.0 : 90.0, yaw, 0.0};
    }
    return {
        std::asin(std::clamp(2.0 * singularity, -1.0, 1.0)) * degrees,
        std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)) * degrees,
        std::atan2(-2.0 * (q.w * q.x + q.y * q.z), 1.0 - 2.0 * (q.x * q.x + q.y * q.y)) * degrees};
}
}
