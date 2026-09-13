#include "sandbox/simulation/simulation/collision_uniform_grid.h"

#include <sandbox/simulation/collision_grid.h>
#include <sandbox/simulation/collision_grid_overlap_query.h>
#include <sandbox/simulation/entities/NativeEntityRegistryView.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/entity_cell_data_operations.h>
#include <sandbox/simulation/simulation/TraceHits.h>

#include <cassert>
#include <cmath>
#include <format>
#include <sandbox/core/diagnostics.h>
#include <sandbox/simulation/rotator_math.h>
#include <utility>

namespace ml::ioj {
namespace {
static_assert(simulation::collision::EntityAABBs::space_ship_index ==
              std::to_underlying(simulation::EntityType::PlayerShip));
static_assert(simulation::collision::EntityAABBs::static_turret_index ==
              std::to_underlying(simulation::EntityType::Turret));
static_assert(simulation::collision::EntityAABBs::capital_ship_index ==
              std::to_underlying(simulation::EntityType::CapitalShip));
static_assert(simulation::collision::EntityAABBs::fighter_index ==
              std::to_underlying(simulation::EntityType::CapitalShipFighter));
static_assert(simulation::collision::EntityAABBs::tube_spinner_index ==
              std::to_underlying(simulation::EntityType::TubeSpinner));
static_assert(simulation::collision::EntityAABBs::num_rows ==
              std::to_underlying(simulation::EntityType::COUNT));

}

auto CollisionUniformGrid::get_grid_dims() const noexcept -> simulation::collision::CellCoord {
    return geometry_.dimensions;
}
void
    CollisionUniformGrid::set_grid_dims(simulation::collision::CellCoord const grid_dims) noexcept {
    geometry_.dimensions = grid_dims;
}

auto CollisionUniformGrid::get_cell_dims() const noexcept -> simulation::Vector3f {
    return geometry_.cell_dimensions;
}
void CollisionUniformGrid::set_cell_dims(simulation::Vector3f const cell_dims) noexcept {
    geometry_.cell_dimensions = cell_dims;
}

CollisionUniformGrid::CollisionUniformGrid(FTestEntityRegistry const& entity_registry) noexcept
    : entity_registry_{entity_registry} {}

auto CollisionUniformGrid::is_configured() const noexcept -> bool {
    return simulation::collision::is_configured(geometry_);
}

auto CollisionUniformGrid::num_cells() const -> std::int32_t {
    return simulation::collision::num_cells(geometry_);
}
auto
    CollisionUniformGrid::get_cell_entities(simulation::collision::CellCoord const cell_coord) const
    -> std::span<FRegistryEntityHandle const> {
    assert(is_cell_coord_in_bounds(cell_coord));

    auto const cell_index{to_index(cell_coord)};
    return entity_storage_.entities_for_cell(cell_index);
}

void CollisionUniformGrid::reset() {
    geometry_ = {};
    entity_storage_.reset();
    static_storage_.reset();
}

void CollisionUniformGrid::set_static_aabbs(simulation::collision::WorldAABBs static_aabbs) {

    if (!is_configured()) {
        ml::fatal_error("Cannot build static geometry for an unconfigured grid");
    }

    static_aabbs.get_const_view().columns().validate_array_sizes();
    static_storage_.set_aabbs(std::move(static_aabbs));
    rebuild_static_grid();
}

auto CollisionUniformGrid::add_static_aabb(simulation::Vector3f const min_point,
                                           simulation::Vector3f const max_point) -> std::int32_t {

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

void CollisionUniformGrid::rebuild_grid(simulation::collision::EntityAABBs const& entity_aabbs) {
    telemetry_.record_rebuild();
    if (!is_configured()) {
        ml::fatal_error("Cannot rebuild an unconfigured collision grid");
    }

    auto const& entity_data{entity_registry_.get_entity_data()};
    auto const entity_count{entity_registry_.get_num_elements()};
    auto const generations{entity_registry_.get_generations()};
    auto const geometry{geometry_};
    entity_storage_.begin_rebuild(geometry.dimensions);

    for (std::int32_t index{}; index < entity_count; ++index) {
        if (entity_data.alive[index] == 0) {
            continue;
        }
        auto const entity_type{entity_data.entity_types[index]};
        auto const bounds{simulation::collision::make_entity_world_bounds(
            entity_aabbs,
            std::to_underlying(entity_type),
            entity_data.locations[index],
            simulation::to_quaternion(entity_data.rotations[index]))};
        auto const [min_coord, max_coord]{
            simulation::collision::to_cell_coord_bounds(geometry, bounds.min, bounds.max)};
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
    if (!entity_storage_.finish_rebuild()) {
        ml::fatal_error("Collision grid entity membership index is inconsistent");
    }
}

void CollisionUniformGrid::append_overlaps(
    simulation::collision::WorldAABB const& query_bounds,
    FRegistryEntityHandle const ignored_entity,
    std::vector<FRegistryEntityHandle>& out_entities,
    std::vector<std::int32_t>& out_static_geometry_indices) const {

    auto const geometry{geometry_};
    [[maybe_unused]] auto const [min_coord, max_coord]{
        simulation::collision::to_cell_coord_bounds(geometry, query_bounds.min, query_bounds.max)};
    assert(is_cell_coord_in_bounds(min_coord, max_coord));

    simulation::collision::append_grid_overlaps(geometry_,
                                                entity_storage_,
                                                static_storage_,
                                                ml::make_native_query_view(entity_registry_),
                                                query_bounds,
                                                ignored_entity,
                                                out_entities,
                                                out_static_geometry_indices);
}

void CollisionUniformGrid::trace_aabbs(simulation::LineTracesConstView const& traces,
                                       FTraceHitsView const& hits) const {
    telemetry_.record_line_traces(static_cast<std::uint64_t>(traces.num()));
    simulation::collision::trace_grid_lines(geometry_,
                                            entity_storage_,
                                            static_storage_,
                                            ml::make_native_query_view(entity_registry_),
                                            traces,
                                            hits);
}

void CollisionUniformGrid::trace_aabbs(
    simulation::LineTracesConstView const& traces,
    FTraceHitsView const& hits,
    std::span<FRegistryEntityHandle const> const ignored_entities) const {
    telemetry_.record_line_traces(static_cast<std::uint64_t>(traces.num()));
    simulation::collision::trace_grid_lines_ignoring_entities(
        geometry_,
        entity_storage_,
        static_storage_,
        ml::make_native_query_view(entity_registry_),
        traces,
        hits,
        {ignored_entities.data(), static_cast<std::size_t>(ignored_entities.size())});
}

void
    CollisionUniformGrid::sweep_aabbs(simulation::LineTracesConstView const& centre_paths,
                                      simulation::Vector3f const moving_half_extent,
                                      FTraceHitsView const& hits,
                                      std::span<FRegistryEntityHandle const> const ignored_entities,
                                      ETraceEntityFilter const entity_filter) const {
    telemetry_.record_sweep_traces(static_cast<std::uint64_t>(centre_paths.num()));
    assert(std::isfinite(moving_half_extent.X) && std::isfinite(moving_half_extent.Y) &&
           std::isfinite(moving_half_extent.Z));
    assert(moving_half_extent.X >= 0.f);
    assert(moving_half_extent.Y >= 0.f);
    assert(moving_half_extent.Z >= 0.f);
    simulation::collision::sweep_grid_aabbs(
        geometry_,
        entity_storage_,
        static_storage_,
        ml::make_native_query_view(entity_registry_),
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
    -> FCollisionGridTelemetrySnapshot {
    return telemetry_.snapshot();
}

auto CollisionUniformGrid::to_cell_x(float const value) const -> std::int32_t {
    return simulation::collision::to_cell_coord(
        value, geometry_.cell_dimensions.X, geometry_.dimensions.x);
}
auto CollisionUniformGrid::to_cell_y(float const value) const -> std::int32_t {
    return simulation::collision::to_cell_coord(
        value, geometry_.cell_dimensions.Y, geometry_.dimensions.y);
}
auto CollisionUniformGrid::to_cell_z(float const value) const -> std::int32_t {
    return simulation::collision::to_cell_coord(
        value, geometry_.cell_dimensions.Z, geometry_.dimensions.z);
}
auto CollisionUniformGrid::to_cell_coord(simulation::Vector3f const pos) const
    -> simulation::collision::CellCoord {
    return simulation::collision::to_cell_coord(geometry_, pos);
}
auto CollisionUniformGrid::to_min_cell_coord(simulation::Vector3f const pos) const
    -> simulation::collision::CellCoord {
    return to_cell_coord(pos);
}
auto CollisionUniformGrid::to_max_cell_coord(simulation::Vector3f const pos) const
    -> simulation::collision::CellCoord {
    return simulation::collision::to_max_cell_coord(geometry_, pos);
}
auto CollisionUniformGrid::to_cell_coord_bounds(simulation::Vector3f const min_point,
                                                simulation::Vector3f const max_point) const
    -> FCellCoordBounds {
    auto const bounds{simulation::collision::to_cell_coord_bounds(geometry_, min_point, max_point)};
    return {bounds.min, bounds.max};
}
auto CollisionUniformGrid::to_cell_min_x(std::int32_t const x) const -> float {
    return simulation::collision::to_cell_min(
        x, geometry_.cell_dimensions.X, geometry_.dimensions.x);
}
auto CollisionUniformGrid::to_cell_min_y(std::int32_t const y) const -> float {
    return simulation::collision::to_cell_min(
        y, geometry_.cell_dimensions.Y, geometry_.dimensions.y);
}
auto CollisionUniformGrid::to_cell_min_z(std::int32_t const z) const -> float {
    return simulation::collision::to_cell_min(
        z, geometry_.cell_dimensions.Z, geometry_.dimensions.z);
}
auto CollisionUniformGrid::to_cell_min(std::int32_t const x,
                                       std::int32_t const y,
                                       std::int32_t const z) const -> simulation::Vector3f {
    return ml::make_vector3f(to_cell_min_x(x), to_cell_min_y(y), to_cell_min_z(z));
}
auto CollisionUniformGrid::to_cell_min(simulation::collision::CellCoord const coord) const
    -> simulation::Vector3f {
    return simulation::collision::to_cell_min(geometry_, coord);
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
                                          std::int32_t const z) const -> simulation::Vector3f {
    return ml::make_vector3f(to_cell_centre_x(x), to_cell_centre_y(y), to_cell_centre_z(z));
}
auto CollisionUniformGrid::to_cell_centre(simulation::collision::CellCoord const coord) const
    -> simulation::Vector3f {
    return simulation::collision::to_cell_centre(geometry_, coord);
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(
    simulation::collision::CellCoord const coord) const -> bool {
    return simulation::collision::is_cell_coord_in_bounds(geometry_, coord);
}
auto CollisionUniformGrid::is_cell_coord_in_bounds(
    simulation::collision::CellCoord const min_coord,
    simulation::collision::CellCoord const max_coord) const -> bool {
    return is_cell_coord_in_bounds(min_coord) && is_cell_coord_in_bounds(max_coord);
}
void CollisionUniformGrid::are_spheres_in_bounds(simulation::Vectors3fConstView const centres,
                                                 float const radius,
                                                 std::span<std::uint8_t> const out_results) const {
    [[maybe_unused]] auto const count{centres.num()};
    assert(out_results.size() == static_cast<std::size_t>(count));
    assert(std::isfinite(radius));
    assert(radius >= 0.f);

    simulation::collision::are_spheres_in_bounds(
        geometry_,
        centres,
        radius,
        {out_results.data(), static_cast<std::size_t>(out_results.size())});
}
auto CollisionUniformGrid::to_string(simulation::collision::CellCoord const value) -> std::string {
    return std::format("({}, {}, {})", value.x, value.y, value.z);
}
auto CollisionUniformGrid::to_index(std::int32_t const x,
                                    std::int32_t const y,
                                    std::int32_t const z) const -> std::int32_t {
    return simulation::collision::to_index(geometry_, {x, y, z});
}
auto CollisionUniformGrid::to_index(simulation::collision::CellCoord const coord) const
    -> std::int32_t {
    return to_index(coord.x, coord.y, coord.z);
}
auto CollisionUniformGrid::to_index(simulation::Vector3f const pos) const -> std::int32_t {
    return to_index(to_cell_coord(pos));
}
}
