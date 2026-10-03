#pragma once

#include <ioj/sim/entity_unique_id.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/index_span.h>

namespace ioj::sim {
// Populate once per instance. Keep the result storage within its scratch epoch.
// Each request owns a contiguous range of matches.
struct FrameRangeQueryResults {
    explicit FrameRangeQueryResults(ml::FrameScratchResource& scratch_resource)
        : entities{&scratch_resource}
        , distances{&scratch_resource}
        , directions{scratch_resource}
        , ranges{&scratch_resource} {}

    void set_num_matches(std::uint32_t const count) {
        entities.set_num(count);
        distances.set_num(count);
        directions.set_num(count);
    }

    ml::FrameArray<EntityUniqueId> entities;
    ml::FrameArray<float> distances;
    FrameVectors3f directions;
    ml::FrameArray<IndexSpan> ranges;
  private:
    friend struct SpatialQueryManager;
    bool query_started_{};
};
} // namespace ioj::sim
