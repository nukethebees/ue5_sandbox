#include "ioj/sim/spatial_query_manager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <ioj/sim/entity_world_bounds.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>
#include <sandbox/core/diagnostics.h>
#include <thread>

#include <ioj/sim/entity_registry.h>
#include <ioj/sim/entity_registry_view.h>

#include <cstddef>
#include <utility>

namespace {
enum class QueryMode : std::uint8_t {
    HitEntity,
    ClearLine,
    TargetLineOfSight,
    ClosestHit,
};

struct TraceRequest {
    ioj::sim::Vectors3fConstView start_locations{};
    ioj::sim::Vectors3fConstView end_locations{};
    ioj::sim::Vector3f scalar_start{};
    ioj::sim::Vector3f scalar_end{};
    std::span<ioj::sim::RegistryEntityHandle const> targets{};
    std::span<ioj::sim::RegistryEntityHandle const> ignored_entities{};
    std::span<ioj::sim::RegistryEntityHandle> out_entity_handles{};
    std::span<std::uint8_t> out_flags{};
};

template <QueryMode Mode>
auto trace_impl(ioj::sim::SpatialQueryManager const& manager, TraceRequest const& request)
    -> ioj::sim::LineTraceResult {
    auto const count{[&] {
        if constexpr (Mode == QueryMode::ClosestHit) {
            return 1;
        } else {
            return request.end_locations.num();
        }
    }()};

    if constexpr (Mode == QueryMode::HitEntity) {
        assert(count == request.start_locations.num());
        assert(static_cast<std::size_t>(count) == request.out_entity_handles.size());
        std::ranges::fill(request.out_entity_handles, ioj::sim::RegistryEntityHandle{});
    } else if constexpr (Mode == QueryMode::ClearLine) {
        assert(count == request.start_locations.num());
        assert(static_cast<std::size_t>(count) == request.out_flags.size());
        assert(request.ignored_entities.empty() ||
               request.ignored_entities.size() == static_cast<std::size_t>(count));
        std::ranges::fill(request.out_flags, std::uint8_t{0});
    } else if constexpr (Mode == QueryMode::TargetLineOfSight) {
        assert(static_cast<std::size_t>(count) == request.targets.size());
        assert(static_cast<std::size_t>(count) == request.out_flags.size());
        std::ranges::fill(request.out_flags, std::uint8_t{0});
    } else {
        assert(request.ignored_entities.size() == 1);
    }

    if (count == 0) {
        return {};
    }

    ioj::sim::query_manager::ThreadBufferLease const buffer_lease{manager};
    auto& buffers{buffer_lease.get()};
    auto& traces{buffers.line_traces};
    auto& hits{buffers.trace_hits};
    hits.set_num(count);

    auto const trace_view{[&] {
        if constexpr (Mode == QueryMode::TargetLineOfSight || Mode == QueryMode::ClosestHit) {
            traces.set_num(count);
            for (std::int32_t i{}; i < count; ++i) {
                if constexpr (Mode == QueryMode::TargetLineOfSight) {
                    traces.set(i, request.scalar_start, request.end_locations[i]);
                } else {
                    traces.set(i, request.scalar_start, request.scalar_end);
                }
            }
            return traces.get_const_view();
        } else {
            return ioj::sim::LineTracesConstView{request.start_locations, request.end_locations};
        }
    }()};

    auto const& uniform_grid{manager.get_collision_system().get_uniform_grid()};
    if constexpr (Mode == QueryMode::ClosestHit) {
        uniform_grid.trace_aabbs(trace_view, hits.get_view(), request.ignored_entities);
    } else if constexpr (Mode == QueryMode::ClearLine) {
        if (request.ignored_entities.empty()) {
            uniform_grid.trace_aabbs(trace_view, hits.get_view());
        } else {
            uniform_grid.trace_aabbs(trace_view, hits.get_view(), request.ignored_entities);
        }
    } else {
        uniform_grid.trace_aabbs(trace_view, hits.get_view());
    }

    if constexpr (Mode == QueryMode::ClosestHit) {
        return {
            .location = hits.locations[0],
            .entity = hits.entities[0],
            .static_geometry_index = hits.static_geometry_indices[0],
            .hit = hits.hits[0] != 0,
        };
    } else {
        if constexpr (Mode == QueryMode::HitEntity) {
            for (std::int32_t i{}; i < count; ++i) {
                request.out_entity_handles[i] = hits.entities[i];
            }
        } else if constexpr (Mode == QueryMode::ClearLine) {
            for (std::int32_t i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<std::uint8_t>(hits.hits[i] == 0);
            }
        } else if constexpr (Mode == QueryMode::TargetLineOfSight) {
            for (std::int32_t i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<std::uint8_t>(
                    hits.hits[i] == 0 || hits.entities[i] == request.targets[i]);
            }
        }

        return {};
    }
}

void validate_grid_for_range_query(ioj::sim::collision::CollisionUniformGrid const& grid,
                                   ioj::sim::Vector3f const& origin,
                                   float const radius) {
    if (!grid.is_configured()) {
        ml::fatal_error(std::format(
            "Cannot query unconfigured grid: origin=({}, {}, {}), radius={}, dimensions={}",
            origin.X,
            origin.Y,
            origin.Z,
            radius,
            ioj::sim::collision::CollisionUniformGrid::to_string(grid.get_grid_dims())));
    }
}
}

