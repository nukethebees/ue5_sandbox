#pragma once

#include "sandbox/simulation/laser_hit_details.h"
#include "sandbox/simulation/sim_tick.h"

#include <cstdint>
#include <vector>

namespace ml::simulation::lasers {
struct FrameOutput {
    void reset();
    void append_hits(LaserHitDetailsConstView hits, SimTick tick);

    LaserHitDetails hits;
    std::vector<SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};
} // namespace ml::simulation::lasers
