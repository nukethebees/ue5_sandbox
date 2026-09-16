#pragma once

#include "ioj/sim/fighter_spawn_queue.h"
#include "ioj/sim/frame_rotators3f.h"
#include "ioj/sim/frame_vectors3f.h"
#include "sandbox/core/frame_array.h"

namespace ioj::sim::fighters {
struct FrameSpawnQueue {
    explicit FrameSpawnQueue(std::pmr::memory_resource* resource);
    void reserve(std::int32_t count);
    void clear();
    void add(Vector3f location,
             Rotator3f rotation,
             Team team,
             EntityUniqueId parent,
             EntityUniqueId target);
    auto get_const_view() const -> FighterSpawnQueueConstView;

    FrameVectors3f locations;
    FrameRotators3f rotations;
    ml::FrameArray<Team> teams;
    ml::FrameArray<EntityUniqueId> parents;
    ml::FrameArray<EntityUniqueId> targets;
};
}
