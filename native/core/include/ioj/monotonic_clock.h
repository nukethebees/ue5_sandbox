#pragma once

#include <chrono>

namespace ioj {
inline auto monotonic_seconds() noexcept -> double {
    return std::chrono::duration<double>{std::chrono::steady_clock::now().time_since_epoch()}
        .count();
}
}
