#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/frame_vectors3f.h"
#include "sandbox/simulation/trace_hits.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation {
struct FrameTraceHits {
    explicit FrameTraceHits(std::pmr::memory_resource* resource);

    FrameTraceHits(FrameTraceHits const&) = delete;
    FrameTraceHits(FrameTraceHits&&) = delete;
    auto operator=(FrameTraceHits const&) -> FrameTraceHits& = delete;
    auto operator=(FrameTraceHits&&) -> FrameTraceHits& = delete;
    ~FrameTraceHits() = default;

    void set_num(std::int32_t count);
    void clear() noexcept;
    [[nodiscard]] auto get_view() noexcept -> TraceHitsView;
    [[nodiscard]] auto get_const_view() const noexcept -> TraceHitsConstView;
    [[nodiscard]] auto num() const noexcept -> std::int32_t;

    FrameVectors3f locations;
    ml::FrameArray<FRegistryEntityHandle> entities;
    ml::FrameArray<std::int32_t> static_geometry_indices;
    ml::FrameArray<std::uint8_t> hits;
};
} // namespace ml::simulation
