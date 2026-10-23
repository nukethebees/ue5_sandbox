#include "ioj/sim/collision/frame_hit_details.h"

namespace ioj::sim::lasers {
FrameHitDetails::FrameHitDetails(std::pmr::memory_resource* const scratch_resource)
    : storage_{scratch_resource} {}

void FrameHitDetails::reserve(std::uint32_t const count) {
    storage_.reserve(count);
}

void FrameHitDetails::add(Vector3f const location,
                          Vector3f const emission_direction,
                          LaserSource const source) {
    auto const index{storage_.num()};
    storage_.add_uninitialised(1);
    auto const view{storage_.get_view()};
    view.view_locations().set(index, location);
    view.view_emission_directions().set(index, emission_direction);
    view.sources()[index] = source;
}

auto FrameHitDetails::num() const noexcept -> std::uint32_t {
    return storage_.num();
}
} // namespace lasers
