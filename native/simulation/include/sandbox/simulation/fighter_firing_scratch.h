#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/frame_laser_spawn_requests.h"
#include "sandbox/simulation/frame_vectors3f.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation::fighters {
struct FiringScratch {
    explicit FiringScratch(std::pmr::memory_resource* resource);

    lasers::FrameSpawnRequests new_lasers;
    FrameArray<float> aiming_dot_products;
    FrameArray<std::int32_t> can_fire;
    FrameVectors3f line_of_sight_starts;
    FrameVectors3f line_of_sight_ends;
    FrameArray<std::uint8_t> line_of_sight_results;
    FrameArray<FRegistryEntityHandle> ignored_entities;
    FrameArray<std::int32_t> position_fighter_indices;
    FrameVectors3f position_candidates;
};
}
