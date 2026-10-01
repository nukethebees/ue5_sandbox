#pragma once

#include "ioj/sim/line_traces.h"
#include "ioj/sim/trace_hits.h"

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace ioj::sim {
struct QueryThreadBuffers {
    explicit QueryThreadBuffers(
        std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : range_query_entity_stamps{resource} {}

    void ensure_entity_stamp_count(std::uint32_t entity_count);
    [[nodiscard]] auto advance_range_query_stamp() noexcept -> std::uint32_t;

    LineTraces line_traces;
    TraceHits trace_hits;
    std::pmr::vector<std::uint32_t> range_query_entity_stamps;
    std::uint32_t range_query_stamp{};
};
} // namespace ioj::sim
