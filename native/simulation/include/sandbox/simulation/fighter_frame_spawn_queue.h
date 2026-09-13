#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/fighter_spawn_queue.h"
#include "sandbox/simulation/frame_rotators3f.h"
#include "sandbox/simulation/frame_vectors3f.h"

namespace ml::simulation::fighters {
struct FrameSpawnQueue {
    explicit FrameSpawnQueue(std::pmr::memory_resource* resource);
    void reserve(std::int32_t count);
    void clear();
    void add(Vector3f location,
             Rotator3f rotation,
             Team team,
             FRegistryEntityHandle parent,
             FRegistryEntityHandle target);
    auto get_const_view() const -> TestCapitalShipFighterSpawnQueueConstView;

    FrameVectors3f locations;
    FrameRotators3f rotations;
    FrameArray<Team> teams;
    FrameArray<FRegistryEntityHandle> parents;
    FrameArray<FRegistryEntityHandle> targets;
};
}
