#include "sandbox/simulation/frame_trace_hits.h"

namespace ml::simulation {
FrameTraceHits::FrameTraceHits(std::pmr::memory_resource* const resource)
    : locations{resource}
    , entities{resource}
    , static_geometry_indices{resource}
    , hits{resource} {}

void FrameTraceHits::set_num(std::int32_t const count) {
    locations.set_num(count);
    entities.set_num(count);
    static_geometry_indices.set_num(count);
    hits.set_num(count);
}
void FrameTraceHits::clear() noexcept {
    locations.clear();
    entities.clear();
    static_geometry_indices.clear();
    hits.clear();
}
auto FrameTraceHits::get_view() noexcept -> TraceHitsView {
    return {locations.get_view(), entities, static_geometry_indices, hits};
}
auto FrameTraceHits::get_const_view() const noexcept -> TraceHitsConstView {
    return {locations.get_const_view(), entities, static_geometry_indices, hits};
}
auto FrameTraceHits::num() const noexcept -> std::int32_t {
    return locations.num();
}
} // namespace ml::simulation
