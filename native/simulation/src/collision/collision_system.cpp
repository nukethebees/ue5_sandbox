#include "ioj/sim/collision/collision_system.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>
#include <sandbox/core/diagnostics.h>
#include <thread>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/entity_registry.h>

namespace ioj::sim::collision {
void CollisionSystem::initialise(collision::EntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
    overlap_storage_.reset();
    reset_frame_events();
    overlapping_entities_scratch_.clear();
    overlapping_static_geometry_indices_scratch_.clear();
}
auto CollisionSystem::update(std::span<RegistryEntityHandle const> const collision_dirty_entities,
                             SimTick const tick) -> DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionSystem::update");
    rebuild_grid();
    collect_overlaps_for_moved_entities(collision_dirty_entities);

    overlap_event_storage_.append_batch(
        tick, overlap_storage_.entity_entity_overlaps(), overlap_storage_.entity_static_overlaps());

    return overlap_storage_.get_view();
}
void CollisionSystem::reset_frame_events() {
    overlap_event_storage_.reset();
}
CollisionSystem::CollisionSystem(EntityRegistry const& registry,
                                 AgentAccessor const& agents) noexcept
    : entity_registry_{registry}
    , agents_{agents}
    , uniform_grid_{registry, agents} {}
void CollisionSystem::rebuild_grid() {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionSystem::rebuild_grid");
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
void CollisionSystem::refresh_queries() {
    rebuild_grid();
}
void CollisionSystem::collect_overlaps_for_moved_entities(
    std::span<RegistryEntityHandle const> const collision_dirty_entities) {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionSystem::collect_overlaps_for_moved_entities");

    overlap_storage_.clear();

    for (auto const dirty_entity : collision_dirty_entities) {
        auto const id{entity_registry_.get_current_id(dirty_entity)};
        auto const state{agents_.read_alive(id)};
        if (!state) {
            continue;
        }

        auto const bounds{collision::make_entity_world_bounds(entity_aabbs_,
                                                              std::to_underlying(id.entity_type()),
                                                              state->location,
                                                              to_quaternion(state->rotation))};

        overlapping_entities_scratch_.clear();
        overlapping_static_geometry_indices_scratch_.clear();
        uniform_grid_.append_overlaps(bounds,
                                      id,
                                      overlapping_entities_scratch_,
                                      overlapping_static_geometry_indices_scratch_);

        for (auto const overlapping_entity : overlapping_entities_scratch_) {
            overlap_storage_.add_entity_overlap(id, overlapping_entity);
        }

        for (auto const static_geometry_index : overlapping_static_geometry_indices_scratch_) {
            overlap_storage_.add_static_overlap(id, static_geometry_index);
        }
    }

    overlap_storage_.finalize();
}
}
