#pragma once

#include "ioj/sim/entity_overlaps.h"
#include "ioj/sim/index_span.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim::collision {
using AABBOverlapEventBatchIndex = std::int32_t;

struct DetectedOverlapsView {
    EntityEntityOverlaps::ConstView entity_entity_overlaps;
    EntityStaticOverlaps::ConstView entity_static_overlaps;
};

struct AABBOverlapEventBatch {
    IndexSpan entity_entity_overlaps;
    IndexSpan entity_static_overlaps;
};

struct AABBOverlapEventBatchView {
    DetectedOverlapsView overlaps;
};

struct AABBOverlapEventsView {
    EntityEntityOverlaps::ConstView entity_entity_overlaps;
    EntityStaticOverlaps::ConstView entity_static_overlaps;
    std::span<AABBOverlapEventBatch const> batches;

    [[nodiscard]] auto get_batch(AABBOverlapEventBatchIndex index) const
        -> AABBOverlapEventBatchView;
};

class AABBOverlapEventStorage {
  public:
    void reset() noexcept;
    void append_batch(EntityEntityOverlapsConstView entity_entity_overlaps,
                      EntityStaticOverlapsConstView entity_static_overlaps);
    [[nodiscard]] auto get_view() const noexcept -> AABBOverlapEventsView;
  private:
    EntityEntityOverlaps entity_entity_overlaps_;
    EntityStaticOverlaps entity_static_overlaps_;
    std::vector<AABBOverlapEventBatch> batches_;
};
} // namespace ioj::sim::collision
