#pragma once

#include <ioj/vector_value_math.h>

namespace ioj {
template <std::floating_point T>
struct alignas(std::same_as<T, float> ? 16 : alignof(T)) Quaternion {
    Vector<T, 4> components{{T{}, T{}, T{}, T{1}}};

    auto operator<=>(Quaternion const&) const = default;
    constexpr auto x() noexcept -> T& { return components.x(); }
    constexpr auto x() const noexcept -> T const& { return components.x(); }
    constexpr auto y() noexcept -> T& { return components.y(); }
    constexpr auto y() const noexcept -> T const& { return components.y(); }
    constexpr auto z() noexcept -> T& { return components.z(); }
    constexpr auto z() const noexcept -> T const& { return components.z(); }
    constexpr auto w() noexcept -> T& { return components.w(); }
    constexpr auto w() const noexcept -> T const& { return components.w(); }
    constexpr auto operator[](std::size_t const index) noexcept -> T& { return components[index]; }
    constexpr auto operator[](std::size_t const index) const noexcept -> T const& {
        return components[index];
    }
};

template <std::floating_point T>
[[nodiscard]] constexpr auto operator*(Quaternion<T> const lhs, Quaternion<T> const rhs) noexcept
    -> Quaternion<T> {
    return {lhs.w() * rhs.x() + lhs.x() * rhs.w() + lhs.y() * rhs.z() - lhs.z() * rhs.y(),
            lhs.w() * rhs.y() - lhs.x() * rhs.z() + lhs.y() * rhs.w() + lhs.z() * rhs.x(),
            lhs.w() * rhs.z() + lhs.x() * rhs.y() - lhs.y() * rhs.x() + lhs.z() * rhs.w(),
            lhs.w() * rhs.w() - lhs.x() * rhs.x() - lhs.y() * rhs.y() - lhs.z() * rhs.z()};
}
template <std::floating_point T>
[[nodiscard]] constexpr auto operator*(Quaternion<T> value,
                                       std::type_identity_t<T> const scale) noexcept
    -> Quaternion<T> {
    value.components *= scale;
    return value;
}
template <std::floating_point T>
[[nodiscard]] constexpr auto dot(Quaternion<T> const a, Quaternion<T> const b) noexcept -> T {
    // Preserve the float packing path's pairwise sum.
    if constexpr (std::same_as<T, float>) {
        return (a.x() * b.x() + a.y() * b.y()) + (a.z() * b.z() + a.w() * b.w());
    }
    return a.x() * b.x() + a.y() * b.y() + a.z() * b.z() + a.w() * b.w();
}
template <std::floating_point T>
[[nodiscard]] auto normalized(Quaternion<T> const value) noexcept -> Quaternion<T> {
    auto const squared{dot(value, value)};
    assert(squared > T{});
    return value * (T{1} / std::sqrt(squared));
}
template <std::floating_point T>
void normalize(Quaternion<T>& value) noexcept {
    auto const squared{dot(value, value)};
    if (squared < T{1.e-8}) {
        value = {};
        return;
    }
    value = value * (T{1} / std::sqrt(squared));
}
// Require a unit quaternion for rotation and inverse rotation.
template <std::floating_point T>
[[nodiscard]] constexpr auto rotate_vector(Quaternion<T> const orientation,
                                           Vector<T, 3> const value) noexcept -> Vector<T, 3> {
    Vector<T, 3> const axis{orientation.x(), orientation.y(), orientation.z()};
    auto const twice_cross{cross(axis, value) * T{2}};
    return value + twice_cross * orientation.w() + cross(axis, twice_cross);
}
template <std::floating_point T>
[[nodiscard]] constexpr auto unrotate_vector(Quaternion<T> const orientation,
                                             Vector<T, 3> const value) noexcept -> Vector<T, 3> {
    return rotate_vector(
        Quaternion<T>{-orientation.x(), -orientation.y(), -orientation.z(), orientation.w()},
        value);
}
}
