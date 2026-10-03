#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/frame_vectors3f.h"
#include "ioj/sim/trace_hits.h"

#include "sandbox/core/frame_array.h"
#include "sandbox/core/frame_memory_resource.h"

#include <cstdint>

namespace ioj::sim::fighters {
struct NavigationScratch {
    explicit NavigationScratch(ml::FrameScratchResource& scratch_resource)
        : ready_fighter_indices{&scratch_resource}
        , line_of_sight_starts{scratch_resource}
        , line_of_sight_ends{scratch_resource}
        , line_of_sight_results{&scratch_resource}
        , trace_hits{&scratch_resource}
        , blocked_fighter_indices{&scratch_resource}
        , trace_fighter_indices{&scratch_resource}
        , trace_choice_indices{&scratch_resource}
        , observed_risk_tiers{&scratch_resource} {}

    ml::FrameArray<std::uint32_t> ready_fighter_indices;
    FrameVectors3f line_of_sight_starts;
    FrameVectors3f line_of_sight_ends;
    ml::FrameArray<collision::SphereInBoundsResult> line_of_sight_results;
    TraceHits trace_hits;
    ml::FrameArray<std::uint32_t> blocked_fighter_indices;
    ml::FrameArray<std::uint32_t> trace_fighter_indices;
    ml::FrameArray<std::int8_t> trace_choice_indices;
    ml::FrameArray<std::uint8_t> observed_risk_tiers;
};
}
