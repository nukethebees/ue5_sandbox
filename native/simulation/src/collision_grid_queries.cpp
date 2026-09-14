#include "ioj/sim/collision_grid_queries.h"

#include "ioj/sim/world_aabb_operations.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ioj::sim::collision {
namespace {
enum class TraceKind : std::uint8_t {
    Line,
    Sweep,
};

enum class IgnoredEntityMode : std::uint8_t {
    None,
    PerTrace,
};

auto entity_type_at(EntityRegistryQueryView const registry,
                    RegistryEntityHandle const entity) noexcept -> EntityType {
    return static_cast<EntityType>(std::to_integer<std::uint8_t>(
        registry.entity_types[static_cast<std::size_t>(entity.index)]));
}

template <TraceKind Kind, IgnoredEntityMode IgnoredMode, TraceEntityFilter EntityFilter>
void trace_grid_aabbs(GridGeometry const geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      EntityRegistryQueryView const registry,
                      LineTracesConstView const traces,
                      TraceHitsView const hits,
                      std::span<RegistryEntityHandle const> const ignored_entities,
                      Vector3f const moving_half_extent) {
    traces.validate_array_sizes();
    hits.validate_array_sizes();
    ml::native_soa::require(traces.num() == hits.num());
    if constexpr (IgnoredMode == IgnoredEntityMode::PerTrace) {
        ml::native_soa::require(ignored_entities.size() == static_cast<std::size_t>(traces.num()));
    }

    auto const max_cell_coord{
        CellCoord{geometry.dimensions.x - 1, geometry.dimensions.y - 1, geometry.dimensions.z - 1}};
    auto const static_aabbs{static_storage.aabbs().get_const_view().columns()};
    CellCoord cell_padding{};
    if constexpr (Kind == TraceKind::Sweep) {
        cell_padding = {
            static_cast<int>(std::ceil(moving_half_extent.X / geometry.cell_dimensions.X)),
            static_cast<int>(std::ceil(moving_half_extent.Y / geometry.cell_dimensions.Y)),
            static_cast<int>(std::ceil(moving_half_extent.Z / geometry.cell_dimensions.Z)),
        };
    }

    auto const trace_count{traces.num()};
    for (std::int32_t trace_index{}; trace_index < trace_count; ++trace_index) {
        auto const output_index{static_cast<std::size_t>(trace_index)};
        hits.hits[output_index] = 0;
        hits.entities[output_index] = RegistryEntityHandle{};
        hits.static_geometry_indices[output_index] = -1;

        auto const start{traces.starts[trace_index]};
        auto const end{traces.ends[trace_index]};
        auto const delta{end - start};
        GridTraversal traversal;
        if (!GridTraversal::create(geometry, start, end, traversal)) {
            continue;
        }

        auto current_cell{traversal.current_cell()};
        Vector3f inverse_delta{};
        for (std::int32_t axis{}; axis < 3; ++axis) {
            if (delta.Elements[axis] != 0.0f) {
                inverse_delta.Elements[axis] = 1.0f / delta.Elements[axis];
            }
        }

        auto nearest_t{std::numeric_limits<float>::infinity()};
        RegistryEntityHandle nearest_entity;
        std::int32_t nearest_static_index{-1};
        RegistryEntityHandle ignored_entity{};
        if constexpr (IgnoredMode == IgnoredEntityMode::PerTrace) {
            ignored_entity = ignored_entities[output_index];
        }

        auto const trace_cell{[&](std::int32_t const cell_index) {
            auto const entities{entity_storage.entities_for_cell(cell_index)};
            auto const entity_count{static_cast<std::int32_t>(entities.size())};
            if (entity_count > 0) {
                auto const aabbs{entity_storage.aabbs_for_cell(cell_index)};
                for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
                    auto const entity{entities[static_cast<std::size_t>(entity_index)]};
                    if constexpr (IgnoredMode == IgnoredEntityMode::PerTrace) {
                        if (entity == ignored_entity) {
                            continue;
                        }
                    }
                    if constexpr (EntityFilter == TraceEntityFilter::ExcludeFighters) {
                        if (entity_type_at(registry, entity) == EntityType::Fighter) {
                            continue;
                        }
                    }

                    auto const hit_t{trace_aabb(start,
                                                inverse_delta,
                                                delta,
                                                min_at(aabbs, entity_index),
                                                max_at(aabbs, entity_index),
                                                moving_half_extent)};
                    if (hit_t < nearest_t) {
                        nearest_t = hit_t;
                        nearest_entity = entity;
                        nearest_static_index = -1;
                    }
                }
            }

            auto const static_indices{static_storage.aabb_indices_for_cell(cell_index)};
            for (auto const static_index : static_indices) {
                auto const hit_t{trace_aabb(start,
                                            inverse_delta,
                                            delta,
                                            min_at(static_aabbs, static_index),
                                            max_at(static_aabbs, static_index),
                                            moving_half_extent)};
                if (hit_t < nearest_t) {
                    nearest_t = hit_t;
                    nearest_entity = RegistryEntityHandle{};
                    nearest_static_index = static_index;
                }
            }
        }};

        auto const try_advance_traversal{[&] {
            if (!traversal.advance()) {
                return false;
            }
            current_cell = traversal.current_cell();
            return true;
        }};

        if constexpr (Kind == TraceKind::Sweep) {
            auto const trace_z_range{[&](std::int32_t const x,
                                         std::int32_t const y,
                                         std::int32_t const min_z,
                                         std::int32_t const max_z) {
                auto cell_index{to_index(geometry, {x, y, min_z})};
                auto const plane_stride{geometry.dimensions.x * geometry.dimensions.y};
                for (auto z{min_z}; z <= max_z; ++z) {
                    trace_cell(cell_index);
                    cell_index += plane_stride;
                }
            }};
            auto const trace_yz_plane{
                [&](std::int32_t const x, CellCoord const min_cell, CellCoord const max_cell) {
                    for (auto y{min_cell.y}; y <= max_cell.y; ++y) {
                        trace_z_range(x, y, min_cell.z, max_cell.z);
                    }
                }};
            auto const trace_x_range{[&](std::int32_t const min_x,
                                         std::int32_t const max_x,
                                         CellCoord const min_cell,
                                         CellCoord const max_cell) {
                for (auto x{min_x}; x <= max_x; ++x) {
                    trace_yz_plane(x, min_cell, max_cell);
                }
            }};
            auto const trace_y_range{[&](std::int32_t const x,
                                         std::int32_t const min_y,
                                         std::int32_t const max_y,
                                         std::int32_t const min_z,
                                         std::int32_t const max_z) {
                for (auto y{min_y}; y <= max_y; ++y) {
                    trace_z_range(x, y, min_z, max_z);
                }
            }};
            auto const get_padded_cell_range{[&](CellCoord const centre_cell) {
                auto min_cell{CellCoord{centre_cell.x - cell_padding.x,
                                        centre_cell.y - cell_padding.y,
                                        centre_cell.z - cell_padding.z}};
                auto max_cell{CellCoord{centre_cell.x + cell_padding.x,
                                        centre_cell.y + cell_padding.y,
                                        centre_cell.z + cell_padding.z}};
                for (std::int32_t axis{}; axis < 3; ++axis) {
                    min_cell[axis] = std::max(min_cell[axis], 0);
                    max_cell[axis] = std::min(max_cell[axis], max_cell_coord[axis]);
                }
                return CellCoordBounds{min_cell, max_cell};
            }};

            auto [previous_min_cell, previous_max_cell]{get_padded_cell_range(current_cell)};
            trace_x_range(
                previous_min_cell.x, previous_max_cell.x, previous_min_cell, previous_max_cell);

            while (try_advance_traversal()) {
                auto const [min_cell, max_cell]{get_padded_cell_range(current_cell)};
                trace_x_range(
                    min_cell.x, std::min(max_cell.x, previous_min_cell.x - 1), min_cell, max_cell);

                auto const overlap_min_x{std::max(min_cell.x, previous_min_cell.x)};
                auto const overlap_max_x{std::min(max_cell.x, previous_max_cell.x)};
                for (auto x{overlap_min_x}; x <= overlap_max_x; ++x) {
                    trace_y_range(x,
                                  min_cell.y,
                                  std::min(max_cell.y, previous_min_cell.y - 1),
                                  min_cell.z,
                                  max_cell.z);

                    auto const overlap_min_y{std::max(min_cell.y, previous_min_cell.y)};
                    auto const overlap_max_y{std::min(max_cell.y, previous_max_cell.y)};
                    for (auto y{overlap_min_y}; y <= overlap_max_y; ++y) {
                        trace_z_range(
                            x, y, min_cell.z, std::min(max_cell.z, previous_min_cell.z - 1));
                        trace_z_range(
                            x, y, std::max(min_cell.z, previous_max_cell.z + 1), max_cell.z);
                    }

                    trace_y_range(x,
                                  std::max(min_cell.y, previous_max_cell.y + 1),
                                  max_cell.y,
                                  min_cell.z,
                                  max_cell.z);
                }

                trace_x_range(
                    std::max(min_cell.x, previous_max_cell.x + 1), max_cell.x, min_cell, max_cell);
                previous_min_cell = min_cell;
                previous_max_cell = max_cell;
            }
        } else {
            while (true) {
                trace_cell(to_index(geometry, current_cell));
                if (!try_advance_traversal()) {
                    break;
                }
            }
        }

        if (std::isfinite(nearest_t)) {
            hits.set(trace_index,
                     start + delta * nearest_t,
                     nearest_entity,
                     nearest_static_index,
                     std::uint8_t{1});
        }
    }
}

