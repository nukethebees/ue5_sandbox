#include "ioj/sim/spatial_query_telemetry.h"

namespace ioj::sim::collision {
void CollisionGridTelemetry::record_rebuild() noexcept {
    rebuild_count_.fetch_add(1, std::memory_order_relaxed);
}

void CollisionGridTelemetry::record_line_traces(std::uint64_t const count) const noexcept {
    line_trace_count_.fetch_add(count, std::memory_order_relaxed);
}

void CollisionGridTelemetry::record_sweep_traces(std::uint64_t const count) const noexcept {
    sweep_trace_count_.fetch_add(count, std::memory_order_relaxed);
}

void CollisionGridTelemetry::reset() noexcept {
    rebuild_count_.store(0, std::memory_order_relaxed);
    line_trace_count_.store(0, std::memory_order_relaxed);
    sweep_trace_count_.store(0, std::memory_order_relaxed);
}

auto CollisionGridTelemetry::snapshot() const noexcept -> CollisionGridTelemetrySnapshot {
    return {
        .rebuild_count = rebuild_count_.load(std::memory_order_relaxed),
        .line_trace_count = line_trace_count_.load(std::memory_order_relaxed),
        .sweep_trace_count = sweep_trace_count_.load(std::memory_order_relaxed),
    };
}

void SpatialQueryTelemetry::record_range_query() const noexcept {
    range_query_count_.fetch_add(1, std::memory_order_relaxed);
}

void SpatialQueryTelemetry::reset() noexcept {
    range_query_count_.store(0, std::memory_order_relaxed);
}

auto SpatialQueryTelemetry::snapshot(CollisionGridTelemetrySnapshot const grid,
                                     std::int32_t const occupied_dynamic_cell_count) const noexcept
    -> SpatialQueryTelemetrySnapshot {
    return {
        .grid_rebuild_count = grid.rebuild_count,
        .range_query_count = range_query_count_.load(std::memory_order_relaxed),
        .line_trace_count = grid.line_trace_count,
        .sweep_trace_count = grid.sweep_trace_count,
        .occupied_dynamic_cell_count = occupied_dynamic_cell_count,
    };
}
} // namespace collision
