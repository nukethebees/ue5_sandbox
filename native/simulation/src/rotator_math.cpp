#include "sandbox/simulation/rotator_math.h"

#include "sandbox/core/trigonometry.h"

#include <cmath>
#include <numbers>

namespace ml::simulation {
auto forward_direction(Rotator3f const rotation) noexcept -> Vector3f {
    constexpr float radians_per_degree{std::numbers::pi_v<float> / 180.f};
    float sin_pitch{}, cos_pitch{}, sin_yaw{}, cos_yaw{};
    ml::native_math::sin_cos(
        std::fmod(rotation.pitch, 360.f) * radians_per_degree, sin_pitch, cos_pitch);
    ml::native_math::sin_cos(std::fmod(rotation.yaw, 360.f) * radians_per_degree, sin_yaw, cos_yaw);
    return HMM_V3(cos_pitch * cos_yaw, cos_pitch * sin_yaw, sin_pitch);
}
}
