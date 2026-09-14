#pragma once

#include "ioj/sim/entity_overlaps.h"
#include "ioj/sim/index_span.h"
#include "ioj/sim/sim_tick.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim::collision {
struct DetectedOverlapsView {
    ioj::sim::collision::EntityEntityOverlaps::ConstView entity_entity_overlaps;
    ioj::sim::collision::EntityStaticOverlaps::ConstView entity_static_overlaps;
};

struct AABBOverlapEventBatch {
    SimTick tick{};
    IndexSpan entity_entity_overlaps;
    IndexSpan entity_static_overlaps;
};

struct AABBOverlapEventBatchView {
    SimTick tick{};
    DetectedOverlapsView overlaps;
};

struct AABBOverlapEventsView {
    ioj::sim::collision::EntityEntityOverlaps::ConstView entity_entity_overlaps;
    ioj::sim::collision::EntityStaticOverlaps::ConstView entity_static_overlaps;
    std::span<AABBOverlapEventBatch const> batches;

    [[nodiscard]] auto get_batch(std::int32_t index) const -> AABBOverlapEventBatchView;
};

class AABBOverlapEventStorage {
  public:
    void reset() noexcept;
    void append_batch(SimTick tick,
                      ioj::sim::collision::EntityEntityOverlapsConstView entity_entity_overlaps,
                      ioj::sim::collision::EntityStaticOverlapsConstView entity_static_overlaps);
    [[nodiscard]] auto get_view() const noexcept -> AABBOverlapEventsView;
  private:
    ioj::sim::collision::EntityEntityOverlaps entity_entity_overlaps_;
    ioj::sim::collision::EntityStaticOverlaps entity_static_overlaps_;
    std::vector<AABBOverlapEventBatch> batches_;
};
} // namespace ioj::sim::collision