namespace ioj::sim::query_manager {
/* **************************************** */
// Thread buffer lease
/* **************************************** */
ThreadBufferLease::ThreadBufferLease(SpatialQueryManager const& in_manager)
    : manager{in_manager}
    , index{manager.acquire_thread_buffer()} {}

ThreadBufferLease::~ThreadBufferLease() {
    manager.release_thread_buffer(index);
}

auto ThreadBufferLease::get() const -> ThreadBuffers& {
    return manager.thread_buffer_pool_.get(index);
}
}

namespace ioj::sim {
/* **************************************** */
// Thread buffer management
/* **************************************** */
void SpatialQueryManager::reserve_thread_buffers(std::int32_t const count) {
    auto const maximum_thread_buffer_count{std::max(1u, std::thread::hardware_concurrency()) * 2u};
    if (count <= 0 || static_cast<unsigned>(count) > maximum_thread_buffer_count) {
        ml::fatal_error(std::format(
            "Invalid thread buffer count {} (maximum {})", count, maximum_thread_buffer_count));
    }
    auto const result{thread_buffer_pool_.reserve(count)};
    if (result == QueryThreadBufferReserveResult::invalid_count ||
        result == QueryThreadBufferReserveResult::active_queries) {
        ml::fatal_error("Cannot reserve spatial query buffers while queries are active");
    }
}

auto SpatialQueryManager::acquire_thread_buffer() const -> std::int32_t {
    auto const index{thread_buffer_pool_.try_acquire()};
    if (!index.has_value()) {
        ml::fatal_error("Spatial query thread buffer pool exhausted; reserve buffers before "
                        "starting concurrent queries");
    }

    return *index;
}

void SpatialQueryManager::release_thread_buffer(std::int32_t const index) const {
    if (!thread_buffer_pool_.release(index)) {
        ml::fatal_error("Invalid spatial query thread buffer release");
    }
}

/* **************************************** */
// Construction and setup
/* **************************************** */
SpatialQueryManager::SpatialQueryManager(EntityRegistry const& in_entity_registry)
    : entity_registry{in_entity_registry}
    , collision{in_entity_registry} {}

void SpatialQueryManager::initialise(collision::CellCoord const grid_dimensions,
                                     Vector3f const cell_size,
                                     collision::EntityAABBs const& entity_bounds) {
    reserve_thread_buffers(1);

    auto& uniform_grid{collision.get_uniform_grid()};
    uniform_grid.set_grid_dims(grid_dimensions);
    uniform_grid.set_cell_dims(cell_size);

    collision.initialise(entity_bounds);

    auto const radius_count{static_cast<std::int32_t>(entity_radii_.size())};
    for (std::int32_t type_index{}; type_index < radius_count; ++type_index) {
        entity_radii_[static_cast<std::size_t>(type_index)] =
            collision::get_entity_radius(entity_bounds, type_index);
    }
}

/* **************************************** */
// Batched line queries
/* **************************************** */
void SpatialQueryManager::trace_line_of_sight(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    std::span<RegistryEntityHandle> const out_entity_handles) const {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::trace_line_of_sight");

    trace_impl<QueryMode::HitEntity>(*this,
                                     {.start_locations = start_locations,
                                      .end_locations = end_locations,
                                      .out_entity_handles = out_entity_handles});
}

void SpatialQueryManager::has_line_of_sight_to_targets(
    Vector3f const& start_location,
    Vectors3fConstView const end_locations,
    std::span<RegistryEntityHandle const> const targets,
    std::span<std::uint8_t> const has_los) const {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::has_line_of_sight_to_targets");

    trace_impl<QueryMode::TargetLineOfSight>(*this,
                                             {.end_locations = end_locations,
                                              .scalar_start = start_location,
                                              .targets = targets,
                                              .out_flags = has_los});
}

void SpatialQueryManager::have_clear_lines(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    std::span<std::uint8_t> const clear_lines,
    std::span<RegistryEntityHandle const> const ignored_entities) const {
    trace_impl<QueryMode::ClearLine>(*this,
                                     {.start_locations = start_locations,
                                      .end_locations = end_locations,
                                      .ignored_entities = ignored_entities,
                                      .out_flags = clear_lines});
}

void SpatialQueryManager::trace_closest_lines(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    TraceHitsView const out_hits,
    std::span<RegistryEntityHandle const> const ignored_entities) const {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::trace_closest_lines");

    [[maybe_unused]] auto const count{start_locations.num()};
    assert(end_locations.num() == count);
    assert(out_hits.num() == count);
    assert(ignored_entities.empty() || ignored_entities.size() == static_cast<std::size_t>(count));

    auto const traces{LineTracesConstView{start_locations, end_locations}};
    if (ignored_entities.empty()) {
        collision.get_uniform_grid().trace_aabbs(traces, out_hits);
    } else {
        collision.get_uniform_grid().trace_aabbs(traces, out_hits, ignored_entities);
    }
}

void SpatialQueryManager::sweep_closest_aabbs(
    Vectors3fConstView const start_locations,
    Vectors3fConstView const end_locations,
    Vector3f const moving_half_extent,
    TraceHitsView const out_hits,
    std::span<RegistryEntityHandle const> const ignored_entities,
    collision::TraceEntityFilter const entity_filter) const {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::sweep_closest_aabbs");

    [[maybe_unused]] auto const count{start_locations.num()};
    assert(end_locations.num() == count);
    assert(out_hits.num() == count);
    assert(ignored_entities.empty() || ignored_entities.size() == static_cast<std::size_t>(count));

    collision.get_uniform_grid().sweep_aabbs(LineTracesConstView{start_locations, end_locations},
                                             moving_half_extent,
                                             out_hits,
                                             ignored_entities,
                                             entity_filter);
}

/* **************************************** */
// Scalar and entity queries
/* **************************************** */
auto SpatialQueryManager::has_clear_line(Vector3f const start_location,
                                         Vector3f const end_location,
                                         RegistryEntityHandle const ignored_entity) const -> bool {
    return !trace_closest(start_location, end_location, ignored_entity).hit;
}

auto SpatialQueryManager::trace_closest(Vector3f const start_location,
                                        Vector3f const end_location,
                                        RegistryEntityHandle const ignored_entity) const
    -> LineTraceResult {
    std::array<RegistryEntityHandle, 1> ignored_entities{ignored_entity};
    return trace_impl<QueryMode::ClosestHit>(*this,
                                             {.scalar_start = start_location,
                                              .scalar_end = end_location,
                                              .ignored_entities = ignored_entities});
}

auto SpatialQueryManager::collect_non_team_entities_in_range(
    Vector3f const& origin,
    Team const team,
    float const radius,
    std::span<RegistryEntityHandle> const out_entities) const -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::collect_non_team_entities_in_range");

