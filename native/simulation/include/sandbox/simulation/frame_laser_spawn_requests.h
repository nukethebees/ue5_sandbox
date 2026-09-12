#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/frame_rotators3f.h"
#include "sandbox/simulation/frame_vectors3f.h"
#include "sandbox/simulation/laser_source.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation::lasers {
struct FrameSpawnRequests {
    explicit FrameSpawnRequests(std::pmr::memory_resource* resource);

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
             FRegistryEntityHandle instigator_handle,
             LaserSource source);
    void set_damages(std::int32_t value);
    void set_speeds(float value);
    void set_max_distances(float value);
    [[nodiscard]] auto num() const noexcept -> std::int32_t;

    FrameVectors3f locations;
    FrameRotators3f rotations;
    FrameVectors3f base_velocities;
    FrameArray<std::int32_t> damages;
    FrameArray<float> speeds;
    FrameArray<float> max_distances;
    FrameArray<FRegistryEntityHandle> instigator_handles;
    FrameArray<LaserSource> sources;
};
} // namespace ml::simulation::lasers
