#pragma once

#include "sandbox/core/math_types.h"
#include "sandbox/core/vector_math.h"

#include <cmath>

namespace ml::native_math {
template <std::floating_point T>
void safe_normal_components(T& x, T& y, T& z, T const squared_tolerance = T{1.e-8}) noexcept {
    auto const length_squared{size_squared(x, y, z)};
    if (length_squared == T{1}) {
        return;
    }
    if (length_squared < squared_tolerance) {
        x = y = z = T{};
        return;
    }
    auto const scale{T{1} / std::sqrt(length_squared)};
    x *= scale;
    y *= scale;
    z *= scale;
}

[[nodiscard]] inline auto safe_normal(Vector3f const vector,
                                      float const squared_tolerance = 1.e-8f) noexcept -> Vector3f {
    auto const length_squared{HMM_DotV3(vector, vector)};
    if (length_squared == 1.0f) {
        return vector;
    }
    if (length_squared < squared_tolerance) {
        return make_vector3f(0.0f, 0.0f, 0.0f);
    }
    return vector * (1.0f / std::sqrt(length_squared));
}
}