    telemetry_.record_range_query();

    if (out_entities.empty()) {
        return 0;
    }

    auto const& grid{collision.get_uniform_grid()};
    validate_grid_for_range_query(grid, origin, radius);
    query_manager::ThreadBufferLease const buffer_lease{*this};
    {
        SANDBOX_PROFILE_SCOPE(
            "Sandbox::SpatialQueryManager::collect_non_team_entities_in_range::loop");
        return ioj::sim::collect_non_team_entities_in_range(
            grid.get_native_geometry(),
            grid.get_native_entity_storage(),
            make_native_query_view(entity_registry),
            buffer_lease.get(),
            origin,
            radius,
            team,
            {out_entities.data(), static_cast<std::size_t>(out_entities.size())});
    }
}

auto SpatialQueryManager::collect_entities_of_type_in_range(
    Vector3f const& origin,
    EntityType const entity_type,
    float const radius,
    RegistryEntityHandle const ignored_entity,
    std::span<RegistryEntityHandle> const out_entities) const -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::collect_entities_of_type_in_range");

    telemetry_.record_range_query();

    if (out_entities.empty()) {
        return 0;
    }

    auto const& grid{collision.get_uniform_grid()};
    validate_grid_for_range_query(grid, origin, radius);
    query_manager::ThreadBufferLease const buffer_lease{*this};
    return ioj::sim::collect_entities_of_type_in_range(
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

auto SpatialQueryManager::get_any_non_team_entity(Team const team) const -> RegistryEntityHandle {
    return find_any_non_team_entity(make_native_query_view(entity_registry), team);
}

auto SpatialQueryManager::get_any_non_team_entity(Team const team,
                                                  EntityType const entity_type) const
    -> RegistryEntityHandle {
    return find_any_non_team_entity(make_native_query_view(entity_registry), team, entity_type);
}

void SpatialQueryManager::are_spheres_in_bounds(Vectors3fConstView const centres,
                                                float const radius,
                                                std::span<std::uint8_t> const out_results) const {
    collision.get_uniform_grid().are_spheres_in_bounds(centres, radius, out_results);
}

auto SpatialQueryManager::get_entity_type_radius(EntityType const entity_type) const noexcept
    -> float {
    auto const index{static_cast<std::size_t>(entity_type)};
    assert(index < entity_radii_.size());
    return entity_radii_[index];
}

auto SpatialQueryManager::get_entity_type_radii() const noexcept -> std::span<float const> {
    return entity_radii_;
}

void SpatialQueryManager::copy_entity_radii(std::span<RegistryEntityHandle const> const handles,
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
auto SpatialQueryManager::update(SimTick const tick) -> collision::DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("Sandbox::SpatialQueryManager::update");

    return collision.update(entity_registry.get_moved_entities_this_tick(), tick);
}

void SpatialQueryManager::reset_runtime_telemetry() noexcept {
    telemetry_.reset();
    collision.get_uniform_grid().reset_runtime_telemetry();
}

auto SpatialQueryManager::get_runtime_telemetry() const noexcept -> SpatialQueryTelemetrySnapshot {
    auto const& grid{collision.get_uniform_grid()};
    auto const grid_telemetry{grid.get_runtime_telemetry()};
    return telemetry_.snapshot(grid_telemetry, grid.get_non_empty_cell_count());
}
}
