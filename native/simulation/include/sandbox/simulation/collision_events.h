#pragma once

#include "sandbox/simulation/entity_overlaps.h"
#include "sandbox/simulation/index_span.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ml::simulation::collision {
struct DetectedOverlapsView {
    ioj::FEntityEntityOverlaps::ConstView entity_entity_overlaps;
    ioj::FEntityStaticOverlaps::ConstView entity_static_overlaps;
};

struct AABBOverlapEventBatch {
    std::uint64_t tick{};
    FIndexSpan entity_entity_overlaps;
    FIndexSpan entity_static_overlaps;
};

struct AABBOverlapEventBatchView {
    std::uint64_t tick{};
    DetectedOverlapsView overlaps;
};

struct AABBOverlapEventsView {
    ioj::FEntityEntityOverlaps::ConstView entity_entity_overlaps;
    ioj::FEntityStaticOverlaps::ConstView entity_static_overlaps;
    std::span<AABBOverlapEventBatch const> batches;

    [[nodiscard]] auto get_batch(std::int32_t index) const -> AABBOverlapEventBatchView;
};

class AABBOverlapEventStorage {
  public:
    void reset() noexcept;
    void append_batch(std::uint64_t tick,
                      ioj::FEntityEntityOverlapsConstView entity_entity_overlaps,
                      ioj::FEntityStaticOverlapsConstView entity_static_overlaps);
    [[nodiscard]] auto get_view() const noexcept -> AABBOverlapEventsView;
  private:
    ioj::FEntityEntityOverlaps entity_entity_overlaps_;
    ioj::FEntityStaticOverlaps entity_static_overlaps_;
    std::vector<AABBOverlapEventBatch> batches_;
};
} // namespace ml::simulation::collision
