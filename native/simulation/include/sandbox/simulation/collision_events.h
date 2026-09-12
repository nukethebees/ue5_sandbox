#pragma once

#include "sandbox/simulation/entity_overlaps.h"
#include "sandbox/simulation/index_span.h"

#include <cstdint>
#include <span>

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
} // namespace ml::simulation::collision
