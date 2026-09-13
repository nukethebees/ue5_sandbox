#include "sandbox/simulation/collision_events.h"

#include <cstddef>

namespace ml::simulation::collision {
auto AABBOverlapEventsView::get_batch(std::int32_t const index) const -> AABBOverlapEventBatchView {
    auto const batch{batches[static_cast<std::size_t>(index)]};
    return {
        .tick = batch.tick,
        .overlaps =
            {
                .entity_entity_overlaps = entity_entity_overlaps.slice(
                    batch.entity_entity_overlaps.offset, batch.entity_entity_overlaps.count),
                .entity_static_overlaps = entity_static_overlaps.slice(
                    batch.entity_static_overlaps.offset, batch.entity_static_overlaps.count),
            },
    };
}

void AABBOverlapEventStorage::reset() noexcept {
    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();
    batches_.clear();
}

void AABBOverlapEventStorage::append_batch(
    std::uint64_t const tick,
    ioj::FEntityEntityOverlapsConstView const entity_entity_overlaps,
    ioj::FEntityStaticOverlapsConstView const entity_static_overlaps) {
    auto const entity_entity_offset{entity_entity_overlaps_.num()};
    auto const entity_static_offset{entity_static_overlaps_.num()};
    entity_entity_overlaps_.append_from(entity_entity_overlaps);
    entity_static_overlaps_.append_from(entity_static_overlaps);
    batches_.push_back({
        .tick = tick,
        .entity_entity_overlaps =
            {
                .offset = entity_entity_offset,
                .count = entity_entity_overlaps.num(),
            },
        .entity_static_overlaps =
            {
                .offset = entity_static_offset,
                .count = entity_static_overlaps.num(),
            },
    });
}

auto AABBOverlapEventStorage::get_view() const noexcept -> AABBOverlapEventsView {
    return {entity_entity_overlaps_.get_const_view(),
            entity_static_overlaps_.get_const_view(),
            batches_};
}
} // namespace ml::simulation::collision
