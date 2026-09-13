#include "sandbox/simulation/simulation/SpatialQueryManager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>
#include <sandbox/simulation/entity_world_bounds.h>
#include <sandbox/simulation/rotator_math.h>
#include <thread>

#include <sandbox/simulation/entities/NativeEntityRegistryView.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>

#include <cstddef>
#include <utility>

namespace {
enum class EQueryMode : std::uint8_t {
    HitEntity,
    ClearLine,
    TargetLineOfSight,
    ClosestHit,
};

struct FTraceRequest {
    ml::simulation::Vectors3fConstView start_locations{};
    ml::simulation::Vectors3fConstView end_locations{};
    ml::simulation::Vector3f scalar_start{};
    ml::simulation::Vector3f scalar_end{};
    std::span<FRegistryEntityHandle const> targets{};
    std::span<FRegistryEntityHandle const> ignored_entities{};
    std::span<FRegistryEntityHandle> out_entity_handles{};
    std::span<std::uint8_t> out_flags{};
};

template <EQueryMode Mode>
auto trace_impl(ml::FSpatialQueryManager const& manager, FTraceRequest const& request)
    -> ml::FLineTraceResult {
    auto const count{[](FTraceRequest const& trace_request) {
        if constexpr (Mode == EQueryMode::ClosestHit) {
            return 1;
        } else {
            return trace_request.end_locations.num();
        }
    }(request)};

    if constexpr (Mode == EQueryMode::HitEntity) {
        assert(count == request.start_locations.num());
        assert(static_cast<std::size_t>(count) == request.out_entity_handles.size());
        std::ranges::fill(request.out_entity_handles, FRegistryEntityHandle{});
    } else if constexpr (Mode == EQueryMode::ClearLine) {
        assert(count == request.start_locations.num());
        assert(static_cast<std::size_t>(count) == request.out_flags.size());
        assert(request.ignored_entities.empty() ||
               request.ignored_entities.size() == static_cast<std::size_t>(count));
        std::ranges::fill(request.out_flags, std::uint8_t{0});
    } else if constexpr (Mode == EQueryMode::TargetLineOfSight) {
        assert(static_cast<std::size_t>(count) == request.targets.size());
        assert(static_cast<std::size_t>(count) == request.out_flags.size());
        std::ranges::fill(request.out_flags, std::uint8_t{0});
    } else {
        assert(request.ignored_entities.size() == 1);
    }

    if (count == 0) {
        return {};
    }

    ml::query_manager::FThreadBufferLease const buffer_lease{manager};
    auto& buffers{buffer_lease.get()};
    auto& traces{buffers.line_traces};
    auto& hits{buffers.trace_hits};
    hits.set_num(count);

    auto const trace_view{[&] {
        if constexpr (Mode == EQueryMode::TargetLineOfSight || Mode == EQueryMode::ClosestHit) {
            traces.set_num(count);
            for (std::int32_t i{}; i < count; ++i) {
                if constexpr (Mode == EQueryMode::TargetLineOfSight) {
                    traces.set(i, request.scalar_start, request.end_locations[i]);
                } else {
                    traces.set(i, request.scalar_start, request.scalar_end);
                }
            }
            return traces.get_const_view();
        } else {
            return ml::simulation::LineTracesConstView{request.start_locations,
                                                       request.end_locations};
        }
    }()};

    auto const& uniform_grid{manager.get_collision_system().get_uniform_grid()};
    if constexpr (Mode == EQueryMode::ClosestHit) {
        uniform_grid.trace_aabbs(trace_view, hits.get_view(), request.ignored_entities);
    } else if constexpr (Mode == EQueryMode::ClearLine) {
        if (request.ignored_entities.empty()) {
            uniform_grid.trace_aabbs(trace_view, hits.get_view());
        } else {
            uniform_grid.trace_aabbs(trace_view, hits.get_view(), request.ignored_entities);
        }
    } else {
        uniform_grid.trace_aabbs(trace_view, hits.get_view());
    }

    if constexpr (Mode == EQueryMode::ClosestHit) {
        return {
            .location = hits.locations[0],
            .entity = hits.entities[0],
            .static_geometry_index = hits.static_geometry_indices[0],
            .hit = hits.hits[0] != 0,
        };
    } else {
        if constexpr (Mode == EQueryMode::HitEntity) {
            for (std::int32_t i{}; i < count; ++i) {
                request.out_entity_handles[i] = hits.entities[i];
            }
        } else if constexpr (Mode == EQueryMode::ClearLine) {
            for (std::int32_t i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<std::uint8_t>(hits.hits[i] == 0);
            }
        } else if constexpr (Mode == EQueryMode::TargetLineOfSight) {
            for (std::int32_t i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<std::uint8_t>(
                    hits.hits[i] == 0 || hits.entities[i] == request.targets[i]);
            }
        }

        return {};
    }
}

void validate_grid_for_range_query(ml::ioj::CollisionUniformGrid const& grid,
                                   ml::simulation::Vector3f const& origin,
                                   float const radius) {
    if (!grid.is_configured()) {
        ml::fatal_error(std::format(
            "Cannot query unconfigured grid: origin=({}, {}, {}), radius={}, dimensions={}",
            origin.X,
            origin.Y,
            origin.Z,
            radius,
            ml::ioj::CollisionUniformGrid::to_string(grid.get_grid_dims())));
    }
}
}

