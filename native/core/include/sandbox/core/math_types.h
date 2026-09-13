#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#endif

#include <HandmadeMath.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <type_traits>

namespace ml {
using Vector2f = HMM_Vec2;
using Vector3f = HMM_Vec3;
using Vector4f = HMM_Vec4;
using Matrix2f = HMM_Mat2;
using Matrix3f = HMM_Mat3;
using Matrix4f = HMM_Mat4;
using Quaternion4f = HMM_Quat;

static_assert(sizeof(Vector3f) == sizeof(float) * 3);
static_assert(alignof(Vector3f) == alignof(float));
static_assert(std::is_trivially_copyable_v<Vector3f>);
static_assert(std::is_standard_layout_v<Vector3f>);

[[nodiscard]] constexpr auto make_vector3f(float const x, float const y, float const z) noexcept
    -> Vector3f {
    return Vector3f{{x, y, z}};
}

[[nodiscard]] inline auto
    make_quaternion4f(float const x, float const y, float const z, float const w) noexcept
    -> Quaternion4f {
    return HMM_Q(x, y, z, w);
}
}
