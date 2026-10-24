#pragma once

#include <cstdint>
#include <limits>

namespace ioj::levels {
using ExpressionIndex = std::uint32_t;
inline constexpr ExpressionIndex absent_expression{std::numeric_limits<ExpressionIndex>::max()};
}