template <TraceKind Kind>
void dispatch_ignored_mode(GridGeometry const geometry,
                           CollisionGridEntityStorage const& entity_storage,
                           CollisionGridStaticStorage const& static_storage,
                           EntityRegistryQueryView const registry,
                           LineTracesConstView const traces,
                           TraceHitsView const hits,
                           std::span<RegistryEntityHandle const> const ignored_entities,
                           Vector3f const moving_half_extent,
                           TraceEntityFilter const entity_filter) {
    auto const dispatch_filter{[&]<IgnoredEntityMode IgnoredMode>() {
        switch (entity_filter) {
            case TraceEntityFilter::None:
                trace_grid_aabbs<Kind, IgnoredMode, TraceEntityFilter::None>(geometry,
                                                                             entity_storage,
                                                                             static_storage,
                                                                             registry,
                                                                             traces,
                                                                             hits,
                                                                             ignored_entities,
                                                                             moving_half_extent);
                return;
            case TraceEntityFilter::ExcludeFighters:
                trace_grid_aabbs<Kind, IgnoredMode, TraceEntityFilter::ExcludeFighters>(
                    geometry,
                    entity_storage,
                    static_storage,
                    registry,
                    traces,
                    hits,
                    ignored_entities,
                    moving_half_extent);
                return;
        }
    }};

    if (ignored_entities.empty()) {
        dispatch_filter.template operator()<IgnoredEntityMode::None>();
    } else {
        dispatch_filter.template operator()<IgnoredEntityMode::PerTrace>();
    }
}
} // namespace

