#pragma once

#include "ioj/sim/frame_trace_hits.h"
#include "ioj/sim/frame_vectors3f.h"

#include <cstdint>
#include <memory_resource>

namespace ioj::sim::lasers {
struct FrameCollisionScratch {
    explicit FrameCollisionScratch(std::pmr::memory_resource* resource);

    FrameCollisionScratch(FrameCollisionScratch const&) = delete;
    FrameCollisionScratch(FrameCollisionScratch&&) = delete;
    auto operator=(FrameCollisionScratch const&) -> FrameCollisionScratch& = delete;
    auto operator=(FrameCollisionScratch&&) -> FrameCollisionScratch& = delete;
    ~FrameCollisionScratch() = default;

    void set_num(std::int32_t count);

    FrameVectors3f trace_starts;
    FrameVectors3f trace_ends;
    FrameTraceHits trace_hits;
};
} // namespace ioj::sim::lasers
