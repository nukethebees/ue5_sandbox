#pragma once

#include <ioj/sim/levels/level_event_count.h>

#include <cstdint>
#include <limits>

namespace ioj::sim {
inline constexpr auto max_level_event_count{
    std::numeric_limits<LevelEventCount::value_type>::max()};

}
