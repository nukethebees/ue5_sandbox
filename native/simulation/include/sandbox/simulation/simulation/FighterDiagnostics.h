#pragma once

#include <cstdint>

namespace ml::fighter_diagnostics {
inline auto take_report(bool const enabled,
                        std::int32_t& emitted,
                        std::int32_t const limit) noexcept -> bool {
    if (!enabled) {
        emitted = 0;
        return false;
    }
    if (emitted >= limit) {
        return false;
    }
    ++emitted;
    return true;
}
}
