#include "ioj/sim/collision/collision_uniform_grid.h"

#include <ioj/sim/collision_grid.h>
#include <ioj/sim/entity_cell_data_operations.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/entity_registry_view.h>
#include <ioj/sim/trace_hits.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/world_aabb_operations.h>
#include <limits>
#include <sandbox/core/diagnostics.h>
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

namespace {
static_assert(ioj::sim::collision::EntityAABBs::space_ship_index ==
              std::to_underlying(ioj::sim::EntityType::PlayerShip));
static_assert(ioj::sim::collision::EntityAABBs::static_turret_index ==
              std::to_underlying(ioj::sim::EntityType::Turret));
static_assert(ioj::sim::collision::EntityAABBs::capital_ship_index ==
              std::to_underlying(ioj::sim::EntityType::CapitalShip));
static_assert(ioj::sim::collision::EntityAABBs::fighter_index ==
              std::to_underlying(ioj::sim::EntityType::Fighter));
static_assert(ioj::sim::collision::EntityAABBs::tube_spinner_index ==
              std::to_underlying(ioj::sim::EntityType::TubeSpinner));
static_assert(ioj::sim::collision::EntityAABBs::num_rows ==
              std::to_underlying(ioj::sim::EntityType::COUNT));

}

auto CollisionUniformGrid::get_grid_dims() const noexcept -> ioj::sim::collision::CellCoord {
    return geometry_.dimensions;
}
void CollisionUniformGrid::set_grid_dims(ioj::sim::collision::CellCoord const grid_dims) noexcept {
    geometry_.dimensions = grid_dims;
}

auto CollisionUniformGrid::get_cell_dims() const noexcept -> ioj::sim::Vector3f {
    return geometry_.cell_dimensions;
}
void CollisionUniformGrid::set_cell_dims(ioj::sim::Vector3f const cell_dims) noexcept {
    geometry_.cell_dimensions = cell_dims;
}

CollisionUniformGrid::CollisionUniformGrid(EntityRegistry const& entity_registry) noexcept
    : entity_registry_{entity_registry} {}

auto CollisionUniformGrid::is_configured() const noexcept -> bool {
    return ioj::sim::collision::is_configured(geometry_);
}

auto CollisionUniformGrid::num_cells() const -> std::int32_t {
    return ioj::sim::collision::num_cells(geometry_);
}
auto CollisionUniformGrid::get_cell_entities(ioj::sim::collision::CellCoord const cell_coord) const
    -> std::span<RegistryEntityHandle const> {
    assert(is_cell_coord_in_bounds(cell_coord));

    auto const cell_index{to_index(cell_coord)};
    return entity_storage_.entities_for_cell(cell_index);
}

void CollisionUniformGrid::reset() {
    geometry_ = {};
    entity_storage_.reset();
    static_storage_.reset();
}

void CollisionUniformGrid::set_static_aabbs(ioj::sim::collision::WorldAABBs static_aabbs) {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionUniformGrid::set_static_aabbs");

    if (!is_configured()) {
        ml::fatal_error("Cannot build static geometry for an unconfigured grid");
    }

    static_aabbs.get_const_view().columns().validate_array_sizes();
    static_storage_.set_aabbs(std::move(static_aabbs));
    rebuild_static_grid();
}

auto CollisionUniformGrid::add_static_aabb(ioj::sim::Vector3f const min_point,
                                           ioj::sim::Vector3f const max_point) -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionUniformGrid::add_static_aabb");

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

