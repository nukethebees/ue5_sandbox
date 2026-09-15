#include "ioj/sim/entity_registry_query.h"

#include "ioj/sim/entity_registry_bookkeeping.h"

#include <algorithm>
#include <cmath>

namespace ioj::sim {
namespace {
auto byte_value(std::span<std::byte const> const values, std::int32_t const index) noexcept
    -> std::uint8_t {
    return std::to_integer<std::uint8_t>(values[static_cast<std::size_t>(index)]);
}

template <typename IncludeEntity>
auto collect_entities_in_range(collision::GridGeometry const geometry,
                               collision::CollisionGridEntityStorage const& grid_entities,
                               EntityRegistryQueryView const registry,
                               QueryThreadBuffers& buffers,
                               Vector3f const origin,
                               float const radius,
                               std::span<RegistryEntityHandle> const out_entities,
                               IncludeEntity&& include_entity) -> std::int32_t {
    if (out_entities.empty()) {
        return 0;
    }

    auto const absolute_radius{std::abs(radius)};
    auto const radius_extent{ml::make_vector3f(absolute_radius, absolute_radius, absolute_radius)};
    auto [min_coord, max_coord]{
        collision::to_cell_coord_bounds(geometry, origin - radius_extent, origin + radius_extent)};
    auto const max_grid_coord{collision::CellCoord{
        geometry.dimensions.x - 1, geometry.dimensions.y - 1, geometry.dimensions.z - 1}};
    if (max_coord.x < 0 || max_coord.y < 0 || max_coord.z < 0 || min_coord.x > max_grid_coord.x ||
        min_coord.y > max_grid_coord.y || min_coord.z > max_grid_coord.z) {
        return 0;
    }

    min_coord.x = std::max(min_coord.x, 0);
    min_coord.y = std::max(min_coord.y, 0);
    min_coord.z = std::max(min_coord.z, 0);
    max_coord.x = std::min(max_coord.x, max_grid_coord.x);
    max_coord.y = std::min(max_coord.y, max_grid_coord.y);
    max_coord.z = std::min(max_coord.z, max_grid_coord.z);

    auto& entity_stamps{buffers.range_query_entity_stamps};
    buffers.ensure_entity_stamp_count(registry.num());
    auto const query_stamp{buffers.advance_range_query_stamp()};
    auto const radius_squared{radius * radius};
    std::int32_t count{};

    for (auto x{min_coord.x}; x <= max_coord.x; ++x) {
        for (auto y{min_coord.y}; y <= max_coord.y; ++y) {
            for (auto z{min_coord.z}; z <= max_coord.z; ++z) {
                auto const cell_index{collision::to_index(geometry, {x, y, z})};
                for (auto const handle : grid_entities.entities_for_cell(cell_index)) {
                    if (!is_valid_alive(registry, handle)) {
                        continue;
                    }

                    auto const entity_index{static_cast<std::size_t>(handle.index)};
                    if (entity_stamps[entity_index] == query_stamp) {
                        continue;
                    }
                    entity_stamps[entity_index] = query_stamp;

                    if (!include_entity(handle)) {
                        continue;
                    }

                    auto const dx{registry.locations.xs[entity_index] - origin.X};
                    auto const dy{registry.locations.ys[entity_index] - origin.Y};
                    auto const dz{registry.locations.zs[entity_index] - origin.Z};
                    auto const distance_squared{dx * dx + dy * dy + dz * dz};
                    if (distance_squared > radius_squared) {
                        continue;
                    }

                    out_entities[static_cast<std::size_t>(count++)] = handle;
                    if (count >= static_cast<std::int32_t>(out_entities.size())) {
                        return count;
                    }
                }
            }
        }
    }

    return count;
}
} // namespace

auto analyse_handle(EntityRegistryQueryView const registry,
                    RegistryEntityHandle const handle) noexcept -> RegistryHandleState {
    return analyse_handle(registry.generations, handle);
}

auto is_valid_alive(EntityRegistryQueryView const registry,
                    RegistryEntityHandle const handle) noexcept -> bool {
    return analyse_handle(registry, handle) == RegistryHandleState::Active &&
           registry.alive[static_cast<std::size_t>(handle.index)] != 0;
}

auto collect_entities_in_range(EntityRegistryQueryView const registry,
                               Vector3f const origin,
                               float const radius,
                               std::span<RegistryEntityHandle> const out_entities) noexcept
    -> std::int32_t {
    if (out_entities.empty()) {
        return 0;
    }

    auto const radius_squared{radius * radius};
    auto const count{registry.num()};
    std::int32_t output_count{};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const dx{registry.locations.xs[element] - origin.X};
        auto const dy{registry.locations.ys[element] - origin.Y};
        auto const dz{registry.locations.zs[element] - origin.Z};
        auto const distance_squared{dx * dx + dy * dy + dz * dz};
        if (distance_squared <= radius_squared) {
            out_entities[static_cast<std::size_t>(output_count++)] = {
                index, registry.generations[element]};
        }

        if (output_count >= static_cast<std::int32_t>(out_entities.size())) {
            break;
        }
    }
    return output_count;
}

