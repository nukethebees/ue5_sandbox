#include "ioj/sim/frame_collision_scratch.h"

namespace ioj::sim::lasers {
FrameCollisionScratch::FrameCollisionScratch(ml::FrameScratchResource& scratch_resource)
    : trace_starts{scratch_resource}
    , trace_ends{scratch_resource}
    , trace_hits{&scratch_resource} {}

void FrameCollisionScratch::set_num(std::uint32_t const count) {
    trace_starts.set_num(count);
    trace_ends.set_num(count);
    trace_hits.set_num(count);
}
} // namespace lasers
