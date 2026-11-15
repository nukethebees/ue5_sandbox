#include "ioj/sim/collision/collision_events.h"

#include <cstddef>

namespace ioj::sim::collision {
/* **************************************** */
// Event views
/* **************************************** */
auto AABBOverlapEventsView::get_batch(AABBOverlapEventBatchIndex const index) const
    -> DetectedOverlapsView {
    auto const batch{batches[static_cast<std::size_t>(index)]};
    return {
        .entity_entity_overlaps = entity_entity_overlaps.slice(batch.entity_entity_overlaps.offset,
                                                               batch.entity_entity_overlaps.count),
        .entity_static_overlaps = entity_static_overlaps.slice(batch.entity_static_overlaps.offset,
                                                               batch.entity_static_overlaps.count),
    };
}

} // namespace ioj::sim::collision
