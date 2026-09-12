#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/frame_vectors3f.h"
#include "sandbox/simulation/laser_source.h"
#include "sandbox/simulation/vector_types.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation::lasers {
struct FrameHitDetails {
    explicit FrameHitDetails(std::pmr::memory_resource* resource);

    FrameHitDetails(FrameHitDetails const&) = delete;
    FrameHitDetails(FrameHitDetails&&) = delete;
    auto operator=(FrameHitDetails const&) -> FrameHitDetails& = delete;
    auto operator=(FrameHitDetails&&) -> FrameHitDetails& = delete;
    ~FrameHitDetails() = default;

    void reserve(std::int32_t count);
    void add(Vector3f location, Vector3f emission_direction, LaserSource source);
    [[nodiscard]] auto num() const noexcept -> std::int32_t;

    FrameVectors3f locations;
    FrameVectors3f emission_directions;
    FrameArray<LaserSource> sources;
};
} // namespace ml::simulation::lasers
