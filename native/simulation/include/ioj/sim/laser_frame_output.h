#pragma once

#include "ioj/sim/laser_hit_details.h"
#include "ioj/sim/sim_tick.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::lasers {
struct FrameOutput {
    void reset();
    template <typename Source>
        requires SingleAllocationLaserHitDetails::accepts_source<Source>
    void append_hits(Source const& new_hits, SimTick const tick) {
        auto const count{new_hits.num()};
        hits.append_from(new_hits);
        for (std::int32_t index{}; index < count; ++index) {
            hit_ticks.push_back(tick);
            hit_ordinals.push_back(index);
        }
    }

    SingleAllocationLaserHitDetails hits;
    std::vector<SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};
} // namespace ioj::sim::lasers
