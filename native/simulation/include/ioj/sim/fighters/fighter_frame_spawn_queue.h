#pragma once

#include "ioj/frame_array.h"
#include "ioj/frame_rotators3f.h"
#include "ioj/frame_vectors3f.h"
#include "ioj/sim/fighters/fighter_spawn_queue.h"

#include <cassert>
#include <memory_resource>

namespace ioj::sim::fighters {
struct FrameSpawnQueue {
    using soa_schema = FighterSpawnQueue::soa_schema;

    explicit FrameSpawnQueue(std::pmr::memory_resource* const scratch_resource);
    void reserve(std::uint32_t count);
    void clear();
    void add(Vector3f location,
             Rotator3f rotation,
             Team team,
             EntityId parent,
             EntityId target);

    auto view_locations() -> FrameVectors3f& { return locations_; }
    auto view_locations() const -> FrameVectors3f const& { return locations_; }
    auto view_rotations() -> FrameRotators3f& { return rotations_; }
    auto view_rotations() const -> FrameRotators3f const& { return rotations_; }
    auto teams() -> std::span<Team> { return teams_.view(); }
    auto teams() const -> std::span<Team const> { return teams_.view(); }
    auto parents() -> std::span<EntityId> { return parents_.view(); }
    auto parents() const -> std::span<EntityId const> { return parents_.view(); }
    auto targets() -> std::span<EntityId> { return targets_.view(); }
    auto targets() const -> std::span<EntityId const> { return targets_.view(); }
    auto num() const -> std::uint32_t { return locations_.num(); }
    void validate() const {
        locations_.validate();
        rotations_.validate();
        assert(locations_.num() == num());
        assert(rotations_.num() == num());
        assert(teams_.num() == num());
        assert(parents_.num() == num());
        assert(targets_.num() == num());
    }
  private:
    FrameVectors3f locations_;
    FrameRotators3f rotations_;
    ioj::FrameArray<Team> teams_;
    ioj::FrameArray<EntityId> parents_;
    ioj::FrameArray<EntityId> targets_;
};
}
