#include "ioj/sim/frame_hit_details.h"

namespace ioj::sim::lasers {
FrameHitDetails::FrameHitDetails(ml::FrameScratch& scratch)
    : locations_{scratch}
    , emission_directions_{scratch}
    , sources_{&scratch} {}

void FrameHitDetails::reserve(std::int32_t const count) {
    locations_.reserve(count);
    emission_directions_.reserve(count);
    sources_.reserve(count);
}

void FrameHitDetails::add(Vector3f const location,
                          Vector3f const emission_direction,
                          LaserSource const source) {
    locations_.add(location);
    emission_directions_.add(emission_direction);
    sources_.add(source);
}

auto FrameHitDetails::num() const noexcept -> std::int32_t {
    return locations_.num();
}
} // namespace lasers
