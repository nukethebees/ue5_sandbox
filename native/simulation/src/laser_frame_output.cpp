#include "sandbox/simulation/laser_frame_output.h"

namespace ml::simulation::lasers {
void FrameOutput::reset() {
    hits.reset();
    hit_ticks.clear();
    hit_ordinals.clear();
}

void FrameOutput::append_hits(LaserHitDetailsConstView const new_hits, std::uint64_t const tick) {
    hits.append_from(new_hits);

    auto const count{new_hits.num()};
    for (std::int32_t index{}; index < count; ++index) {
        hit_ticks.push_back(tick);
        hit_ordinals.push_back(index);
    }
}
} // namespace ml::simulation::lasers
