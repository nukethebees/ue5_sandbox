#include "ioj/sim/collision/collision_system.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>
#include <thread>

#include <ioj/sim/agent_accessor.h>

namespace ioj::sim::collision {
void CollisionSystem::initialise(collision::EntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
    overlap_storage_.reset();
    reset_frame_events();
}
auto CollisionSystem::update(std::span<EntityUniqueId const> const collision_dirty_entities,
                             ml::FrameScratch& scratch) -> DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::update");
    rebuild_grid();
    collect_overlaps_for_moved_entities(collision_dirty_entities, scratch);

    overlap_event_storage_.append_batch(overlap_storage_.entity_entity_overlaps(),
                                        overlap_storage_.entity_static_overlaps());

    return overlap_storage_.get_view();
}
void CollisionSystem::reset_frame_events() {
    overlap_event_storage_.reset();
}
CollisionSystem::CollisionSystem(AgentAccessor const& agents) noexcept
    : agents_{agents}
    , uniform_grid_{agents} {}
void CollisionSystem::rebuild_grid() {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::rebuild_grid");
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
void CollisionSystem::refresh_queries() {
    rebuild_grid();
}
void CollisionSystem::collect_overlaps_for_moved_entities(
    std::span<EntityUniqueId const> const collision_dirty_entities, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("CollisionSystem::collect_overlaps_for_moved_entities");

    overlap_storage_.clear();

    ml::FrameArray<EntityUniqueId> overlapping_entities{&scratch};
    ml::FrameArray<std::int32_t> overlapping_static_geometry_indices{&scratch};

    for (auto const dirty_entity : collision_dirty_entities) {
        auto const id{dirty_entity};
        auto const state{agents_.read_alive(id)};
        if (!state) {
            continue;
        }

        auto const bounds{collision::make_entity_world_bounds(
            entity_aabbs_, id.entity_type(), state->location, to_quaternion(state->rotation))};

        overlapping_entities.clear();
        overlapping_static_geometry_indices.clear();
        uniform_grid_.append_overlaps(
            bounds, id, overlapping_entities, overlapping_static_geometry_indices);

        for (auto const overlapping_entity : overlapping_entities) {
            overlap_storage_.add_entity_overlap(id, overlapping_entity);
        }

        for (auto const static_geometry_index : overlapping_static_geometry_indices) {
            overlap_storage_.add_static_overlap(id, static_geometry_index);
        }
    }

    overlap_storage_.finalize(scratch);
}
}
