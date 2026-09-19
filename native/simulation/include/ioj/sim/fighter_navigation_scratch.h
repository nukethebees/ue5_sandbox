#pragma once

#include "ioj/sim/frame_trace_hits.h"
#include "ioj/sim/frame_vectors3f.h"
#include "sandbox/core/frame_array.h"
#include "sandbox/core/frame_memory_resource.h"

#include <cstdint>

namespace ioj::sim::fighters {
struct NavigationScratch {
    explicit NavigationScratch(ml::FrameScratch& scratch)
        : ready_fighter_indices{&scratch}
        , line_of_sight_starts{scratch}
        , line_of_sight_ends{scratch}
        , line_of_sight_results{&scratch}
        , trace_hits{scratch}
        , blocked_fighter_indices{&scratch}
        , trace_fighter_indices{&scratch}
        , trace_choice_indices{&scratch}
        , observed_risk_tiers{&scratch} {}

    ml::FrameArray<std::int32_t> ready_fighter_indices;
    FrameVectors3f line_of_sight_starts;
    FrameVectors3f line_of_sight_ends;
    ml::FrameArray<std::uint8_t> line_of_sight_results;
    FrameTraceHits trace_hits;
    ml::FrameArray<std::int32_t> blocked_fighter_indices;
    ml::FrameArray<std::int32_t> trace_fighter_indices;
    ml::FrameArray<std::int8_t> trace_choice_indices;
    ml::FrameArray<std::uint8_t> observed_risk_tiers;
};
}