namespace ml::query_manager {
/* **************************************** */
// Thread buffer lease
/* **************************************** */
FThreadBufferLease::FThreadBufferLease(FSpatialQueryManager const& in_manager)
    : manager{in_manager}
    , index{manager.acquire_thread_buffer()} {}

FThreadBufferLease::~FThreadBufferLease() {
    manager.release_thread_buffer(index);
}

auto FThreadBufferLease::get() const -> FThreadBuffers& {
    return manager.thread_buffer_pool_.get(index);
}
}

namespace ml {
/* **************************************** */
// Thread buffer management
/* **************************************** */
void FSpatialQueryManager::reserve_thread_buffers(std::int32_t const count) {
    auto const maximum_thread_buffer_count{std::max(1u, std::thread::hardware_concurrency()) * 2u};
    if (count <= 0 || static_cast<unsigned>(count) > maximum_thread_buffer_count) {
        ml::fatal_error(std::format(
            "Invalid thread buffer count {} (maximum {})", count, maximum_thread_buffer_count));
    }
    auto const result{thread_buffer_pool_.reserve(count)};
    if (result == simulation::QueryThreadBufferReserveResult::invalid_count ||
        result == simulation::QueryThreadBufferReserveResult::active_queries) {
        ml::fatal_error("Cannot reserve spatial query buffers while queries are active");
    }
}

auto FSpatialQueryManager::acquire_thread_buffer() const -> std::int32_t {
    auto const index{thread_buffer_pool_.try_acquire()};
    if (!index.has_value()) {
        ml::fatal_error("Spatial query thread buffer pool exhausted; reserve buffers before "
                        "starting concurrent queries");
    }

    return *index;
}

void FSpatialQueryManager::release_thread_buffer(std::int32_t const index) const {
    if (!thread_buffer_pool_.release(index)) {
        ml::fatal_error("Invalid spatial query thread buffer release");
    }
}

/* **************************************** */
// Construction and setup
/* **************************************** */
FSpatialQueryManager::FSpatialQueryManager(FTestEntityRegistry const& in_entity_registry)
    : entity_registry{in_entity_registry}
    , collision{in_entity_registry} {}

void FSpatialQueryManager::initialise(ml::simulation::collision::CellCoord const grid_dimensions,
                                      ml::simulation::Vector3f const cell_size,
                                      simulation::collision::EntityAABBs const& entity_bounds) {
    reserve_thread_buffers(1);

    auto& uniform_grid{collision.get_uniform_grid()};
    uniform_grid.set_grid_dims(grid_dimensions);
    uniform_grid.set_cell_dims(cell_size);

    collision.initialise(entity_bounds);

    auto const radius_count{static_cast<std::int32_t>(entity_radii_.size())};
    for (std::int32_t type_index{}; type_index < radius_count; ++type_index) {
        entity_radii_[static_cast<std::size_t>(type_index)] =
            simulation::collision::get_entity_radius(entity_bounds, type_index);
    }
}

/* **************************************** */
// Batched line queries
/* **************************************** */
void FSpatialQueryManager::trace_line_of_sight(
    ml::simulation::Vectors3fConstView const start_locations,
    ml::simulation::Vectors3fConstView const end_locations,
    std::span<FRegistryEntityHandle> const out_entity_handles) const {

    trace_impl<EQueryMode::HitEntity>(*this,
                                      {.start_locations = start_locations,
                                       .end_locations = end_locations,
                                       .out_entity_handles = out_entity_handles});
}

void FSpatialQueryManager::has_line_of_sight_to_targets(
    ml::simulation::Vector3f const& start_location,
    ml::simulation::Vectors3fConstView const end_locations,
    std::span<FRegistryEntityHandle const> const targets,
    std::span<std::uint8_t> const has_los) const {

    trace_impl<EQueryMode::TargetLineOfSight>(*this,
                                              {.end_locations = end_locations,
                                               .scalar_start = start_location,
                                               .targets = targets,
                                               .out_flags = has_los});
}

void FSpatialQueryManager::have_clear_lines(
    ml::simulation::Vectors3fConstView const start_locations,
    ml::simulation::Vectors3fConstView const end_locations,
    std::span<std::uint8_t> const clear_lines,
    std::span<FRegistryEntityHandle const> const ignored_entities) const {
    trace_impl<EQueryMode::ClearLine>(*this,
                                      {.start_locations = start_locations,
                                       .end_locations = end_locations,
                                       .ignored_entities = ignored_entities,
                                       .out_flags = clear_lines});
}

void FSpatialQueryManager::trace_closest_lines(
    ml::simulation::Vectors3fConstView const start_locations,
    ml::simulation::Vectors3fConstView const end_locations,
    FTraceHitsView const out_hits,
    std::span<FRegistryEntityHandle const> const ignored_entities) const {

    [[maybe_unused]] auto const count{start_locations.num()};
    assert(end_locations.num() == count);
    assert(out_hits.num() == count);
    assert(ignored_entities.empty() || ignored_entities.size() == static_cast<std::size_t>(count));

    auto const traces{simulation::LineTracesConstView{start_locations, end_locations}};
    if (ignored_entities.empty()) {
        collision.get_uniform_grid().trace_aabbs(traces, out_hits);
    } else {
        collision.get_uniform_grid().trace_aabbs(traces, out_hits, ignored_entities);
    }
}

void FSpatialQueryManager::sweep_closest_aabbs(
    ml::simulation::Vectors3fConstView const start_locations,
    ml::simulation::Vectors3fConstView const end_locations,
    ml::simulation::Vector3f const moving_half_extent,
    FTraceHitsView const out_hits,
    std::span<FRegistryEntityHandle const> const ignored_entities,
    ioj::ETraceEntityFilter const entity_filter) const {

    [[maybe_unused]] auto const count{start_locations.num()};
    assert(end_locations.num() == count);
    assert(out_hits.num() == count);
    assert(ignored_entities.empty() || ignored_entities.size() == static_cast<std::size_t>(count));

    collision.get_uniform_grid().sweep_aabbs(
        simulation::LineTracesConstView{start_locations, end_locations},
        moving_half_extent,
        out_hits,
        ignored_entities,
        entity_filter);
}

/* **************************************** */
// Scalar and entity queries
/* **************************************** */
auto FSpatialQueryManager::has_clear_line(ml::simulation::Vector3f const start_location,
                                          ml::simulation::Vector3f const end_location,
                                          FRegistryEntityHandle const ignored_entity) const
    -> bool {
    return !trace_closest(start_location, end_location, ignored_entity).hit;
}

auto FSpatialQueryManager::trace_closest(ml::simulation::Vector3f const start_location,
                                         ml::simulation::Vector3f const end_location,
                                         FRegistryEntityHandle const ignored_entity) const
    -> FLineTraceResult {
    std::array<FRegistryEntityHandle, 1> ignored_entities{ignored_entity};
    return trace_impl<EQueryMode::ClosestHit>(*this,
                                              {.scalar_start = start_location,
                                               .scalar_end = end_location,
                                               .ignored_entities = ignored_entities});
}

auto FSpatialQueryManager::collect_non_team_entities_in_range(
    ml::simulation::Vector3f const& origin,
    simulation::Team const team,
    float const radius,
    std::span<FRegistryEntityHandle> const out_entities) const -> std::int32_t {

    telemetry_.record_range_query();

    if (out_entities.empty()) {
        return 0;
    }

    auto const& grid{collision.get_uniform_grid()};
    validate_grid_for_range_query(grid, origin, radius);
    query_manager::FThreadBufferLease const buffer_lease{*this};
    return simulation::collect_non_team_entities_in_range(
        grid.get_native_geometry(),
        grid.get_native_entity_storage(),
        make_native_query_view(entity_registry),
        buffer_lease.get(),
        origin,
        radius,
        team,
        {out_entities.data(), static_cast<std::size_t>(out_entities.size())});
}

auto FSpatialQueryManager::collect_entities_of_type_in_range(
    ml::simulation::Vector3f const& origin,
    simulation::EntityType const entity_type,
    float const radius,
    FRegistryEntityHandle const ignored_entity,
    std::span<FRegistryEntityHandle> const out_entities) const -> std::int32_t {

    telemetry_.record_range_query();

    if (out_entities.empty()) {
        return 0;
    }

    auto const& grid{collision.get_uniform_grid()};
    validate_grid_for_range_query(grid, origin, radius);
    query_manager::FThreadBufferLease const buffer_lease{*this};
    return simulation::collect_entities_of_type_in_range(
        grid.get_native_geometry(),
        grid.get_native_entity_storage(),
        make_native_query_view(entity_registry),
        buffer_lease.get(),
        origin,
        radius,
        entity_type,
        ignored_entity,
        {out_entities.data(), static_cast<std::size_t>(out_entities.size())});
}

auto FSpatialQueryManager::get_any_non_team_entity(simulation::Team const team) const
    -> FRegistryEntityHandle {
    return simulation::find_any_non_team_entity(make_native_query_view(entity_registry), team);
}

auto FSpatialQueryManager::get_any_non_team_entity(simulation::Team const team,
                                                   simulation::EntityType const entity_type) const
    -> FRegistryEntityHandle {
    return simulation::find_any_non_team_entity(
        make_native_query_view(entity_registry), team, entity_type);
}

void FSpatialQueryManager::are_spheres_in_bounds(ml::simulation::Vectors3fConstView const centres,
                                                 float const radius,
                                                 std::span<std::uint8_t> const out_results) const {
    collision.get_uniform_grid().are_spheres_in_bounds(centres, radius, out_results);
}

auto FSpatialQueryManager::get_entity_type_radius(
    simulation::EntityType const entity_type) const noexcept -> float {
    auto const index{static_cast<std::size_t>(entity_type)};
    assert(index < entity_radii_.size());
    return entity_radii_[index];
}

auto FSpatialQueryManager::get_entity_type_radii() const noexcept -> std::span<float const> {
    return entity_radii_;
}

void FSpatialQueryManager::copy_entity_radii(std::span<FRegistryEntityHandle const> const handles,
                                             std::span<float> const out_radii) const {
    assert(handles.size() == out_radii.size());

    auto const count{handles.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const handle{handles[index]};
        if (handle.is_null()) {
            out_radii[index] = 0.0f;
            continue;
        }

        assert(entity_registry.is_valid_alive(handle));
        out_radii[index] = get_entity_type_radius(entity_registry.get_entity_type(handle));
    }
}

/* **************************************** */
// Collision state and telemetry
/* **************************************** */
auto FSpatialQueryManager::update(std::uint64_t const tick) -> ioj::FDetectedOverlapsView {

    return collision.update(entity_registry.get_moved_entities_this_tick(), tick);
}

void FSpatialQueryManager::reset_runtime_telemetry() noexcept {
    telemetry_.reset();
    collision.get_uniform_grid().reset_runtime_telemetry();
}

auto FSpatialQueryManager::get_runtime_telemetry() const noexcept
    -> FSpatialQueryTelemetrySnapshot {
    auto const& grid{collision.get_uniform_grid()};
    auto const grid_telemetry{grid.get_runtime_telemetry()};
    return telemetry_.snapshot(grid_telemetry, grid.get_non_empty_cell_count());
}
}
