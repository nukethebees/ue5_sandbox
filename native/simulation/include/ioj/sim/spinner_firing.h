#pragma once

#include "ioj/sim/frame_laser_spawn_requests.h"
#include "sandbox/core/tick_countdown.h"

namespace ioj::sim::spinners {
struct FirePoint {
    Vector3f location;
    Rotator3f rotation;
};

struct FiringView {
    Vectors3fConstView locations;
    std::span<float const> yaws;
    std::span<RegistryEntityHandle const> handles;
    std::span<std::int32_t> next_fire_point_indices;
    ml::TickCountdownView<std::int16_t> cooldowns;
};

struct FiringParameters {
    std::int32_t damage;
    float speed;
    float maximum_distance;
};

void fire_lasers(FiringView spinners,
                 std::span<FirePoint const> fire_points,
                 FiringParameters parameters,
                 std::pmr::memory_resource& resource,
                 lasers::FrameSpawnRequests& requests);
}
