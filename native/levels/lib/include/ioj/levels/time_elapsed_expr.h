#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/levels/mission_types.h>

namespace ioj::levels {
struct TimeElapsedExpr {
    auto operator==(TimeElapsedExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::TimeElapsed};
    NonNegativeFinite<double> seconds{};
};
}
