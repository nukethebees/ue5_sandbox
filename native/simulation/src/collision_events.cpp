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
} // namespace ml::simulation::collision