void trace_grid_lines(GridGeometry const geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      EntityRegistryQueryView const registry,
                      LineTracesConstView const traces,
                      TraceHitsView const hits) {
    trace_grid_aabbs<TraceKind::Line, IgnoredEntityMode::None, TraceEntityFilter::None>(
        geometry, entity_storage, static_storage, registry, traces, hits, {}, {});
}

void trace_grid_lines_ignoring_entities(
    GridGeometry const geometry,
    CollisionGridEntityStorage const& entity_storage,
    CollisionGridStaticStorage const& static_storage,
    EntityRegistryQueryView const registry,
    LineTracesConstView const traces,
    TraceHitsView const hits,
    std::span<RegistryEntityHandle const> const ignored_entities) {
    trace_grid_aabbs<TraceKind::Line, IgnoredEntityMode::PerTrace, TraceEntityFilter::None>(
        geometry, entity_storage, static_storage, registry, traces, hits, ignored_entities, {});
}

void sweep_grid_aabbs(GridGeometry const geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      EntityRegistryQueryView const registry,
                      LineTracesConstView const centre_paths,
                      Vector3f const moving_half_extent,
                      TraceHitsView const hits,
                      std::span<RegistryEntityHandle const> const ignored_entities,
                      TraceEntityFilter const entity_filter) {
    dispatch_ignored_mode<TraceKind::Sweep>(geometry,
                                            entity_storage,
                                            static_storage,
                                            registry,
                                            centre_paths,
                                            hits,
                                            ignored_entities,
                                            moving_half_extent,
                                            entity_filter);
}
} // namespace ioj::sim::collision
