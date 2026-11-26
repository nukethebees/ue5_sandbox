#pragma once
#include <cstdint>

namespace ioj::sim {
// Exclusive ends in each accumulated event stream after one fixed tick.
struct FrameEventCounts {
    std::uint32_t deaths;
    std::uint32_t impacts;
    std::uint32_t capital_changes;
    std::uint32_t turret_changes;
};
}
