#pragma once

#include "sandbox/simulation/collision_grid.h"
#include "sandbox/simulation/collision_grid_entity_storage.h"
#include "sandbox/simulation/entity_types.h"
#include "sandbox/simulation/query_thread_buffers.h"
#include "sandbox/simulation/vectors3f.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ml::simulation {
struct EntityRegistryQueryView {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return locations.num(); }

    Vectors3fConstView locations;
    Vectors3fConstView velocities;
    std::span<std::int32_t const> generations;
    std::span<std::uint8_t const> alive;
    std::span<std::byte const> teams;
    std::span<std::byte const> entity_types;
};

[[nodiscard]] auto analyse_handle(EntityRegistryQueryView registry,
                                  FRegistryEntityHandle handle) noexcept -> RegistryHandleState;
[[nodiscard]] auto is_valid_alive(EntityRegistryQueryView registry,
                                  FRegistryEntityHandle handle) noexcept -> bool;

[[nodiscard]] auto collect_entities_in_range(EntityRegistryQueryView registry,
                                             Vector3f origin,
                                             float radius,
                                             std::span<FRegistryEntityHandle> out_entities) noexcept
    -> std::int32_t;
[[nodiscard]] auto
    collect_non_team_alive_entities(EntityRegistryQueryView registry,
                                    Team excluded_team,
                                    std::span<FRegistryEntityHandle> out_entities) noexcept
    -> std::int32_t;

[[nodiscard]] auto
    collect_non_team_entities_in_range(collision::GridGeometry geometry,
                                       collision::CollisionGridEntityStorage const& grid_entities,
                                       EntityRegistryQueryView registry,
                                       QueryThreadBuffers& buffers,
                                       Vector3f origin,
                                       float radius,
                                       Team excluded_team,
                                       std::span<FRegistryEntityHandle> out_entities)
        -> std::int32_t;

[[nodiscard]] auto
    collect_entities_of_type_in_range(collision::GridGeometry geometry,
                                      collision::CollisionGridEntityStorage const& grid_entities,
                                      EntityRegistryQueryView registry,
                                      QueryThreadBuffers& buffers,
                                      Vector3f origin,
                                      float radius,
                                      EntityType entity_type,
                                      FRegistryEntityHandle ignored_entity,
                                      std::span<FRegistryEntityHandle> out_entities)
        -> std::int32_t;

[[nodiscard]] auto find_any_non_team_entity(EntityRegistryQueryView registry,
                                            Team excluded_team) noexcept -> FRegistryEntityHandle;
[[nodiscard]] auto find_any_non_team_entity(EntityRegistryQueryView registry,
                                            Team excluded_team,
                                            EntityType entity_type) noexcept
    -> FRegistryEntityHandle;
} // namespace ml::simulation
