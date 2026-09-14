#pragma once

#include "ioj/sim/frame_laser_spawn_requests.h"
#include "ioj/sim/frame_vectors3f.h"
#include "sandbox/core/frame_array.h"

#include <cstdint>
#include <memory_resource>

namespace ioj::sim::fighters {
struct FiringScratch {
    explicit FiringScratch(std::pmr::memory_resource* resource);

    lasers::FrameSpawnRequests new_lasers;
    ml::FrameArray<float> aiming_dot_products;
    ml::FrameArray<std::int32_t> can_fire;
    FrameVectors3f line_of_sight_starts;
    FrameVectors3f line_of_sight_ends;
    ml::FrameArray<std::uint8_t> line_of_sight_results;
    ml::FrameArray<RegistryEntityHandle> ignored_entities;
    ml::FrameArray<std::int32_t> position_fighter_indices;
    FrameVectors3f position_candidates;
};
}
