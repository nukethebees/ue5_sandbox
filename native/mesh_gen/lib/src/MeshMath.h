#pragma once

#include <mesh_gen/MeshGeneration.h>

#include <algorithm>
#include <cmath>

namespace mesh_gen::math {

inline constexpr float small_number{1.0e-8f};

[[nodiscard]] inline auto operator+(Vec2f const left, Vec2f const right) -> Vec2f {
    return {left.x + right.x, left.y + right.y};
}

[[nodiscard]] inline auto operator-(Vec2f const left, Vec2f const right) -> Vec2f {
    return {left.x - right.x, left.y - right.y};
}

[[nodiscard]] inline auto operator*(Vec2f const value, float const scalar) -> Vec2f {
    return {value.x * scalar, value.y * scalar};
}

[[nodiscard]] inline auto operator+(Vec3f const left, Vec3f const right) -> Vec3f {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline auto operator-(Vec3f const left, Vec3f const right) -> Vec3f {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline auto operator-(Vec3f const value) -> Vec3f {
    return {-value.x, -value.y, -value.z};
}

[[nodiscard]] inline auto operator*(Vec3f const value, float const scalar) -> Vec3f {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline auto component_multiply(Vec3f const left, Vec3f const right) -> Vec3f {
    return {left.x * right.x, left.y * right.y, left.z * right.z};
}

[[nodiscard]] inline auto dot(Vec3f const left, Vec3f const right) -> float {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] inline auto cross(Vec3f const left, Vec3f const right) -> Vec3f {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] inline auto length_squared(Vec3f const value) -> float {
    return dot(value, value);
}

[[nodiscard]] inline auto normalize(Vec3f const value) -> Vec3f {
    auto const square_length{length_squared(value)};
    if (square_length <= small_number) {
        return {};
    }
    return value * (1.0f / std::sqrt(square_length));
}

[[nodiscard]] inline auto normalize(Vec2f const value) -> Vec2f {
    auto const square_length{value.x * value.x + value.y * value.y};
    if (square_length <= small_number) {
        return {};
    }
    return value * (1.0f / std::sqrt(square_length));
}

[[nodiscard]] inline auto minimum_component(Vec3f const value) -> float {
    return std::min(value.x, std::min(value.y, value.z));
}

[[nodiscard]] auto rotate(Vec3f value, Rotatorf rotation) -> Vec3f;

}
