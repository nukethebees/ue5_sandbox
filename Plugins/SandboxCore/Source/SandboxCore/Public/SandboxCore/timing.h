#pragma once

#include <concepts>

namespace ml {
template <typename... PeriodTypes>
    requires ((std::integral<PeriodTypes> || std::floating_point<PeriodTypes>) && ...)
[[nodiscard]] constexpr auto valid_periods(PeriodTypes const... periods) noexcept -> bool {
    return ((periods > 0) && ...);
}
}
