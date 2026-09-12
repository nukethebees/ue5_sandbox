#pragma once

#include <cstdint>

namespace ml::simulation::collision {
struct CollisionGridTelemetrySnapshot {
    std::uint64_t rebuild_count{};
    std::uint64_t line_trace_count{};
    std::uint64_t sweep_trace_count{};
};

struct SpatialQueryTelemetrySnapshot {
    std::uint64_t grid_rebuild_count{};
    std::uint64_t range_query_count{};
    std::uint64_t line_trace_count{};
    std::uint64_t sweep_trace_count{};
    std::int32_t occupied_dynamic_cell_count{};
};
} // namespace ml::simulation::collision
