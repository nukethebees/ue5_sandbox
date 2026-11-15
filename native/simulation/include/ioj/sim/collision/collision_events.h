#pragma once

#include "ioj/sim/collision/entity_overlaps.h"
#include "ioj/sim/index_span.h"

#include <cstdint>
#include <span>

namespace ioj::sim::collision {
using AABBOverlapEventBatchIndex = std::int32_t;

/* **************************************** */
// Event views
/* **************************************** */
struct DetectedOverlapsView {
    EntityEntityOverlaps::CompactConstView entity_entity_overlaps;
    EntityStaticOverlaps::CompactConstView entity_static_overlaps;
};

struct AABBOverlapEventBatch {
    IndexSpan entity_entity_overlaps;
    IndexSpan entity_static_overlaps;
};

struct AABBOverlapEventsView {
    EntityEntityOverlaps::CompactConstView entity_entity_overlaps;
    EntityStaticOverlaps::CompactConstView entity_static_overlaps;
    std::span<AABBOverlapEventBatch const> batches;

    [[nodiscard]] auto get_batch(AABBOverlapEventBatchIndex index) const -> DetectedOverlapsView;
};

} // namespace ioj::sim::collision