void CollisionUniformGrid::rebuild_grid(ioj::sim::collision::EntityAABBs const& entity_aabbs) {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionUniformGrid::rebuild_grid");
    telemetry_.record_rebuild();
    if (!is_configured()) {
        ml::fatal_error("Cannot rebuild an unconfigured collision grid");
    }

    auto const& entity_data{entity_registry_.get_entity_data()};
    auto const entity_count{entity_registry_.get_num_elements()};
    auto const generations{entity_registry_.get_generations()};
    auto const geometry{geometry_};
    entity_storage_.begin_rebuild(geometry.dimensions);

    {
        SANDBOX_PROFILE_SCOPE("Sandbox::CollisionUniformGrid::rebuild_grid::count_loop");

        for (std::int32_t index{}; index < entity_count; ++index) {
            if (entity_data.alive[index] == 0) {
                continue;
            }
            auto const entity_type{entity_data.entity_types[index]};
            auto const bounds{ioj::sim::collision::make_entity_world_bounds(
                entity_aabbs,
                std::to_underlying(entity_type),
                entity_data.locations[index],
                ioj::sim::to_quaternion(entity_data.rotations[index]))};
            auto const [min_coord, max_coord]{
                ioj::sim::collision::to_cell_coord_bounds(geometry, bounds.min, bounds.max)};
            if (!is_cell_coord_in_bounds(min_coord, max_coord)) {
                ml::fatal_error(std::format(
                    "Collision-grid entity {}:{} type {} has world AABB ({}, {}, {}) through "
                    "({}, {}, {}), cell AABB {} through {}, outside grid dimensions {}",
                    index,
                    generations[index],
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
            entity_storage_.add(
                bounds.min, bounds.max, min_coord, max_coord, {index, generations[index]});
        }
    }
    if (!entity_storage_.finish_rebuild()) {
        ml::fatal_error("Collision grid entity membership index is inconsistent");
    }
}

void CollisionUniformGrid::append_overlaps(
    ioj::sim::collision::WorldAABB const& query_bounds,
    RegistryEntityHandle const ignored_entity,
    std::vector<RegistryEntityHandle>& out_entities,
    std::vector<std::int32_t>& out_static_geometry_indices) const {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionUniformGrid::append_overlaps");

    auto const geometry{geometry_};
    [[maybe_unused]] auto const [min_coord, max_coord]{
        ioj::sim::collision::to_cell_coord_bounds(geometry, query_bounds.min, query_bounds.max)};
    assert(is_cell_coord_in_bounds(min_coord, max_coord));

    collision_uniform_grid_detail::append_grid_overlaps(
        geometry_,
        entity_storage_,
        static_storage_,
        ioj::sim::make_native_query_view(entity_registry_),
        query_bounds,
        ignored_entity,
        out_entities,
        out_static_geometry_indices);
}

void CollisionUniformGrid::trace_aabbs(ioj::sim::LineTracesConstView const& traces,
                                       TraceHitsView const& hits) const {
    telemetry_.record_line_traces(static_cast<std::uint64_t>(traces.num()));
    collision_uniform_grid_detail::trace_grid_lines(
        geometry_,
        entity_storage_,
        static_storage_,
        ioj::sim::make_native_query_view(entity_registry_),
        traces,
        hits);
}

void CollisionUniformGrid::trace_aabbs(
    ioj::sim::LineTracesConstView const& traces,
    TraceHitsView const& hits,
    std::span<RegistryEntityHandle const> const ignored_entities) const {
    telemetry_.record_line_traces(static_cast<std::uint64_t>(traces.num()));
    collision_uniform_grid_detail::trace_grid_lines_ignoring_entities(
        geometry_,
        entity_storage_,
        static_storage_,
        ioj::sim::make_native_query_view(entity_registry_),
        traces,
        hits,
        {ignored_entities.data(), static_cast<std::size_t>(ignored_entities.size())});
}

void CollisionUniformGrid::sweep_aabbs(ioj::sim::LineTracesConstView const& centre_paths,
                                       ioj::sim::Vector3f const moving_half_extent,
                                       TraceHitsView const& hits,
                                       std::span<RegistryEntityHandle const> const ignored_entities,
                                       TraceEntityFilter const entity_filter) const {
    telemetry_.record_sweep_traces(static_cast<std::uint64_t>(centre_paths.num()));
    assert(std::isfinite(moving_half_extent.X) && std::isfinite(moving_half_extent.Y) &&
           std::isfinite(moving_half_extent.Z));
    assert(moving_half_extent.X >= 0.f);
    assert(moving_half_extent.Y >= 0.f);
    assert(moving_half_extent.Z >= 0.f);
    collision_uniform_grid_detail::sweep_grid_aabbs(
        geometry_,
        entity_storage_,
        static_storage_,
        ioj::sim::make_native_query_view(entity_registry_),
        centre_paths,
        moving_half_extent,
        hits,
        {ignored_entities.data(), static_cast<std::size_t>(ignored_entities.size())},
        entity_filter);
}

void CollisionUniformGrid::reset_runtime_telemetry() noexcept {
    telemetry_.reset();
}

auto CollisionUniformGrid::get_runtime_telemetry() const noexcept
    -> CollisionGridTelemetrySnapshot {
    return telemetry_.snapshot();
}

auto CollisionUniformGrid::to_cell_x(float const value) const -> std::int32_t {
    return ioj::sim::collision::to_cell_coord(
        value, geometry_.cell_dimensions.X, geometry_.dimensions.x);
}
auto CollisionUniformGrid::to_cell_y(float const value) const -> std::int32_t {
    return ioj::sim::collision::to_cell_coord(
        value, geometry_.cell_dimensions.Y, geometry_.dimensions.y);
}
auto CollisionUniformGrid::to_cell_z(float const value) const -> std::int32_t {
    return ioj::sim::collision::to_cell_coord(
        value, geometry_.cell_dimensions.Z, geometry_.dimensions.z);
}
auto CollisionUniformGrid::to_cell_coord(ioj::sim::Vector3f const pos) const
    -> ioj::sim::collision::CellCoord {
    return ioj::sim::collision::to_cell_coord(geometry_, pos);
}
auto CollisionUniformGrid::to_min_cell_coord(ioj::sim::Vector3f const pos) const
    -> ioj::sim::collision::CellCoord {
    return to_cell_coord(pos);
}
auto CollisionUniformGrid::to_max_cell_coord(ioj::sim::Vector3f const pos) const
    -> ioj::sim::collision::CellCoord {
    return ioj::sim::collision::to_max_cell_coord(geometry_, pos);
}
auto CollisionUniformGrid::to_cell_coord_bounds(ioj::sim::Vector3f const min_point,
                                                ioj::sim::Vector3f const max_point) const
    -> CellCoordBounds {
    auto const bounds{ioj::sim::collision::to_cell_coord_bounds(geometry_, min_point, max_point)};
    return {bounds.min, bounds.max};
}
auto CollisionUniformGrid::to_cell_min_x(std::int32_t const x) const -> float {
    return ioj::sim::collision::to_cell_min(x, geometry_.cell_dimensions.X, geometry_.dimensions.x);
}
auto CollisionUniformGrid::to_cell_min_y(std::int32_t const y) const -> float {
    return ioj::sim::collision::to_cell_min(y, geometry_.cell_dimensions.Y, geometry_.dimensions.y);
}
auto CollisionUniformGrid::to_cell_min_z(std::int32_t const z) const -> float {
    return ioj::sim::collision::to_cell_min(z, geometry_.cell_dimensions.Z, geometry_.dimensions.z);
}
auto CollisionUniformGrid::to_cell_min(std::int32_t const x,
                                       std::int32_t const y,
                                       std::int32_t const z) const -> ioj::sim::Vector3f {
    return ml::make_vector3f(to_cell_min_x(x), to_cell_min_y(y), to_cell_min_z(z));
}
auto CollisionUniformGrid::to_cell_min(ioj::sim::collision::CellCoord const coord) const
    -> ioj::sim::Vector3f {
    return ioj::sim::collision::to_cell_min(geometry_, coord);
}
auto CollisionUniformGrid::to_cell_centre_x(std::int32_t const x) const -> float {
    return to_cell_min_x(x) + (geometry_.cell_dimensions.X * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre_y(std::int32_t const y) const -> float {
    return to_cell_min_y(y) + (geometry_.cell_dimensions.Y * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre_z(std::int32_t const z) const -> float {
    return to_cell_min_z(z) + (geometry_.cell_dimensions.Z * 0.5f);
}
auto CollisionUniformGrid::to_cell_centre(std::int32_t const x,
                                          std::int32_t const y,
                                          std::int32_t const z) const -> ioj::sim::Vector3f {
    return ml::make_vector3f(to_cell_centre_x(x), to_cell_centre_y(y), to_cell_centre_z(z));
}
auto CollisionUniformGrid::to_cell_centre(ioj::sim::collision::CellCoord const coord) const
    -> ioj::sim::Vector3f {
    return ioj::sim::collision::to_cell_centre(geometry_, coord);
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(ioj::sim::collision::CellCoord const coord) const
    -> bool {
    return ioj::sim::collision::is_cell_coord_in_bounds(geometry_, coord);
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(
    ioj::sim::collision::CellCoord const min_coord,
    ioj::sim::collision::CellCoord const max_coord) const -> bool {
    return is_cell_coord_in_bounds(min_coord) && is_cell_coord_in_bounds(max_coord);
}
void CollisionUniformGrid::are_spheres_in_bounds(ioj::sim::Vectors3fConstView const centres,
                                                 float const radius,
                                                 std::span<std::uint8_t> const out_results) const {
    [[maybe_unused]] auto const count{centres.num()};
    assert(out_results.size() == static_cast<std::size_t>(count));
    assert(std::isfinite(radius));
    assert(radius >= 0.f);

    ioj::sim::collision::are_spheres_in_bounds(
        geometry_,
        centres,
        radius,
        {out_results.data(), static_cast<std::size_t>(out_results.size())});
}
auto CollisionUniformGrid::to_string(ioj::sim::collision::CellCoord const value) -> std::string {
    return std::format("({}, {}, {})", value.x, value.y, value.z);
}
auto CollisionUniformGrid::to_index(std::int32_t const x,
                                    std::int32_t const y,
                                    std::int32_t const z) const -> std::int32_t {
    return ioj::sim::collision::to_index(geometry_, {x, y, z});
}
auto CollisionUniformGrid::to_index(ioj::sim::collision::CellCoord const coord) const
    -> std::int32_t {
    return to_index(coord.x, coord.y, coord.z);
}
auto CollisionUniformGrid::to_index(ioj::sim::Vector3f const pos) const -> std::int32_t {
    return to_index(to_cell_coord(pos));
}
}
