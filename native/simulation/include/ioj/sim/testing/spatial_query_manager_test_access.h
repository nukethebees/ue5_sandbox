#pragma once

#include <ioj/sim/spatial_query_manager.h>

namespace ioj::sim {
struct SpatialQueryManagerTestAccess {
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
