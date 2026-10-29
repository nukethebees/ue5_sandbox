#include <ioj/sim/collision/collision_pipeline.h>

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/profiling.h>
#include <ioj/transform3d.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace ioj::sim::collision {

CollisionPipeline::CollisionPipeline(EntityTables const& entities,
                                     CollisionInitData data,
                                     std::pmr::memory_resource* const resource)
    : queries_{entities, resource}
    , overlap_handler_{
          Positive<Health::value_type>::make(data.overlap_response.damage_per_overlap_detection)} {
    queries_.initialise(data.grid_geometry, data.entity_bounds);
    queries_.set_static_collision(std::move(data.static_bounds));
}

void CollisionPipeline::set_static_collision(std::span<WorldAABB const> const bounds) {
    WorldAABBs storage;
    storage.reserve(static_cast<std::uint32_t>(bounds.size()));
    for (auto const& bound : bounds) {
        storage.add(bound.min, bound.max);
    }
    queries_.set_static_collision(std::move(storage));
}

void CollisionPipeline::reset_frame_output() {
    queries_.reset_frame_collision_events();
}

void CollisionPipeline::refresh_spatial_index(LevelReadAccess const& level) {
    queries_.refresh_spatial_index(level);
}

void CollisionPipeline::collect_spawn_candidates(EntityTypeSizes const& previous_issued_counts,
                                                 std::span<EntityId const> const capitals,
                                                 std::span<EntityId const> const fighters,
                                                 std::span<EntityId const> const turrets) {
    overlap_candidates_.clear();
    for (auto const ids : std::array{capitals, fighters, turrets}) {
        for (auto const id : ids) {
            if (id.index() >= previous_issued_counts[id.entity_type()]) {
                overlap_candidates_.push_back(id);
            }
        }
    }
}

void CollisionPipeline::track_player_movement(EntityId const player,
                                              Transform3d const& before,
                                              Transform3d const& after) {
    auto const before_location{Vector3f{before.location}};
    auto const after_location{Vector3f{after.location}};
    auto const before_rotation{Rotator3f{before.rotator()}};
    auto const after_rotation{Rotator3f{after.rotator()}};
    if (before_location != after_location || before_rotation != after_rotation) {
        overlap_candidates_.push_back(player);
    }
}

void CollisionPipeline::detect_and_handle_overlaps(LevelReadAccess const& level,
                                                   std::span<EntityId const> const moved_fighters,
                                                   CombatEvents& combat_events,
                                                   FrameMemoryResource& scratch) {
    SANDBOX_PROFILE_SCOPE("Refresh spatial index and detect overlaps");

    overlap_candidates_.insert(
        overlap_candidates_.end(), moved_fighters.begin(), moved_fighters.end());
    std::ranges::sort(overlap_candidates_);
    auto const duplicates{std::ranges::unique(overlap_candidates_)};
    overlap_candidates_.erase(duplicates.begin(), duplicates.end());

    refresh_spatial_index(level);

    FrameScratchScope scratch_scope{scratch};
    auto const overlaps{queries_.detect_overlaps(overlap_candidates_, &scratch)};
    overlap_handler_.handle(overlaps, combat_events);
}

} // namespace ioj::sim::collision
