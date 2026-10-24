#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/levels/mission_types.h>

namespace ioj::levels {
struct TimeElapsedExpression {
    static constexpr ExpressionType type{ExpressionType::TimeElapsed};
    NonNegativeFinite<double> seconds{};
    auto operator==(TimeElapsedExpression const&) const -> bool = default;
};
}
