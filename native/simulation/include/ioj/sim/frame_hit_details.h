#pragma once

#include "ioj/sim/frame_vectors3f.h"
#include "ioj/sim/laser_hit_details.h"
#include "ioj/sim/laser_source.h"
#include "ioj/sim/vector_types.h"

#include "sandbox/core/frame_array.h"
#include "sandbox/core/frame_memory_resource.h"

#include <cassert>
#include <cstdint>

namespace ioj::sim::lasers {
struct FrameHitDetails {
    using soa_schema = LaserHitDetails::soa_schema;

    explicit FrameHitDetails(ml::FrameScratchResource& scratch_resource);

    FrameHitDetails(FrameHitDetails const&) = delete;
    FrameHitDetails(FrameHitDetails&&) = delete;
    auto operator=(FrameHitDetails const&) -> FrameHitDetails& = delete;
    auto operator=(FrameHitDetails&&) -> FrameHitDetails& = delete;
    ~FrameHitDetails() = default;

    void reserve(std::uint32_t count);
    void add(Vector3f location, Vector3f emission_direction, LaserSource source);
    [[nodiscard]] auto num() const noexcept -> std::uint32_t;

    auto view_locations() -> FrameVectors3f& { return locations_; }
    auto view_locations() const -> FrameVectors3f const& { return locations_; }
    auto view_emission_directions() -> FrameVectors3f& { return emission_directions_; }
    auto view_emission_directions() const -> FrameVectors3f const& { return emission_directions_; }
    auto sources() -> std::span<LaserSource> { return sources_.view(); }
    auto sources() const -> std::span<LaserSource const> { return sources_.view(); }
    void validate() const {
        locations_.validate();
        emission_directions_.validate();
        assert(locations_.num() == num());
        assert(emission_directions_.num() == num());
        assert(sources_.num() == num());
    }
  private:
    FrameVectors3f locations_;
    FrameVectors3f emission_directions_;
    ml::FrameArray<LaserSource> sources_;
};
} // namespace ioj::sim::lasers
