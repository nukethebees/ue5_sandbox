#pragma once

#include <ioj/vector.h>
#include <ioj/vector_math.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <type_traits>

namespace ioj {
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator+(Vector<T, N> lhs, Vector<T, N> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] += rhs[i];
    }
    return lhs;
}
template <typename T, std::size_t N>
constexpr auto operator+=(Vector<T, N>& lhs, Vector<T, N> const rhs) noexcept -> Vector<T, N>& {
    lhs = lhs + rhs;
    return lhs;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator+(Vector<T, N> lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] += rhs;
    }
    return lhs;
}
template <typename T, std::size_t N>
constexpr auto operator+=(Vector<T, N>& lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N>& {
    lhs = lhs + rhs;
    return lhs;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator-(Vector<T, N> lhs, Vector<T, N> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] -= rhs[i];
    }
    return lhs;
}
template <typename T, std::size_t N>
constexpr auto operator-=(Vector<T, N>& lhs, Vector<T, N> const rhs) noexcept -> Vector<T, N>& {
    lhs = lhs - rhs;
    return lhs;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator-(Vector<T, N> lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] -= rhs;
    }
    return lhs;
}
template <typename T, std::size_t N>
constexpr auto operator-=(Vector<T, N>& lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N>& {
    lhs = lhs - rhs;
    return lhs;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator*(Vector<T, N> lhs, Vector<T, N> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] *= rhs[i];
    }
    return lhs;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator*(Vector<T, N> lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] *= rhs;
    }
    return lhs;
}
template <typename T, std::size_t N>
constexpr auto operator*=(Vector<T, N>& lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N>& {
    lhs = lhs * rhs;
    return lhs;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator/(Vector<T, N> lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        lhs[i] /= rhs;
    }
    return lhs;
}
template <typename T, std::size_t N>
constexpr auto operator/=(Vector<T, N>& lhs, std::type_identity_t<T> const rhs) noexcept
    -> Vector<T, N>& {
    lhs = lhs / rhs;
    return lhs;
}

template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator-(Vector<T, N> value) noexcept -> Vector<T, N> {
    for (auto& element : value.elements) {
        element = -element;
    }
    return value;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto operator*(std::type_identity_t<T> const scale,
                                       Vector<T, N> const value) noexcept -> Vector<T, N> {
    return value * scale;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto dot(Vector<T, N> const a, Vector<T, N> const b) noexcept -> T {
    auto result{a[0] * b[0]};
    for (std::size_t i{1}; i < N; ++i) {
        result += a[i] * b[i];
    }
    return result;
}
template <typename T>
[[nodiscard]] auto dot(Vector<T, 3> const a, Vector<T, 3> const b) noexcept -> T {
    return ioj::native_math::dot_product(a.x(), a.y(), a.z(), b.x(), b.y(), b.z());
}
template <typename T, std::size_t N>
[[nodiscard]] auto size_squared(Vector<T, N> const value) noexcept -> T {
    return dot(value, value);
}
template <std::floating_point T, std::size_t N>
[[nodiscard]] auto size(Vector<T, N> const value) noexcept -> T {
    return std::sqrt(size_squared(value));
}
template <typename T>
[[nodiscard]] constexpr auto cross(Vector<T, 3> const a, Vector<T, 3> const b) noexcept
    -> Vector<T, 3> {
    return {a.y() * b.z() - a.z() * b.y(),
            a.z() * b.x() - a.x() * b.z(),
            a.x() * b.y() - a.y() * b.x()};
}
template <std::floating_point T, std::size_t N>
[[nodiscard]] auto is_finite(Vector<T, N> const value) noexcept -> bool {
    for (auto const element : value.elements) {
        if (!std::isfinite(element)) {
            return false;
        }
    }
    return true;
}
template <std::floating_point T, std::size_t N>
[[nodiscard]] auto is_positive_finite(Vector<T, N> const value) noexcept -> bool {
    for (auto const element : value.elements) {
        if (!std::isfinite(element) || element <= T{}) {
            return false;
        }
    }
    return true;
}
// Require a nonzero vector; use safe_normal when zero is a valid input.
template <std::floating_point T, std::size_t N>
[[nodiscard]] auto normalized(Vector<T, N> const value) noexcept -> Vector<T, N> {
    auto const squared{size_squared(value)};
    assert(squared > T{});
    return value * (T{1} / std::sqrt(squared));
}
template <std::floating_point T, std::size_t N>
[[nodiscard]] auto safe_normal(Vector<T, N> const value,
                               T const squared_tolerance = T{1.e-8}) noexcept -> Vector<T, N> {
    auto const squared{size_squared(value)};
    if (squared == T{1}) {
        return value;
    }
    if (squared < squared_tolerance) {
        return {};
    }
    return value * (T{1} / std::sqrt(squared));
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto component_min(Vector<T, N> a, Vector<T, N> const b) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        a[i] = std::min(a[i], b[i]);
    }
    return a;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto component_max(Vector<T, N> a, Vector<T, N> const b) noexcept
    -> Vector<T, N> {
    for (std::size_t i{}; i < N; ++i) {
        a[i] = std::max(a[i], b[i]);
    }
    return a;
}
template <typename T, std::size_t N>
[[nodiscard]] constexpr auto clamped(Vector<T, N> const value,
                                     Vector<T, N> const minimum,
                                     Vector<T, N> const maximum) noexcept -> Vector<T, N> {
    return component_min(component_max(value, minimum), maximum);
}
template <std::floating_point T, std::size_t N>
[[nodiscard]] auto move_towards(Vector<T, N> const current,
                                Vector<T, N> const target,
                                std::type_identity_t<T> const max_delta) noexcept -> Vector<T, N> {
    auto const delta{target - current};
    auto const distance{size(delta)};
    if (distance <= max_delta) {
        return target;
    }
    if (distance <= T{} || max_delta <= T{}) {
        return current;
    }
    return current + delta * (max_delta / distance);
}
}
