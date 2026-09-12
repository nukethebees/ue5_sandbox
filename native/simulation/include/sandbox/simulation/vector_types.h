#pragma once

#include <cstddef>

namespace ml::simulation {
struct Vector3f {
    float x{};
    float y{};
    float z{};

    [[nodiscard]] constexpr auto operator[](std::size_t const index) noexcept -> float& {
        if (index == 0) {
            return x;
        }
        if (index == 1) {
            return y;
        }
        return z;
    }
    [[nodiscard]] constexpr auto operator[](std::size_t const index) const noexcept -> float {
        if (index == 0) {
            return x;
        }
        if (index == 1) {
            return y;
        }
        return z;
    }
};
} // namespace ml::simulation
