#pragma once

#include "ioj/sim/entities/entity_unique_id.h"
#include "ioj/sim/lasers/laser_source.h"
#include <ioj/sim/lasers/laser_entities.h>
#include <ioj/sim/lasers/laser_spawn_requests.h>

#include <cassert>
#include <cstdint>
#include <memory_resource>

namespace ioj::sim::lasers {
struct FrameSpawnRequests {
    using soa_schema = LaserSpawnRequests::soa_schema;

    explicit FrameSpawnRequests(std::pmr::memory_resource* const scratch_resource);

    FrameSpawnRequests(FrameSpawnRequests const&) = delete;
    FrameSpawnRequests(FrameSpawnRequests&&) = delete;
    auto operator=(FrameSpawnRequests const&) -> FrameSpawnRequests& = delete;
    auto operator=(FrameSpawnRequests&&) -> FrameSpawnRequests& = delete;
    ~FrameSpawnRequests() = default;

    void reserve(std::uint32_t count);
    void set_num(std::uint32_t count);
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
    [[nodiscard]] auto num() const noexcept -> std::uint32_t;

    auto view_locations() { return storage_.get_view().view_locations(); }
    auto view_locations() const { return storage_.get_const_view().view_locations(); }
    auto view_rotations() { return storage_.get_view().view_rotations(); }
    auto view_rotations() const { return storage_.get_const_view().view_rotations(); }
    auto view_base_velocities() { return storage_.get_view().view_base_velocities(); }
    auto view_base_velocities() const { return storage_.get_const_view().view_base_velocities(); }
    auto damages() -> std::span<std::int32_t> { return storage_.get_view().damages(); }
    auto damages() const -> std::span<std::int32_t const> {
        return storage_.get_const_view().damages();
    }
    auto speeds() -> std::span<float> { return storage_.get_view().speeds(); }
    auto speeds() const -> std::span<float const> { return storage_.get_const_view().speeds(); }
    auto max_distances() -> std::span<float> { return storage_.get_view().max_distances(); }
    auto max_distances() const -> std::span<float const> {
        return storage_.get_const_view().max_distances();
    }
    auto instigator_ids() -> std::span<EntityUniqueId> {
        return storage_.get_view().instigator_ids();
    }
    auto instigator_ids() const -> std::span<EntityUniqueId const> {
        return storage_.get_const_view().instigator_ids();
    }
    auto sources() -> std::span<LaserSource> { return storage_.get_view().sources(); }
    auto sources() const -> std::span<LaserSource const> {
        return storage_.get_const_view().sources();
    }
    void validate() const { storage_.get_const_view().validate(); }
  private:
    LaserSpawnRequests storage_;
};
} // namespace ioj::sim::lasers
