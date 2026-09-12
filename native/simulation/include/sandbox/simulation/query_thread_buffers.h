#pragma once

#include "sandbox/simulation/line_traces.h"
#include "sandbox/simulation/trace_hits.h"

#include <cstdint>
#include <vector>

namespace ml::simulation {
struct QueryThreadBuffers {
    void ensure_entity_stamp_count(std::int32_t entity_count);
    [[nodiscard]] auto advance_range_query_stamp() noexcept -> std::uint32_t;

    LineTraces line_traces;
    TraceHits trace_hits;
    std::vector<std::uint32_t> range_query_entity_stamps;
    std::uint32_t range_query_stamp{};
};
} // namespace ml::simulation
