#include "ioj/sim/frame_hit_details.h"

namespace ioj::sim::lasers {
FrameHitDetails::FrameHitDetails(std::pmr::memory_resource* const resource)
    : locations{resource}
    , emission_directions{resource}
    , sources{resource} {}

void FrameHitDetails::reserve(std::int32_t const count) {
    locations.reserve(count);
    emission_directions.reserve(count);
    sources.reserve(count);
}

void FrameHitDetails::add(Vector3f const location,
                          Vector3f const emission_direction,
                          LaserSource const source) {
    locations.add(location);
    emission_directions.add(emission_direction);
    sources.add(source);
}

auto FrameHitDetails::num() const noexcept -> std::int32_t {
    return locations.num();
}
auto FrameHitDetails::get_const_view() const -> LaserHitDetailsConstView {
    return {locations.get_const_view(), emission_directions.get_const_view(), sources.view()};
}
} // namespace ioj::sim::lasers
