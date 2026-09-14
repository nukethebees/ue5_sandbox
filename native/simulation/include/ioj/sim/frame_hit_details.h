#pragma once

#include "ioj/sim/frame_vectors3f.h"
#include "ioj/sim/laser_hit_details.h"
#include "ioj/sim/laser_source.h"
#include "ioj/sim/vector_types.h"
#include "sandbox/core/frame_array.h"

#include <cstdint>
#include <memory_resource>

namespace ioj::sim::lasers {
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
    [[nodiscard]] auto get_const_view() const -> LaserHitDetailsConstView;

    FrameVectors3f locations;
    FrameVectors3f emission_directions;
    ml::FrameArray<LaserSource> sources;
};
} // namespace ioj::sim::lasers
