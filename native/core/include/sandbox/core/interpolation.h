#pragma once

#include <cstdint>

namespace ml::native_kernel {
template <typename T, typename Alpha>
void
    lerp(T* out, T const* from, T const* to, Alpha const alpha, std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        auto const amount{[&] {
            if constexpr (requires { alpha[i]; }) {
                return alpha[i];
            } else {
                return alpha;
            }
        }()};
        out[i] = from[i] + amount * (to[i] - from[i]);
    }
}

template <typename T, typename Alpha>
void lerp_in_place(T* current,
                   T const* target,
                   Alpha const alpha,
                   std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        auto const amount{[&] {
            if constexpr (requires { alpha[i]; }) {
                return alpha[i];
            } else {
                return alpha;
            }
        }()};
        current[i] += amount * (target[i] - current[i]);
    }
}
}
