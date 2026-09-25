#include "ioj/sim/laser_frame_output.h"

namespace ioj::sim::lasers {
void FrameOutput::reset() {
    hits.reset();
    hit_ticks.clear();
    hit_ordinals.clear();
}

} // namespace lasers
