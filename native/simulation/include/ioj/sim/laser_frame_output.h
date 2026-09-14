#pragma once

#include "ioj/sim/laser_hit_details.h"
#include "ioj/sim/sim_tick.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::lasers {
struct FrameOutput {
    void reset();
    void append_hits(LaserHitDetailsConstView hits, SimTick tick);

    LaserHitDetails hits;
    std::vector<SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};
} // namespace ioj::sim::lasers
