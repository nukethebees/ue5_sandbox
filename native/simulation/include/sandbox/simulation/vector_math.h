#pragma once

#include "sandbox/simulation/vector_types.h"

namespace ml::simulation {
[[nodiscard]] constexpr auto operator+(Vector3f const left, Vector3f const right) noexcept
    -> Vector3f {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] constexpr auto operator-(Vector3f const left, Vector3f const right) noexcept
    -> Vector3f {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] constexpr auto operator*(Vector3f const value, float const scalar) noexcept
    -> Vector3f {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}
} // namespace ml::simulation
