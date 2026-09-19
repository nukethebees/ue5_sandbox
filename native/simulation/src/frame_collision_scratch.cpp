#include "ioj/sim/frame_collision_scratch.h"

namespace ioj::sim::lasers {
FrameCollisionScratch::FrameCollisionScratch(ml::FrameScratch& scratch)
    : trace_starts{scratch}
    , trace_ends{scratch}
    , trace_hits{scratch} {}

void FrameCollisionScratch::set_num(std::int32_t const count) {
    trace_starts.set_num(count);
    trace_ends.set_num(count);
    trace_hits.set_num(count);
}
} // namespace lasers
