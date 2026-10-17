#pragma once
#include <cstdint>
#include <limits>

namespace ioj::sim {
using LevelEntityIndex = std::uint32_t;

inline constexpr LevelEntityIndex invalid_level_entity_index{
    std::numeric_limits<LevelEntityIndex>::max()};
}
