#pragma once

#include "ioj/sim/fighter_spawn_queue.h"
#include "ioj/sim/frame_rotators3f.h"
#include "ioj/sim/frame_vectors3f.h"

#include "sandbox/core/frame_array.h"
#include "sandbox/core/frame_memory_resource.h"

namespace ioj::sim::fighters {
struct FrameSpawnQueue {
    explicit FrameSpawnQueue(ml::FrameScratch& scratch);
    void reserve(std::int32_t count);
    void clear();
    void add(Vector3f location,
             Rotator3f rotation,
             Team team,
             EntityUniqueId parent,
             EntityUniqueId target);

    auto view_locations() -> FrameVectors3f& { return locations_; }
    auto view_locations() const -> FrameVectors3f const& { return locations_; }
    auto view_rotations() -> FrameRotators3f& { return rotations_; }
    auto view_rotations() const -> FrameRotators3f const& { return rotations_; }
    auto teams() -> std::span<Team> { return teams_.view(); }
    auto teams() const -> std::span<Team const> { return teams_.view(); }
    auto parents() -> std::span<EntityUniqueId> { return parents_.view(); }
    auto parents() const -> std::span<EntityUniqueId const> { return parents_.view(); }
    auto targets() -> std::span<EntityUniqueId> { return targets_.view(); }
    auto targets() const -> std::span<EntityUniqueId const> { return targets_.view(); }
    auto num() const -> std::int32_t { return locations_.num(); }
    void validate() const {
        locations_.validate();
        rotations_.validate();
        ml::native_soa::require(locations_.num() == num());
        ml::native_soa::require(rotations_.num() == num());
        ml::native_soa::require(teams_.num() == num());
        ml::native_soa::require(parents_.num() == num());
        ml::native_soa::require(targets_.num() == num());
    }
  private:
    FrameVectors3f locations_;
    FrameRotators3f rotations_;
    ml::FrameArray<Team> teams_;
    ml::FrameArray<EntityUniqueId> parents_;
    ml::FrameArray<EntityUniqueId> targets_;
};
}
