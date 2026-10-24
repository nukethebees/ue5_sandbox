#pragma once

#include <ioj/component_sources.h>

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>

namespace ioj {
template <typename T, std::size_t N>
    requires (N > 0)
struct Vector {
    constexpr Vector() = default;
    constexpr Vector(std::array<T, N> const values)
        : elements{values} {}

    template <typename... Values>
        requires (sizeof...(Values) == N && (std::convertible_to<Values, T> && ...))
    constexpr Vector(Values... values)
        : elements{static_cast<T>(values)...} {}

    template <typename Source>
        requires (N == 1 && CoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x} {}

    template <typename Source>
        requires (N == 2 && CoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x, value.y} {}

    template <typename Source>
        requires (N == 3 && CoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x, value.y, value.z} {}

    template <typename Source>
        requires (N == 4 && CoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x, value.y, value.z, value.w} {}

    template <typename Source>
        requires (N == 1 && CoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x()} {}

    template <typename Source>
        requires (N == 2 && CoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x(), value.y()} {}

    template <typename Source>
        requires (N == 3 && CoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x(), value.y(), value.z()} {}

    template <typename Source>
        requires (N == 4 && CoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.x(), value.y(), value.z(), value.w()} {}

    template <typename Source>
        requires (N == 1 && UppercaseCoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X} {}

    template <typename Source>
        requires (N == 2 && UppercaseCoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X, value.Y} {}

    template <typename Source>
        requires (N == 3 && UppercaseCoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X, value.Y, value.Z} {}

    template <typename Source>
        requires (N == 4 && UppercaseCoordinateMembers<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X, value.Y, value.Z, value.W} {}

    template <typename Source>
        requires (N == 1 && UppercaseCoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X()} {}

    template <typename Source>
        requires (N == 2 && UppercaseCoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X(), value.Y()} {}

    template <typename Source>
        requires (N == 3 && UppercaseCoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X(), value.Y(), value.Z()} {}

    template <typename Source>
        requires (N == 4 && UppercaseCoordinateAccessors<Source, T, N>)
    explicit constexpr Vector(Source const& value)
        : Vector{value.X(), value.Y(), value.Z(), value.W()} {}

    auto operator<=>(Vector const&) const = default;

    // Require an index within the component array.
    constexpr auto operator[](std::size_t const index) noexcept -> T& {
        assert(index < N);
        return elements[index];
    }
    constexpr auto operator[](std::size_t const index) const noexcept -> T const& {
        assert(index < N);
        return elements[index];
    }
    constexpr auto x() noexcept -> T&
        requires (N >= 1)
    {
        return elements[0];
    }
    constexpr auto x() const noexcept -> T const&
        requires (N >= 1)
    {
        return elements[0];
    }
    constexpr auto y() noexcept -> T&
        requires (N >= 2)
    {
        return elements[1];
    }
    constexpr auto y() const noexcept -> T const&
        requires (N >= 2)
    {
        return elements[1];
    }
    constexpr auto z() noexcept -> T&
        requires (N >= 3)
    {
        return elements[2];
    }
    constexpr auto z() const noexcept -> T const&
        requires (N >= 3)
    {
        return elements[2];
    }
    constexpr auto w() noexcept -> T&
        requires (N >= 4)
    {
        return elements[3];
    }
    constexpr auto w() const noexcept -> T const&
        requires (N >= 4)
    {
        return elements[3];
    }

    std::array<T, N> elements{};
};

using Vector2f = Vector<float, 2>;
using Vector3f = Vector<float, 3>;
using Vector2d = Vector<double, 2>;
using Vector3d = Vector<double, 3>;
using Point2f = Vector<float, 2>;
using Point3f = Vector<float, 3>;
}
