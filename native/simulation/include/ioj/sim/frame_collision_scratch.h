#pragma once

#include "ioj/sim/frame_vectors3f.h"
#include "ioj/sim/trace_hits.h"

#include <cstdint>
namespace ioj::sim::lasers {
struct FrameCollisionScratch {
    explicit FrameCollisionScratch(ml::FrameScratch& scratch);

    FrameCollisionScratch(FrameCollisionScratch const&) = delete;
    FrameCollisionScratch(FrameCollisionScratch&&) = delete;
    auto operator=(FrameCollisionScratch const&) -> FrameCollisionScratch& = delete;
    auto operator=(FrameCollisionScratch&&) -> FrameCollisionScratch& = delete;
    ~FrameCollisionScratch() = default;

    void set_num(std::uint32_t count);

    FrameVectors3f trace_starts;
    FrameVectors3f trace_ends;
    TraceHits trace_hits;
};
} // namespace ioj::sim::lasers
