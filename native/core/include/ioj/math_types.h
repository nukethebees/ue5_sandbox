#pragma once

#include <ioj/quaternion.h>
#include <ioj/vector_value_math.h>

#include <type_traits>

namespace ml {
using Vector2f = ioj::Vector<float, 2>;
using Vector3f = ioj::Vector<float, 3>;
using Quaternion4f = ioj::Quaternion<float>;

static_assert(sizeof(Vector3f) == sizeof(float) * 3);
static_assert(alignof(Vector3f) == alignof(float));
static_assert(std::is_trivially_copyable_v<Vector3f>);
static_assert(std::is_standard_layout_v<Vector3f>);
static_assert(sizeof(Quaternion4f) == 16 && alignof(Quaternion4f) == 16);

[[nodiscard]] constexpr auto make_vector3f(float const x, float const y, float const z) noexcept
    -> Vector3f {
    return {x, y, z};
}
[[nodiscard]] constexpr auto
    make_quaternion4f(float const x, float const y, float const z, float const w) noexcept
    -> Quaternion4f {
    return {x, y, z, w};
}
}
