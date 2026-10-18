#pragma once

#include "ioj/math_types.h"
#include "ioj/vector_math.h"

#include <cmath>

namespace ioj::native_math {
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

using ioj::safe_normal;
}
