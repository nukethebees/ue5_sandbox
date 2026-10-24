#pragma once

#include <ioj/component_sources.h>

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <utility>

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

    template <IndexedComponents Source>
        requires (Source::NumComponents == N &&
                  std::convertible_to<decltype(std::declval<Source const&>()[0]), T>)
    explicit constexpr Vector(Source const& value)
        : Vector{value,
                 std::make_integer_sequence<std::remove_cv_t<decltype(Source::NumComponents)>,
                                            Source::NumComponents>{}} {}

    template <std::convertible_to<T> U>
    explicit constexpr Vector(Vector<U, N> const& value)
        : Vector{value, std::make_index_sequence<N>{}} {}
  private:
    template <typename Source, typename Index, Index... I>
    constexpr Vector(Source const& value, std::integer_sequence<Index, I...>)
        : Vector{value[I]...} {}
  public:
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

template <IndexedComponents Source>
Vector(Source const&) -> Vector<std::remove_cvref_t<decltype(std::declval<Source const&>()[0])>,
                                static_cast<std::size_t>(Source::NumComponents)>;

using Vector2f = Vector<float, 2>;
using Vector3f = Vector<float, 3>;
using Vector2d = Vector<double, 2>;
using Vector3d = Vector<double, 3>;
using Point2f = Vector<float, 2>;
using Point3f = Vector<float, 3>;
}
