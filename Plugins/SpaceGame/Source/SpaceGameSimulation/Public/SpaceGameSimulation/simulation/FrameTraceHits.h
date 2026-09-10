#pragma once

#include <SpaceGameSimulation/simulation/TraceHits.h>

#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_vectors.h>

#include <memory_resource>

namespace ml {
struct FFrameTraceHits {
    explicit FFrameTraceHits(std::pmr::memory_resource* const resource)
        : locations{resource}
        , entities{resource}
        , static_geometry_indices{resource}
        , hits{resource} {}

    FFrameTraceHits(FFrameTraceHits const&) = delete;
    FFrameTraceHits(FFrameTraceHits&&) = delete;
    auto operator=(FFrameTraceHits const&) -> FFrameTraceHits& = delete;
    auto operator=(FFrameTraceHits&&) -> FFrameTraceHits& = delete;
    ~FFrameTraceHits() = default;

    void set_num(int32 const count) {
        locations.set_num(count);
        entities.set_num(count);
        static_geometry_indices.set_num(count);
        hits.set_num(count);
    }
    void clear() {
        locations.clear();
        entities.clear();
        static_geometry_indices.clear();
        hits.clear();
    }
    auto get_view() -> FTraceHitsView {
        return {locations.get_view(), entities, static_geometry_indices, hits};
    }
    auto get_const_view() const -> FTraceHitsConstView {
        return {locations.get_const_view(), entities, static_geometry_indices, hits};
    }
    auto num() const -> int32 { return locations.num(); }

    FFrameVectors3f locations;
    TFrameArray<FRegistryEntityHandle> entities;
    TFrameArray<int32> static_geometry_indices;
    TFrameArray<uint8> hits;
};
} // namespace ml
