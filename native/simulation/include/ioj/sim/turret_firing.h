#pragma once

#include "ioj/sim/entity_registry_query.h"
#include "ioj/sim/frame_laser_spawn_requests.h"
#include "sandbox/core/tick_countdown.h"

namespace ioj::sim::turrets {
struct FiringView {
    Vectors3fConstView locations;
    Vectors3fConstView fire_point_locations;
    Vectors3fConstView target_locations;
    Vectors3fConstView target_velocities;
    std::span<RegistryEntityHandle const> handles;
    std::span<RegistryEntityHandle> targets;
    std::span<std::int32_t const> laser_damages;
    std::span<std::byte const> teams;
    ml::TickCountdownView<std::int16_t> cooldowns;
};

struct FiringScratch {
    explicit FiringScratch(std::pmr::memory_resource* resource);
    ml::FrameArray<std::int32_t> candidate_indices;
    ml::FrameArray<RegistryEntityHandle> hit_handles;
    FrameVectors3f starts;
    FrameVectors3f ends;
};

void prepare_firing(FiringView turrets,
                    EntityRegistryQueryView registry,
                    float disengage_radius_squared,
                    FiringScratch& scratch);
void emit_lasers(FiringView turrets,
                 FiringScratch const& scratch,
                 float speed,
                 float maximum_distance,
                 float normal_tolerance,
                 lasers::FrameSpawnRequests& requests);
}
