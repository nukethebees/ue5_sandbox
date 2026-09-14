#include "ioj/sim/collision_grid_overlap_query.h"

#include "ioj/sim/world_aabb_operations.h"

namespace ioj::sim::collision {
void append_grid_overlaps(GridGeometry const geometry,
                          CollisionGridEntityStorage const& entity_storage,
                          CollisionGridStaticStorage const& static_storage,
                          EntityRegistryQueryView const registry,
                          WorldAABB const query_bounds,
                          RegistryEntityHandle const ignored_entity,
                          std::vector<RegistryEntityHandle>& out_entities,
                          std::vector<std::int32_t>& out_static_geometry_indices) {
    auto const [min_coord,
                max_coord]{to_cell_coord_bounds(geometry, query_bounds.min, query_bounds.max)};
    auto const overlaps_query{
        [&query_bounds](Vector3f const candidate_min, Vector3f const candidate_max) {
            return query_bounds.min.X <= candidate_max.X && query_bounds.max.X >= candidate_min.X &&
                   query_bounds.min.Y <= candidate_max.Y && query_bounds.max.Y >= candidate_min.Y &&
                   query_bounds.min.Z <= candidate_max.Z && query_bounds.max.Z >= candidate_min.Z;
        }};
    auto const static_aabbs{static_storage.aabbs().get_const_view().columns()};
    auto const row_stride{geometry.dimensions.x};
    auto const plane_stride{row_stride * geometry.dimensions.y};

    auto plane_index{min_coord.x + min_coord.y * row_stride + min_coord.z * plane_stride};
    for (auto z{min_coord.z}; z <= max_coord.z; ++z) {
        auto row_index{plane_index};
        for (auto y{min_coord.y}; y <= max_coord.y; ++y) {
            auto cell_index{row_index};
            for (auto x{min_coord.x}; x <= max_coord.x; ++x, ++cell_index) {
                auto const entities{entity_storage.entities_for_cell(cell_index)};
                auto const entity_count{static_cast<std::int32_t>(entities.size())};
                if (entity_count > 0) {
                    auto const aabbs{entity_storage.aabbs_for_cell(cell_index)};

                    for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
                        auto const entity{entities[static_cast<std::size_t>(entity_index)]};
                        if (entity == ignored_entity || !is_valid_alive(registry, entity)) {
                            continue;
                        }

                        if (overlaps_query(min_at(aabbs, entity_index),
                                           max_at(aabbs, entity_index))) {
                            out_entities.push_back(entity);
                        }
                    }
                }

                auto const static_indices{static_storage.aabb_indices_for_cell(cell_index)};
                for (auto const static_index : static_indices) {
                    if (overlaps_query(min_at(static_aabbs, static_index),
                                       max_at(static_aabbs, static_index))) {
                        out_static_geometry_indices.push_back(static_index);
                    }
                }
            }
            row_index += row_stride;
        }
        plane_index += plane_stride;
    }
}
}
