#pragma once

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>

namespace ioj {
template <typename T, std::size_t N>
struct Vector {
    static_assert(N > 0);
    std::array<T, N> elements{};

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
};
}
