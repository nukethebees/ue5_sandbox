#pragma once

#include <cmath>

namespace ml {
struct Vector2d {
    double x{};
    double y{};
    auto operator==(Vector2d const&) const -> bool = default;
    auto clamped_to_max_size(double const maximum) const -> Vector2d {
        auto const squared{x * x + y * y};
        if (squared > maximum * maximum) {
            auto const scale{maximum / std::sqrt(squared)};
            return {x * scale, y * scale};
        }
        return *this;
    }
};
}
