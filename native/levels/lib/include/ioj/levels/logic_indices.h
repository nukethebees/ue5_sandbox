#pragma once

#include <cstdint>
#include <limits>

namespace ioj::levels {
using ExprIndex = std::uint32_t;
using TimerIndex = std::uint32_t;
using LatchIndex = std::uint32_t;
using SpawnGroupIndex = std::uint32_t;

inline constexpr ExprIndex absent_expression{std::numeric_limits<ExprIndex>::max()};
}
