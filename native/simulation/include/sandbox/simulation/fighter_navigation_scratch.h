#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/frame_trace_hits.h"
#include "sandbox/simulation/frame_vectors3f.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation::fighters {
struct NavigationScratch {
    explicit NavigationScratch(std::pmr::memory_resource* resource);

    FrameArray<std::int32_t> ready_fighter_indices;
    FrameVectors3f line_of_sight_starts;
    FrameVectors3f line_of_sight_ends;
    FrameArray<std::uint8_t> line_of_sight_results;
    FrameTraceHits trace_hits;
    FrameArray<std::int32_t> blocked_fighter_indices;
    FrameArray<std::int32_t> trace_fighter_indices;
    FrameArray<std::int8_t> trace_choice_indices;
    FrameArray<std::uint8_t> observed_risk_tiers;
};
}
