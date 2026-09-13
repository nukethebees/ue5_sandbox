#include "sandbox/core/trigonometry.h"

#include <cstdint>

namespace ml::native_math {
namespace trigonometry_detail {
inline constexpr float pi{3.14159265358979323846f};
inline constexpr float two_pi{6.28318530717958647692f};
inline constexpr float half_pi{1.57079632679489661923f};
inline constexpr float inverse_pi{0.31830988618379067154f};
}
void sin_cos(float const value, float& sine, float& cosine) noexcept {
    using namespace trigonometry_detail;
    // Match FMath::SinCos so moving this calculation across the library boundary is neutral.
    auto quotient{inverse_pi * 0.5f * value};
    quotient = value >= 0.0f ? static_cast<float>(static_cast<std::int64_t>(quotient + 0.5f))
                             : static_cast<float>(static_cast<std::int64_t>(quotient - 0.5f));
    auto angle{value - two_pi * quotient};

    float cosine_sign;
    if (angle > half_pi) {
        angle = pi - angle;
        cosine_sign = -1.0f;
    } else if (angle < -half_pi) {
        angle = -pi - angle;
        cosine_sign = -1.0f;
    } else {
        cosine_sign = 1.0f;
    }

    auto const angle_squared{angle * angle};
    sine =
        (((((-2.3889859e-08f * angle_squared + 2.7525562e-06f) * angle_squared - 0.00019840874f) *
               angle_squared +
           0.0083333310f) *
              angle_squared -
          0.16666667f) *
             angle_squared +
         1.0f) *
        angle;
    auto const cosine_polynomial{
        ((((-2.6051615e-07f * angle_squared + 2.4760495e-05f) * angle_squared - 0.0013888378f) *
              angle_squared +
          0.041666638f) *
             angle_squared -
         0.5f) *
            angle_squared +
        1.0f};
    cosine = cosine_sign * cosine_polynomial;
}
}
