#include "ioj/rotator_math.h"

#include "ioj/trigonometry.h"
#include "ioj/vector_math.h"

#include <cmath>
#include <numbers>

namespace ioj {
auto direction_to_rotation(Vector3f const direction) noexcept -> Rotator3f {
    Rotator3f rotation{};
    ioj::native_math::to_rotations(&rotation.pitch,
                                  &rotation.yaw,
                                  &rotation.roll,
                                  &direction.x(),
                                  &direction.y(),
                                  &direction.z(),
                                  1);
    return rotation;
}

auto to_quaternion(Rotator3f const rotation) noexcept -> Quaternion4f {
    constexpr float half_radians_per_degree{std::numbers::pi_v<float> / 360.f};
    float sp{};
    float cp{};
    float sy{};
    float cy{};
    float sr{};
    float cr{};
    ioj::native_math::sin_cos(std::fmod(rotation.pitch, 360.f) * half_radians_per_degree, sp, cp);
    ioj::native_math::sin_cos(std::fmod(rotation.yaw, 360.f) * half_radians_per_degree, sy, cy);
    ioj::native_math::sin_cos(std::fmod(rotation.roll, 360.f) * half_radians_per_degree, sr, cr);
    return ioj::make_quaternion4f(cr * sp * sy - sr * cp * cy,
                                 -cr * sp * cy - sr * cp * sy,
                                 cr * cp * sy - sr * sp * cy,
                                 cr * cp * cy + sr * sp * sy);
}

auto forward_direction(Rotator3f const rotation) noexcept -> Vector3f {
    constexpr float radians_per_degree{std::numbers::pi_v<float> / 180.f};
    float sin_pitch{};
    float cos_pitch{};
    float sin_yaw{};
    float cos_yaw{};
    ioj::native_math::sin_cos(
        std::fmod(rotation.pitch, 360.f) * radians_per_degree, sin_pitch, cos_pitch);
    ioj::native_math::sin_cos(std::fmod(rotation.yaw, 360.f) * radians_per_degree, sin_yaw, cos_yaw);
    return ioj::make_vector3f(cos_pitch * cos_yaw, cos_pitch * sin_yaw, sin_pitch);
}
}
