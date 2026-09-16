#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/collision_grid_entity_storage.h"
#include "ioj/sim/entity_types.h"
#include "ioj/sim/query_thread_buffers.h"
#include "ioj/sim/vectors3f.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
struct EntityRegistryQueryView {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return locations.num(); }

    Vectors3fConstView locations;
    Vectors3fConstView velocities;
    std::span<std::int32_t const> generations;
    std::span<std::int32_t const> healths;
    std::span<std::byte const> teams;
    std::span<std::byte const> entity_types;
};

[[nodiscard]] auto analyse_handle(EntityRegistryQueryView registry,
                                  RegistryEntityHandle handle) noexcept -> RegistryHandleState;
[[nodiscard]] auto is_valid_alive(EntityRegistryQueryView registry,
                                  RegistryEntityHandle handle) noexcept -> bool;

[[nodiscard]] auto collect_entities_in_range(EntityRegistryQueryView registry,
                                             Vector3f origin,
                                             float radius,
                                             std::span<RegistryEntityHandle> out_entities) noexcept
    -> std::int32_t;
[[nodiscard]] auto
    collect_non_team_alive_entities(EntityRegistryQueryView registry,
                                    Team excluded_team,
                                    std::span<RegistryEntityHandle> out_entities) noexcept
    -> std::int32_t;

[[nodiscard]] auto
    collect_non_team_entities_in_range(collision::GridGeometry geometry,
                                       collision::CollisionGridEntityStorage const& grid_entities,
                                       EntityRegistryQueryView registry,
                                       QueryThreadBuffers& buffers,
                                       Vector3f origin,
                                       float radius,
                                       Team excluded_team,
                                       std::span<RegistryEntityHandle> out_entities)
        -> std::int32_t;

[[nodiscard]] auto
    collect_entities_of_type_in_range(collision::GridGeometry geometry,
                                      collision::CollisionGridEntityStorage const& grid_entities,
                                      EntityRegistryQueryView registry,
                                      QueryThreadBuffers& buffers,
                                      Vector3f origin,
                                      float radius,
                                      EntityType entity_type,
                                      RegistryEntityHandle ignored_entity,
                                      std::span<RegistryEntityHandle> out_entities) -> std::int32_t;

[[nodiscard]] auto find_any_non_team_entity(EntityRegistryQueryView registry,
                                            Team excluded_team) noexcept -> RegistryEntityHandle;
[[nodiscard]] auto find_any_non_team_entity(EntityRegistryQueryView registry,
                                            Team excluded_team,
                                            EntityType entity_type) noexcept
    -> RegistryEntityHandle;
} // namespace ioj::sim
