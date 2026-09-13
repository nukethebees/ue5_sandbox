#pragma once

#include <atomic>
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

class CollisionGridTelemetry {
  public:
    void record_rebuild() noexcept;
    void record_line_traces(std::uint64_t count) const noexcept;
    void record_sweep_traces(std::uint64_t count) const noexcept;

    void reset() noexcept;
    [[nodiscard]] auto snapshot() const noexcept -> CollisionGridTelemetrySnapshot;
  private:
    std::atomic<std::uint64_t> rebuild_count_{};
    mutable std::atomic<std::uint64_t> line_trace_count_{};
    mutable std::atomic<std::uint64_t> sweep_trace_count_{};
};

class SpatialQueryTelemetry {
  public:
    void record_range_query() const noexcept;
    void reset() noexcept;
    [[nodiscard]] auto snapshot(CollisionGridTelemetrySnapshot grid,
                                std::int32_t occupied_dynamic_cell_count) const noexcept
        -> SpatialQueryTelemetrySnapshot;
  private:
    mutable std::atomic<std::uint64_t> range_query_count_{};
};
} // namespace ml::simulation::collision
