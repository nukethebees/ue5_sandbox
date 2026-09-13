#pragma once

#include "sandbox/simulation/laser_hit_details.h"

#include <cstdint>
#include <vector>

namespace ml::simulation::lasers {
struct FrameOutput {
    void reset();
    void append_hits(LaserHitDetailsConstView hits, std::uint64_t tick);

    LaserHitDetails hits;
    std::vector<std::uint64_t> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};
} // namespace ml::simulation::lasers
