#pragma once

#include "sandbox/core/tick_countdown.h"
#include "sandbox/simulation/entity_registry_query.h"
#include "sandbox/simulation/frame_laser_spawn_requests.h"

namespace ml::simulation::turrets {
struct FiringView {
    Vectors3fConstView locations;
    Vectors3fConstView fire_point_locations;
    Vectors3fConstView target_locations;
    Vectors3fConstView target_velocities;
    std::span<FRegistryEntityHandle const> handles;
    std::span<FRegistryEntityHandle> targets;
    std::span<std::int32_t const> laser_damages;
    std::span<std::byte const> teams;
    TickCountdownView<std::int16_t> cooldowns;
};

struct FiringScratch {
    explicit FiringScratch(std::pmr::memory_resource* resource);
    FrameArray<std::int32_t> candidate_indices;
    FrameArray<FRegistryEntityHandle> hit_handles;
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
