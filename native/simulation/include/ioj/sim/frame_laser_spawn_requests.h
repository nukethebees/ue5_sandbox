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
    [[nodiscard]] auto get_const_view() const -> SpawnRequestsConstView;

    FrameVectors3f locations;
    FrameRotators3f rotations;
    FrameVectors3f base_velocities;
    ml::FrameArray<std::int32_t> damages;
    ml::FrameArray<float> speeds;
    ml::FrameArray<float> max_distances;
    ml::FrameArray<EntityUniqueId> instigator_ids;
    ml::FrameArray<LaserSource> sources;
};
} // namespace ioj::sim::lasers
