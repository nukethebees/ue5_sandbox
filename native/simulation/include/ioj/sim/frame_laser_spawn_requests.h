#pragma once

#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/frame_rotators3f.h"
#include "ioj/sim/frame_vectors3f.h"
#include "ioj/sim/laser_soa.h"
#include "ioj/sim/laser_source.h"
#include "sandbox/core/frame_array.h"
#include "sandbox/core/frame_memory_resource.h"

#include <cstdint>

namespace ioj::sim::lasers {
struct FrameSpawnRequests {
    explicit FrameSpawnRequests(ml::FrameScratch& scratch);

    FrameSpawnRequests(FrameSpawnRequests const&) = delete;
    FrameSpawnRequests(FrameSpawnRequests&&) = delete;
    auto operator=(FrameSpawnRequests const&) -> FrameSpawnRequests& = delete;
    auto operator=(FrameSpawnRequests&&) -> FrameSpawnRequests& = delete;
    ~FrameSpawnRequests() = default;

    void reserve(std::int32_t count);
    void set_num(std::int32_t count);
    void add(Vector3f location,
             Rotator3f rotation,
             Vector3f base_velocity,
             std::int32_t damage,
             float speed,
             float max_distance,
             EntityUniqueId instigator_id,
             LaserSource source);
    void set_damages(std::int32_t value);
    void set_speeds(float value);
    void set_max_distances(float value);
    [[nodiscard]] auto num() const noexcept -> std::int32_t;

    auto view_locations() -> FrameVectors3f& { return locations_; }
    auto view_locations() const -> FrameVectors3f const& { return locations_; }
    auto view_rotations() -> FrameRotators3f& { return rotations_; }
    auto view_rotations() const -> FrameRotators3f const& { return rotations_; }
    auto view_base_velocities() -> FrameVectors3f& { return base_velocities_; }
    auto view_base_velocities() const -> FrameVectors3f const& { return base_velocities_; }
    auto damages() -> std::span<std::int32_t> { return damages_.view(); }
    auto damages() const -> std::span<std::int32_t const> { return damages_.view(); }
    auto speeds() -> std::span<float> { return speeds_.view(); }
    auto speeds() const -> std::span<float const> { return speeds_.view(); }
    auto max_distances() -> std::span<float> { return max_distances_.view(); }
    auto max_distances() const -> std::span<float const> { return max_distances_.view(); }
    auto instigator_ids() -> std::span<EntityUniqueId> { return instigator_ids_.view(); }
    auto instigator_ids() const -> std::span<EntityUniqueId const> {
        return instigator_ids_.view();
    }
    auto sources() -> std::span<LaserSource> { return sources_.view(); }
    auto sources() const -> std::span<LaserSource const> { return sources_.view(); }
    void validate() const {
        locations_.validate();
        rotations_.validate();
        base_velocities_.validate();
        ml::native_soa::require(locations_.num() == num());
        ml::native_soa::require(rotations_.num() == num());
        ml::native_soa::require(base_velocities_.num() == num());
        ml::native_soa::require(damages_.num() == num());
        ml::native_soa::require(speeds_.num() == num());
        ml::native_soa::require(max_distances_.num() == num());
        ml::native_soa::require(instigator_ids_.num() == num());
        ml::native_soa::require(sources_.num() == num());
    }
  private:
    FrameVectors3f locations_;
    FrameRotators3f rotations_;
    FrameVectors3f base_velocities_;
    ml::FrameArray<std::int32_t> damages_;
    ml::FrameArray<float> speeds_;
    ml::FrameArray<float> max_distances_;
    ml::FrameArray<EntityUniqueId> instigator_ids_;
    ml::FrameArray<LaserSource> sources_;
};
} // namespace ioj::sim::lasers
