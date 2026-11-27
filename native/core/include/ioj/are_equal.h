#pragma once

#include <concepts>

namespace ioj {
template <typename T>
[[nodiscard]] constexpr auto are_equal(T const& expected, std::same_as<T> auto const&... values)
    -> bool {
    return ((expected == values) && ...);
}
}
