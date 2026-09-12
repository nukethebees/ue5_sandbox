#include "sandbox/simulation/frame_collision_scratch.h"

namespace ml::simulation::lasers {
FrameCollisionScratch::FrameCollisionScratch(std::pmr::memory_resource* const resource)
    : trace_starts{resource}
    , trace_ends{resource}
    , trace_hits{resource} {}

void FrameCollisionScratch::set_num(std::int32_t const count) {
    trace_starts.set_num(count);
    trace_ends.set_num(count);
    trace_hits.set_num(count);
}
} // namespace ml::simulation::lasers
