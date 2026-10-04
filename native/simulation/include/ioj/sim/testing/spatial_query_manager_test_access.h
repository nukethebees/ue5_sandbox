#pragma once

#include <ioj/sim/spatial_query_manager.h>

namespace ioj::sim {
struct SpatialQueryManagerTestAccess {
    static void set_motion(SpatialQueryManager& manager,
                           EntityType type,
                           Vectors3fConstView locations,
                           Vectors3fConstView velocities = {}) {
        manager.locations_[type] = locations;
        manager.velocities_[type] = velocities;
    }
    static void set_player(SpatialQueryManager& manager, std::optional<PlayerSpatialData> player) {
        set_motion(manager, EntityType::PlayerShip, {});
        if (player) {
            manager.player_spatial_ = *player;
            auto const& location{manager.player_spatial_.location};
            auto const& velocity{manager.player_spatial_.velocity};
            set_motion(manager,
                       EntityType::PlayerShip,
                       {{&location.X, 1}, {&location.Y, 1}, {&location.Z, 1}},
                       {{&velocity.X, 1}, {&velocity.Y, 1}, {&velocity.Z, 1}});
        }
    }
    static auto uniform_grid(SpatialQueryManager& manager) noexcept
        -> collision::CollisionUniformGrid& {
        return manager.collision_system_.uniform_grid_;
    }
    static auto uniform_grid(SpatialQueryManager const& manager) noexcept
        -> collision::CollisionUniformGrid const& {
        return manager.collision_system_.uniform_grid_;
    }
    static auto entity_aabbs(SpatialQueryManager const& manager) noexcept
        -> collision::EntityAABBs const& {
        return manager.collision_system_.entity_aabbs_;
    }
};
} // namespace ioj::sim
