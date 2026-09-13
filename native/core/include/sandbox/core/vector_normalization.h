#pragma once

#include "sandbox/core/math_types.h"

#include <cmath>

namespace ml::native_math {
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
