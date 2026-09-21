#include "ioj/sim/collision/collision_uniform_grid.h"

#include <sandbox/core/diagnostics.h>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/collision_grid.h>
#include <ioj/sim/entity_cell_data_operations.h>
#include <ioj/sim/health.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/trace_hits.h>
#include <ioj/sim/world_aabb_operations.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <format>
#include <utility>

namespace ioj::sim::collision {
namespace collision_uniform_grid_detail {
namespace {
enum class TraceKind : std::uint8_t {
    Line,
    Sweep,
};

enum class IgnoredEntityMode : std::uint8_t {
    None,
    PerTrace,
};

auto aabbs_for_cell(CollisionGridEntityStorage const& storage, CellIndex const cell_index) noexcept
    -> WorldAABBsColumnsConstView {
    assert(cell_index >= 0 && static_cast<std::size_t>(cell_index) < storage.cell_counts.size());

    auto const element{static_cast<std::size_t>(cell_index)};
    return storage.aabbs.get_const_view(storage.cell_offsets[element], storage.cell_counts[element])
        .columns();
}

template <TraceKind Kind, IgnoredEntityMode IgnoredMode, TraceEntityFilter EntityFilter>
void trace_grid_aabbs(GridGeometry const geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      AgentAccessor const& agents,
                      LineTracesConstView const traces,
                      TraceHitsView const hits,
                      std::span<EntityUniqueId const> const ignored_entities,
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
        hits.entities[output_index] = EntityUniqueId{};
        hits.static_geometry_indices[output_index] = invalid_static_geometry_index;

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

        auto nearest_t{no_trace_hit};
        EntityUniqueId nearest_entity;
        StaticGeometryIndex nearest_static_index{invalid_static_geometry_index};
        EntityUniqueId ignored_entity{};
        if constexpr (IgnoredMode == IgnoredEntityMode::PerTrace) {
            ignored_entity = ignored_entities[output_index];
        }

        auto const trace_cell{[&](CellIndex const cell_index) {
            assert(cell_index >= 0 &&
                   static_cast<std::size_t>(cell_index) < entity_storage.cell_counts.size());
            auto const element{static_cast<std::size_t>(cell_index)};
            auto const count{entity_storage.cell_counts[element]};
            auto const entity_count{static_cast<std::int32_t>(count)};
            if (entity_count > 0) {
                auto const entities{std::span{entity_storage.entities}.subspan(
                    static_cast<std::size_t>(entity_storage.cell_offsets[element]), count)};
                auto const aabbs{aabbs_for_cell(entity_storage, cell_index)};
                for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
                    auto const entity{entities[static_cast<std::size_t>(entity_index)]};
                    auto const id{entity};
                    if (!agents.is_alive(id)) {
                        continue;
                    }
                    if constexpr (IgnoredMode == IgnoredEntityMode::PerTrace) {
                        if (id == ignored_entity) {
                            continue;
                        }
                    }
                    if constexpr (EntityFilter == TraceEntityFilter::ExcludeFighters) {
                        if (id.entity_type() == EntityType::Fighter) {
                            continue;
                        }
                    }

                    auto const hit_t{trace_aabb(start,
                                                inverse_delta,
                                                delta,
                                                min_at(aabbs, entity_index),
                                                max_at(aabbs, entity_index),
                                                moving_half_extent)};
                    if (hit_t < nearest_t ||
                        (std::isfinite(hit_t) && hit_t == nearest_t && id < nearest_entity)) {
                        nearest_t = hit_t;
                        nearest_entity = id;
                        nearest_static_index = invalid_static_geometry_index;
                    }
                }
            }

            auto const static_indices{static_storage.aabb_indices_for_cell(cell_index)};
            for (auto const static_index : static_indices) {
                auto const static_aabb_index{static_cast<StaticGeometryIndex>(static_index)};
                auto const hit_t{trace_aabb(start,
                                            inverse_delta,
                                            delta,
                                            min_at(static_aabbs, static_aabb_index),
                                            max_at(static_aabbs, static_aabb_index),
                                            moving_half_extent)};
                if (hit_t < nearest_t) {
                    nearest_t = hit_t;
                    nearest_entity = EntityUniqueId{};
                    nearest_static_index = static_aabb_index;
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
                           AgentAccessor const& agents,
                           LineTracesConstView const traces,
                           TraceHitsView const hits,
                           std::span<EntityUniqueId const> const ignored_entities,
                           Vector3f const moving_half_extent,
                           TraceEntityFilter const entity_filter) {
    auto const dispatch_filter{[&]<IgnoredEntityMode IgnoredMode>() {
        switch (entity_filter) {
            case TraceEntityFilter::None:
                trace_grid_aabbs<Kind, IgnoredMode, TraceEntityFilter::None>(geometry,
                                                                             entity_storage,
                                                                             static_storage,
                                                                             agents,
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
                    agents,
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
                      AgentAccessor const& agents,
                      LineTracesConstView const traces,
                      TraceHitsView const hits) {
    trace_grid_aabbs<TraceKind::Line, IgnoredEntityMode::None, TraceEntityFilter::None>(
        geometry, entity_storage, static_storage, agents, traces, hits, {}, {});
}

void trace_grid_lines_ignoring_entities(GridGeometry const geometry,
                                        CollisionGridEntityStorage const& entity_storage,
                                        CollisionGridStaticStorage const& static_storage,
                                        AgentAccessor const& agents,
                                        LineTracesConstView const traces,
                                        TraceHitsView const hits,
                                        std::span<EntityUniqueId const> const ignored_entities) {
    trace_grid_aabbs<TraceKind::Line, IgnoredEntityMode::PerTrace, TraceEntityFilter::None>(
        geometry, entity_storage, static_storage, agents, traces, hits, ignored_entities, {});
}

void sweep_grid_aabbs(GridGeometry const geometry,
                      CollisionGridEntityStorage const& entity_storage,
                      CollisionGridStaticStorage const& static_storage,
                      AgentAccessor const& agents,
                      LineTracesConstView const centre_paths,
                      Vector3f const moving_half_extent,
                      TraceHitsView const hits,
                      std::span<EntityUniqueId const> const ignored_entities,
                      TraceEntityFilter const entity_filter) {
    dispatch_ignored_mode<TraceKind::Sweep>(geometry,
                                            entity_storage,
                                            static_storage,
                                            agents,
                                            centre_paths,
                                            hits,
                                            ignored_entities,
                                            moving_half_extent,
                                            entity_filter);
}
void append_grid_overlaps(GridGeometry const geometry,
                          CollisionGridEntityStorage const& entity_storage,
                          CollisionGridStaticStorage const& static_storage,
                          AgentAccessor const& agents,
                          WorldAABB const query_bounds,
                          EntityUniqueId const ignored_entity,
                          ml::FrameArray<EntityUniqueId>& out_entities,
                          ml::FrameArray<StaticGeometryIndex>& out_static_geometry_indices) {
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
                assert(cell_index >= 0 &&
                       static_cast<std::size_t>(cell_index) < entity_storage.cell_counts.size());
                auto const element{static_cast<std::size_t>(cell_index)};
                auto const count{entity_storage.cell_counts[element]};
                auto const entity_count{static_cast<std::int32_t>(count)};
                if (entity_count > 0) {
                    auto const entities{std::span{entity_storage.entities}.subspan(
                        static_cast<std::size_t>(entity_storage.cell_offsets[element]), count)};
                    auto const aabbs{aabbs_for_cell(entity_storage, cell_index)};

                    for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
                        auto const entity{entities[static_cast<std::size_t>(entity_index)]};
                        auto const id{entity};
                        if (id == ignored_entity || !agents.is_alive(id)) {
                            continue;
                        }

                        if (overlaps_query(min_at(aabbs, entity_index),
                                           max_at(aabbs, entity_index))) {
                            out_entities.add(id);
                        }
                    }
                }

                auto const static_indices{static_storage.aabb_indices_for_cell(cell_index)};
                for (auto const static_index : static_indices) {
                    auto const static_aabb_index{static_cast<StaticGeometryIndex>(static_index)};
                    if (overlaps_query(min_at(static_aabbs, static_aabb_index),
                                       max_at(static_aabbs, static_aabb_index))) {
                        out_static_geometry_indices.add(static_aabb_index);
                    }
                }
            }
            row_index += row_stride;
        }
        plane_index += plane_stride;
    }
}
}

auto CollisionUniformGrid::get_grid_dims() const noexcept -> collision::CellCoord {
    return geometry_.dimensions;
}
void CollisionUniformGrid::set_grid_dims(collision::CellCoord const grid_dims) noexcept {
    geometry_.dimensions = grid_dims;
}

auto CollisionUniformGrid::get_cell_dims() const noexcept -> Vector3f {
    return geometry_.cell_dimensions;
}
void CollisionUniformGrid::set_cell_dims(Vector3f const cell_dims) noexcept {
    geometry_.cell_dimensions = cell_dims;
}

CollisionUniformGrid::CollisionUniformGrid(AgentAccessor const& agents) noexcept
    : agents_{agents} {}

auto CollisionUniformGrid::is_configured() const noexcept -> bool {
    return collision::is_configured(geometry_);
}

auto CollisionUniformGrid::num_cells() const -> std::int32_t {
    return collision::num_cells(geometry_);
}

void CollisionUniformGrid::reset() {
    geometry_ = {};

    auto& storage{entity_storage_};
    storage.cell_offsets.clear();
    storage.cell_counts.clear();
    storage.non_empty_cell_indices.clear();
    storage.entities.clear();
    storage.aabbs.reset();
    storage.cell_write_indices.clear();
    storage.rebuild_entity_data.reset();

    static_storage_.reset();
}

void CollisionUniformGrid::set_static_aabbs(collision::WorldAABBs static_aabbs) {
    SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::set_static_aabbs");

    if (!is_configured()) {
        ml::fatal_error("Cannot build static geometry for an unconfigured grid");
    }

    static_aabbs.get_const_view().columns().validate_array_sizes();
    static_storage_.set_aabbs(std::move(static_aabbs));
    rebuild_static_grid();
}

auto CollisionUniformGrid::add_static_aabb(Vector3f const min_point, Vector3f const max_point)
    -> StaticGeometryIndex {
    SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::add_static_aabb");

    assert(is_configured());
    [[maybe_unused]] auto const [min_coord, max_coord]{to_cell_coord_bounds(min_point, max_point)};
    assert(is_cell_coord_in_bounds(min_coord, max_coord));
    assert(std::isfinite(min_point.X) && std::isfinite(min_point.Y) && std::isfinite(min_point.Z));
    assert(std::isfinite(max_point.X) && std::isfinite(max_point.Y) && std::isfinite(max_point.Z));

    auto const static_index{static_storage_.add_aabb(min_point, max_point)};
    rebuild_static_grid();
    return static_index;
}

void CollisionUniformGrid::rebuild_static_grid() {
    SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::rebuild_static_grid");

    auto const result{static_storage_.rebuild(geometry_)};
    if (!result) {
        auto const error{result.error()};
        ml::fatal_error(
            std::format("Static collision grid build failed: code={}, AABB={}, cell={}, count={}",
                        std::to_underlying(error.code),
                        error.aabb_index,
                        error.cell_index,
                        error.count));
    }
}

void CollisionUniformGrid::rebuild_entity_grid(collision::EntityAABBs const& entity_aabbs) {
    SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::rebuild_entity_grid");
    if (!is_configured()) {
        ml::fatal_error("Cannot rebuild an unconfigured collision grid");
    }

    auto const& geometry{geometry_};
    auto& storage{entity_storage_};
    auto const dimensions{geometry.dimensions};
    assert(dimensions.x > 0 && dimensions.y > 0 && dimensions.z > 0);
    auto const row_stride{dimensions.x};
    auto const plane_stride{row_stride * dimensions.y};

    {
        SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::prepare_entity_grid");

        for (auto const cell_index : storage.non_empty_cell_indices) {
            storage.cell_counts[static_cast<std::size_t>(cell_index)] = 0;
        }
        storage.non_empty_cell_indices.clear();

        auto const cell_count{static_cast<std::size_t>(dimensions.x) *
                              static_cast<std::size_t>(dimensions.y) *
                              static_cast<std::size_t>(dimensions.z)};
        if (storage.cell_counts.size() != cell_count) {
            storage.cell_counts.assign(cell_count, std::uint16_t{});
        }
        storage.cell_offsets.resize(cell_count);
        storage.cell_write_indices.resize(cell_count);
        storage.rebuild_entity_data.reset();
    }

    {
        SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::gather_and_count_entities");

        agents_.for_each_alive_spatial(
            [&](EntityUniqueId const id, Vector3f const location, Rotator3f const rotation, Team) {
                auto const entity_type{id.entity_type()};
                auto const bounds{collision::make_entity_world_bounds(
                    entity_aabbs, entity_type, location, to_quaternion(rotation))};
                auto const [min_coord, max_coord]{
                    collision::to_cell_coord_bounds(geometry, bounds.min, bounds.max)};
                if (!is_cell_coord_in_bounds(min_coord, max_coord)) {
                    ml::fatal_error(std::format(
                        "Collision-grid entity ID {} type {} has world AABB ({}, {}, {}) through "
                        "({}, {}, {}), cell AABB {} through {}, outside grid dimensions {}",
                        id.raw_value(),
                        std::to_underlying(entity_type),
                        bounds.min.X,
                        bounds.min.Y,
                        bounds.min.Z,
                        bounds.max.X,
                        bounds.max.Y,
                        bounds.max.Z,
                        to_string(min_coord),
                        to_string(max_coord),
                        to_string(geometry.dimensions)));
                }

                collision::add(
                    storage.rebuild_entity_data, bounds.min, bounds.max, min_coord, max_coord, id);

                auto plane_index{min_coord.x + min_coord.y * row_stride +
                                 min_coord.z * plane_stride};
                for (auto z{min_coord.z}; z <= max_coord.z; ++z) {
                    auto row_index{plane_index};
                    for (auto y{min_coord.y}; y <= max_coord.y; ++y) {
                        auto cell_index{row_index};
                        for (auto x{min_coord.x}; x <= max_coord.x; ++x, ++cell_index) {
                            auto& count{storage.cell_counts[static_cast<std::size_t>(cell_index)]};
                            if (count == 0) {
                                storage.non_empty_cell_indices.push_back(cell_index);
                            }
                            ++count;
                        }
                        row_index += row_stride;
                    }
                    plane_index += plane_stride;
                }
            });
    }

    {
        SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::calculate_entity_cell_offsets");

        std::int32_t entry_count{};
        for (auto const cell_index : storage.non_empty_cell_indices) {
            auto const element{static_cast<std::size_t>(cell_index)};
            storage.cell_offsets[element] = entry_count;
            storage.cell_write_indices[element] = entry_count;
            entry_count += storage.cell_counts[element];
        }

        storage.aabbs.reset();
        storage.aabbs.add_uninitialised(entry_count);
        storage.entities.resize(static_cast<std::size_t>(entry_count));
    }

    {
        SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::scatter_entities");

        auto const rebuild_entity_data{storage.rebuild_entity_data.get_const_view().columns()};
        auto const entity_count{rebuild_entity_data.num()};

        for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
            auto const min_cell{min_cell_at(rebuild_entity_data, entity_index)};
            auto const max_cell{max_cell_at(rebuild_entity_data, entity_index)};
            auto const min_point{min_point_at(rebuild_entity_data, entity_index)};
            auto const max_point{max_point_at(rebuild_entity_data, entity_index)};

            auto plane_index{min_cell.x + min_cell.y * row_stride + min_cell.z * plane_stride};
            for (auto z{min_cell.z}; z <= max_cell.z; ++z) {
                auto row_index{plane_index};
                for (auto y{min_cell.y}; y <= max_cell.y; ++y) {
                    auto cell_index{row_index};
                    for (auto x{min_cell.x}; x <= max_cell.x; ++x, ++cell_index) {
                        auto& write_index{
                            storage.cell_write_indices[static_cast<std::size_t>(cell_index)]};
                        auto const destination{write_index++};
                        storage.entities[static_cast<std::size_t>(destination)] =
                            rebuild_entity_data.entity_ids[static_cast<std::size_t>(entity_index)];
                        collision::set(storage.aabbs, destination, min_point, max_point);
                    }
                    row_index += row_stride;
                }
                plane_index += plane_stride;
            }
        }
    }

    {
        SANDBOX_PROFILE_SCOPE("CollisionUniformGrid::validate_entity_grid");

        for (auto const cell_index : storage.non_empty_cell_indices) {
            auto const element{static_cast<std::size_t>(cell_index)};
            if (storage.cell_write_indices[element] !=
                storage.cell_offsets[element] + storage.cell_counts[element]) {
                ml::fatal_error("Collision grid entity membership index is inconsistent");
            }
        }
    }
}

auto CollisionUniformGrid::get_entity_world_bounds() const -> WorldAABBsColumnsConstView {
    auto const entity_data{entity_storage_.rebuild_entity_data.get_const_view().columns()};
    return {entity_data.min_point_xs,
            entity_data.min_point_ys,
            entity_data.min_point_zs,
            entity_data.max_point_xs,
            entity_data.max_point_ys,
            entity_data.max_point_zs};
}

void CollisionUniformGrid::append_overlaps(
    collision::WorldAABB const& query_bounds,
    EntityUniqueId const ignored_entity,
    ml::FrameArray<EntityUniqueId>& out_entities,
    ml::FrameArray<StaticGeometryIndex>& out_static_geometry_indices) const {

    [[maybe_unused]] auto const [min_coord, max_coord]{
        collision::to_cell_coord_bounds(geometry_, query_bounds.min, query_bounds.max)};
    assert(is_cell_coord_in_bounds(min_coord, max_coord));

    collision_uniform_grid_detail::append_grid_overlaps(geometry_,
                                                        entity_storage_,
                                                        static_storage_,
                                                        agents_,
                                                        query_bounds,
                                                        ignored_entity,
                                                        out_entities,
                                                        out_static_geometry_indices);
}

void CollisionUniformGrid::trace_aabbs(LineTracesConstView const& traces,
                                       TraceHitsView const& hits) const {
    collision_uniform_grid_detail::trace_grid_lines(
        geometry_, entity_storage_, static_storage_, agents_, traces, hits);
}

void CollisionUniformGrid::trace_aabbs(
    LineTracesConstView const& traces,
    TraceHitsView const& hits,
    std::span<EntityUniqueId const> const ignored_entities) const {
    collision_uniform_grid_detail::trace_grid_lines_ignoring_entities(
        geometry_,
        entity_storage_,
        static_storage_,
        agents_,
        traces,
        hits,
        {ignored_entities.data(), static_cast<std::size_t>(ignored_entities.size())});
}

void CollisionUniformGrid::sweep_aabbs(LineTracesConstView const& centre_paths,
                                       Vector3f const moving_half_extent,
                                       TraceHitsView const& hits,
                                       std::span<EntityUniqueId const> const ignored_entities,
                                       TraceEntityFilter const entity_filter) const {
    assert(std::isfinite(moving_half_extent.X) && std::isfinite(moving_half_extent.Y) &&
           std::isfinite(moving_half_extent.Z));
    assert(moving_half_extent.X >= 0.f);
    assert(moving_half_extent.Y >= 0.f);
    assert(moving_half_extent.Z >= 0.f);
    collision_uniform_grid_detail::sweep_grid_aabbs(
        geometry_,
        entity_storage_,
        static_storage_,
        agents_,
        centre_paths,
        moving_half_extent,
        hits,
        {ignored_entities.data(), static_cast<std::size_t>(ignored_entities.size())},
        entity_filter);
}

auto CollisionUniformGrid::to_cell_coord(Vector3f const pos) const -> collision::CellCoord {
    return collision::to_cell_coord(geometry_, pos);
}
auto CollisionUniformGrid::to_cell_coord_bounds(Vector3f const min_point,
                                                Vector3f const max_point) const -> CellCoordBounds {
    auto const bounds{collision::to_cell_coord_bounds(geometry_, min_point, max_point)};
    return {bounds.min, bounds.max};
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(collision::CellCoord const coord) const -> bool {
    return collision::is_cell_coord_in_bounds(geometry_, coord);
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(collision::CellCoord const min_coord,
                                                   collision::CellCoord const max_coord) const
    -> bool {
    return is_cell_coord_in_bounds(min_coord) && is_cell_coord_in_bounds(max_coord);
}
void CollisionUniformGrid::are_spheres_in_bounds(Vectors3fConstView const centres,
                                                 float const radius,
                                                 std::span<std::uint8_t> const out_results) const {
    [[maybe_unused]] auto const count{centres.num()};
    assert(out_results.size() == static_cast<std::size_t>(count));
    assert(std::isfinite(radius));
    assert(radius >= 0.f);

    collision::are_spheres_in_bounds(
        geometry_,
        centres,
        radius,
        {out_results.data(), static_cast<std::size_t>(out_results.size())});
}
auto CollisionUniformGrid::to_string(collision::CellCoord const value) -> std::string {
    return std::format("({}, {}, {})", value.x, value.y, value.z);
}
}