auto collect_non_team_alive_entities(EntityRegistryQueryView const registry,
                                     Team const excluded_team,
                                     std::span<RegistryEntityHandle> const out_entities) noexcept
    -> std::int32_t {
    auto const count{registry.num()};
    std::int32_t output_count{};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (byte_value(registry.teams, index) == static_cast<std::uint8_t>(excluded_team) ||
            registry.alive[element] == 0) {
            continue;
        }

        if (output_count >= static_cast<std::int32_t>(out_entities.size())) {
            break;
        }
        out_entities[static_cast<std::size_t>(output_count++)] = {index,
                                                                  registry.generations[element]};
    }
    return output_count;
}

auto collect_non_team_entities_in_range(collision::GridGeometry const geometry,
                                        collision::CollisionGridEntityStorage const& grid_entities,
                                        EntityRegistryQueryView const registry,
                                        QueryThreadBuffers& buffers,
                                        Vector3f const origin,
                                        float const radius,
                                        Team const excluded_team,
                                        std::span<RegistryEntityHandle> const out_entities)
    -> std::int32_t {
    return collect_entities_in_range(geometry,
                                     grid_entities,
                                     registry,
                                     buffers,
                                     origin,
                                     radius,
                                     out_entities,
                                     [registry, excluded_team](RegistryEntityHandle const handle) {
                                         return byte_value(registry.teams, handle.index) !=
                                                static_cast<std::uint8_t>(excluded_team);
                                     });
}

auto collect_entities_of_type_in_range(collision::GridGeometry const geometry,
                                       collision::CollisionGridEntityStorage const& grid_entities,
                                       EntityRegistryQueryView const registry,
                                       QueryThreadBuffers& buffers,
                                       Vector3f const origin,
                                       float const radius,
                                       EntityType const entity_type,
                                       RegistryEntityHandle const ignored_entity,
                                       std::span<RegistryEntityHandle> const out_entities)
    -> std::int32_t {
    return collect_entities_in_range(
        geometry,
        grid_entities,
        registry,
        buffers,
        origin,
        radius,
        out_entities,
        [registry, entity_type, ignored_entity](RegistryEntityHandle const handle) {
            return handle != ignored_entity && byte_value(registry.entity_types, handle.index) ==
                                                   static_cast<std::uint8_t>(entity_type);
        });
}

auto find_any_non_team_entity(EntityRegistryQueryView const registry,
                              Team const excluded_team) noexcept -> RegistryEntityHandle {
    auto const count{registry.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (registry.alive[element] != 0 &&
            byte_value(registry.teams, index) != static_cast<std::uint8_t>(excluded_team)) {
            return {index, registry.generations[element]};
        }
    }
    return {};
}

auto find_any_non_team_entity(EntityRegistryQueryView const registry,
                              Team const excluded_team,
                              EntityType const entity_type) noexcept -> RegistryEntityHandle {
    auto const count{registry.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (registry.alive[element] != 0 &&
            byte_value(registry.teams, index) != static_cast<std::uint8_t>(excluded_team) &&
            byte_value(registry.entity_types, index) == static_cast<std::uint8_t>(entity_type)) {
            return {index, registry.generations[element]};
        }
    }
    return {};
}
} // namespace ioj::sim
