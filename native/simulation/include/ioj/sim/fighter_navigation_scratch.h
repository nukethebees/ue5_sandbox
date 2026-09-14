#pragma once

#include "ioj/sim/frame_trace_hits.h"
#include "ioj/sim/frame_vectors3f.h"
#include "sandbox/core/frame_array.h"

#include <cstdint>
#include <memory_resource>

namespace ioj::sim::fighters {
struct NavigationScratch {
    explicit NavigationScratch(std::pmr::memory_resource* resource)
        : ready_fighter_indices{resource}
        , line_of_sight_starts{resource}
        , line_of_sight_ends{resource}
        , line_of_sight_results{resource}
        , trace_hits{resource}
        , blocked_fighter_indices{resource}
        , trace_fighter_indices{resource}
        , trace_choice_indices{resource}
        , observed_risk_tiers{resource} {}

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
