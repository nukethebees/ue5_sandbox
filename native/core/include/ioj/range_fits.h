#pragma once

#include <type_traits>

namespace ml {

template <typename T>
[[nodiscard]] constexpr auto
    range_fits(T const first, T const count, T const minimum, T const maximum) noexcept -> bool {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T, bool>);

    // Accept empty ranges and subtract after checking bounds to avoid overflow.
    return count == 0 || (first >= minimum && first <= maximum && count - 1 <= maximum - first);
}

} // namespace ml
