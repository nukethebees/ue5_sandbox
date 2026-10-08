#pragma once

#include <algorithm>
#include <initializer_list>

namespace ioj {
template <typename T>
[[nodiscard]] constexpr auto is_any(T const& value, std::initializer_list<T> const candidates)
    -> bool {
    return std::ranges::contains(candidates, value);
}
}
