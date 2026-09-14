#include "ioj/sim/rotator_math.h"

#include "sandbox/core/trigonometry.h"
#include "sandbox/core/vector_math.h"

#include <cmath>
#include <numbers>

namespace ioj::sim {
auto direction_to_rotation(Vector3f const direction) noexcept -> Rotator3f {
    Rotator3f rotation{};
    ml::native_math::to_rotations(&rotation.pitch,
                                  &rotation.yaw,
                                  &rotation.roll,
                                  &direction.X,
                                  &direction.Y,
                                  &direction.Z,
                                  1);
    return rotation;
}

auto to_quaternion(Rotator3f const rotation) noexcept -> Quaternion4f {
    constexpr float half_radians_per_degree{std::numbers::pi_v<float> / 360.f};
    float sp{}, cp{}, sy{}, cy{}, sr{}, cr{};
    ml::native_math::sin_cos(std::fmod(rotation.pitch, 360.f) * half_radians_per_degree, sp, cp);
    ml::native_math::sin_cos(std::fmod(rotation.yaw, 360.f) * half_radians_per_degree, sy, cy);
    ml::native_math::sin_cos(std::fmod(rotation.roll, 360.f) * half_radians_per_degree, sr, cr);
    return HMM_Q(cr * sp * sy - sr * cp * cy,
                 -cr * sp * cy - sr * cp * sy,
                 cr * cp * sy - sr * sp * cy,
                 cr * cp * cy + sr * sp * sy);
}

auto forward_direction(Rotator3f const rotation) noexcept -> Vector3f {
    constexpr float radians_per_degree{std::numbers::pi_v<float> / 180.f};
    float sin_pitch{}, cos_pitch{}, sin_yaw{}, cos_yaw{};
    ml::native_math::sin_cos(
        std::fmod(rotation.pitch, 360.f) * radians_per_degree, sin_pitch, cos_pitch);
    ml::native_math::sin_cos(std::fmod(rotation.yaw, 360.f) * radians_per_degree, sin_yaw, cos_yaw);
    return HMM_V3(cos_pitch * cos_yaw, cos_pitch * sin_yaw, sin_pitch);
}
}
