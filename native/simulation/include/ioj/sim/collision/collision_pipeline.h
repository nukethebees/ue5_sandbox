#pragma once

#include <ioj/sim/collision/collision_init_data.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/overlap_handler.h>

#include <memory_resource>
#include <span>
#include <vector>

namespace ioj {
struct Transform3d;
}

namespace ioj::sim::collision {

class CollisionPipeline {
  public:
    CollisionPipeline(EntityTables const& entities,
                      CollisionInitData data,
                      std::pmr::memory_resource* resource);

    CollisionPipeline(CollisionPipeline const&) = delete;
    CollisionPipeline(CollisionPipeline&&) = delete;
    auto operator=(CollisionPipeline const&) -> CollisionPipeline& = delete;
    auto operator=(CollisionPipeline&&) -> CollisionPipeline& = delete;

    void set_static_collision(std::span<WorldAABB const> bounds);
    void reset_frame_output();
    void refresh_spatial_index(LevelReadAccess const& level);

    // Start a tick after spawn commits; copy IDs before owners prepare or reorder their rows.
    void collect_spawn_candidates(EntityTypeSizes const& previous_issued_counts,
                                  std::span<EntityId const> capitals,
                                  std::span<EntityId const> fighters,
                                  std::span<EntityId const> turrets);
    void
        track_player_movement(EntityId player, Transform3d const& before, Transform3d const& after);
    // Run after all movement, while published entity rows still identify the same entities.
    void detect_and_handle_overlaps(LevelReadAccess const& level,
                                    std::span<EntityId const> moved_fighters,
                                    CombatEvents& combat_events,
                                    FrameMemoryResource& scratch);

    auto queries() const noexcept -> SpatialQueryManager const& { return queries_; }
    auto queries() noexcept -> SpatialQueryManager& { return queries_; }

  private:
    SpatialQueryManager queries_;
    OverlapHandler overlap_handler_;
    std::vector<EntityId> overlap_candidates_;
};

} // namespace ioj::sim::collision
