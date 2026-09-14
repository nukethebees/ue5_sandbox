#pragma once

#include "ioj/sim/line_traces.h"
#include "ioj/sim/trace_hits.h"

#include <cstdint>
#include <vector>

namespace ioj::sim {
struct QueryThreadBuffers {
    void ensure_entity_stamp_count(std::int32_t entity_count);
    [[nodiscard]] auto advance_range_query_stamp() noexcept -> std::uint32_t;

    LineTraces line_traces;
    TraceHits trace_hits;
    std::vector<std::uint32_t> range_query_entity_stamps;
    std::uint32_t range_query_stamp{};
};
} // namespace ioj::sim
