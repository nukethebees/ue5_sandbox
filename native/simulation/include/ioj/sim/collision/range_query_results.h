#pragma once

#include <ioj/frame_array.h>
#include <ioj/sim/collision/range_query_matches.h>
#include <ioj/sim/index_span.h>

#include <memory_resource>

namespace ioj::sim {
// Populate once per instance. Keep the result storage within its resource lifetime.
// Each request owns a contiguous range of matches.
struct RangeQueryResults {
    explicit RangeQueryResults(std::pmr::memory_resource& resource)
        : matches{resource}
        , ranges{&resource} {}

    RangeQueryMatches matches;
    FrameArray<IndexSpan> ranges;
  private:
    friend struct SpatialQueryManager;
    bool query_started_{};
};
} // namespace ioj::sim
