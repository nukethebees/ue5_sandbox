#pragma once

#include "ioj/frame_array.h"
#include "ioj/frame_vectors3f.h"
#include "ioj/sim/collision/collision_grid.h"
#include "ioj/sim/collision/trace_hits.h"

#include <cstdint>
#include <memory_resource>

namespace ioj::sim::fighters {
struct NavigationScratch {
    explicit NavigationScratch(std::pmr::memory_resource* const scratch_resource)
        : ready_fighter_indices{scratch_resource}
        , line_of_sight_starts{scratch_resource}
        , line_of_sight_ends{scratch_resource}
        , line_of_sight_results{scratch_resource}
        , trace_hits{scratch_resource}
        , blocked_fighter_indices{scratch_resource}
        , trace_fighter_indices{scratch_resource}
        , trace_choice_indices{scratch_resource}
        , observed_risk_tiers{scratch_resource} {}

    FrameArray<std::uint32_t> ready_fighter_indices;
    FrameVectors3f line_of_sight_starts;
    FrameVectors3f line_of_sight_ends;
    FrameArray<collision::SphereInBoundsResult> line_of_sight_results;
    TraceHits trace_hits;
    FrameArray<std::uint32_t> blocked_fighter_indices;
    FrameArray<std::uint32_t> trace_fighter_indices;
    FrameArray<std::int8_t> trace_choice_indices;
    FrameArray<std::uint8_t> observed_risk_tiers;
};
}
