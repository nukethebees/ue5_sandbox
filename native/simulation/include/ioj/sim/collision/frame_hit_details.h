#pragma once

#include "ioj/math_types.h"
#include "ioj/sim/lasers/laser_hit_details.h"
#include "ioj/sim/lasers/laser_source.h"

#include <cassert>
#include <cstdint>
#include <memory_resource>

namespace ioj::sim::lasers {
struct FrameHitDetails {
    using soa_schema = LaserHitDetails::soa_schema;

    explicit FrameHitDetails(std::pmr::memory_resource* const scratch_resource);

    FrameHitDetails(FrameHitDetails const&) = delete;
    FrameHitDetails(FrameHitDetails&&) = delete;
    auto operator=(FrameHitDetails const&) -> FrameHitDetails& = delete;
    auto operator=(FrameHitDetails&&) -> FrameHitDetails& = delete;
    ~FrameHitDetails() = default;

    void reserve(std::uint32_t count);
    void add(Vector3f location, Vector3f emission_direction, LaserSource source);
    [[nodiscard]] auto num() const noexcept -> std::uint32_t;

    auto view_locations() { return storage_.get_view().view_locations(); }
    auto view_locations() const { return storage_.get_const_view().view_locations(); }
    auto view_emission_directions() { return storage_.get_view().view_emission_directions(); }
    auto view_emission_directions() const {
        return storage_.get_const_view().view_emission_directions();
    }
    auto sources() -> std::span<LaserSource> { return storage_.get_view().sources(); }
    auto sources() const -> std::span<LaserSource const> {
        return storage_.get_const_view().sources();
    }
    void validate() const { storage_.get_const_view().validate(); }
  private:
    LaserHitDetails storage_;
};
} // namespace ioj::sim::lasers
