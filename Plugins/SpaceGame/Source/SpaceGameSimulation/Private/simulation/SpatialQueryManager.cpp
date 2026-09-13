#include "SpaceGameSimulation/simulation/SpatialQueryManager.h"

#include <SpaceGameSimulation/entities/NativeEntityRegistryView.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <HAL/PlatformMisc.h>
#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <cstddef>
#include <utility>

namespace {
enum class EQueryMode : uint8 {
    HitEntity,
    ClearLine,
    TargetLineOfSight,
    ClosestHit,
};

struct FTraceRequest {
    FVectors3f::ConstView start_locations;
    FVectors3f::ConstView end_locations;
    FVector3f scalar_start{FVector3f::ZeroVector};
    FVector3f scalar_end{FVector3f::ZeroVector};
    TConstArrayView<FRegistryEntityHandle> targets;
    TConstArrayView<FRegistryEntityHandle> ignored_entities;
    TArrayView<FRegistryEntityHandle> out_entity_handles;
    TArrayView<uint8> out_flags;
};

template <EQueryMode Mode>
auto trace_impl(ml::FSpatialQueryManager const& manager, FTraceRequest const& request)
    -> ml::FLineTraceResult {
    auto const count{[&request] {
        if constexpr (Mode == EQueryMode::ClosestHit) {
            return 1;
        } else {
            return request.end_locations.num();
        }
    }()};

    if constexpr (Mode == EQueryMode::HitEntity) {
        check(count == request.start_locations.num());
        check(count == request.out_entity_handles.Num());
        ml::fill(request.out_entity_handles, FRegistryEntityHandle{});
    } else if constexpr (Mode == EQueryMode::ClearLine) {
        check(count == request.start_locations.num());
        check(count == request.out_flags.Num());
        check(request.ignored_entities.IsEmpty() || request.ignored_entities.Num() == count);
        ml::fill(request.out_flags, uint8{0});
    } else if constexpr (Mode == EQueryMode::TargetLineOfSight) {
        check(count == request.targets.Num());
        check(count == request.out_flags.Num());
        ml::fill(request.out_flags, uint8{0});
    } else {
        check(request.ignored_entities.Num() == 1);
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
            for (int32 i{}; i < count; ++i) {
                if constexpr (Mode == EQueryMode::TargetLineOfSight) {
                    traces.set(i,
                               ml::to_native(request.scalar_start),
                               ml::to_native(ml::get_vector3f(request.end_locations, i)));
                } else {
                    traces.set(
                        i, ml::to_native(request.scalar_start), ml::to_native(request.scalar_end));
                }
            }
            return traces.get_const_view();
        } else {
            return ml::make_line_traces_const_view(request.start_locations, request.end_locations);
        }
    }()};

    auto const& uniform_grid{manager.get_collision_system().get_uniform_grid()};
    if constexpr (Mode == EQueryMode::ClosestHit) {
        uniform_grid.trace_aabbs(trace_view, hits.get_view(), request.ignored_entities);
    } else if constexpr (Mode == EQueryMode::ClearLine) {
        if (request.ignored_entities.IsEmpty()) {
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
            for (int32 i{}; i < count; ++i) {
                request.out_entity_handles[i] = hits.entities[i];
            }
        } else if constexpr (Mode == EQueryMode::ClearLine) {
            for (int32 i{}; i < count; ++i) {
                request.out_flags[i] = static_cast<uint8>(hits.hits[i] == 0);
            }
        } else if constexpr (Mode == EQueryMode::TargetLineOfSight) {
            for (int32 i{}; i < count; ++i) {
                request.out_flags[i] =
                    static_cast<uint8>(hits.hits[i] == 0 || hits.entities[i] == request.targets[i]);
            }
        }

        return {};
    }
}

void validate_grid_for_range_query(ml::ioj::CollisionUniformGrid const& grid,
                                   FVector3f const& origin,
                                   float const radius) {
    if (!grid.is_configured()) {
        auto const grid_dims{grid.get_grid_dims()};
        auto const cell_dims{grid.get_cell_dims()};
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("Cannot query unconfigured collision grid: origin is %s, radius is %g, cell "
                    "dimensions are (%g, %g, %g), grid dimensions are %s"),
               *origin.ToString(),
               radius,
               cell_dims.X,
               cell_dims.Y,
               cell_dims.Z,
               *ml::ioj::CollisionUniformGrid::to_string(grid_dims));
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
void FSpatialQueryManager::reserve_thread_buffers(int32 const count) {
    auto const hardware_thread_count{
        FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads())};
    auto const maximum_thread_buffer_count{hardware_thread_count * 2};
    checkf(count <= maximum_thread_buffer_count,
           TEXT("Thread buffer count %d exceeds the maximum of %d"),
           count,
           maximum_thread_buffer_count);

    auto const result{thread_buffer_pool_.reserve(count)};
    checkf(result != simulation::QueryThreadBufferReserveResult::invalid_count,
           TEXT("Thread buffer count must be positive"));
    checkf(result != simulation::QueryThreadBufferReserveResult::active_queries,
           TEXT("Thread buffers cannot be grown while queries are active"));
}

auto FSpatialQueryManager::acquire_thread_buffer() const -> int32 {
    auto const index{thread_buffer_pool_.try_acquire()};
    if (!index.has_value()) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("FSpatialQueryManager thread buffer pool exhausted. Reserve enough buffers "
                    "before starting concurrent queries."));
    }

    return *index;
}

void FSpatialQueryManager::release_thread_buffer(int32 const index) const {
    check(thread_buffer_pool_.release(index));
}

/* **************************************** */
// Construction and setup
/* **************************************** */
FSpatialQueryManager::FSpatialQueryManager(FTestEntityRegistry const& in_entity_registry)
    : entity_registry{in_entity_registry}
    , collision{in_entity_registry} {}

void FSpatialQueryManager::initialise(FIntVector3 const grid_dimensions,
                                      FVector3f const cell_size,
                                      ioj::FEntityAABBs const& entity_bounds) {
    reserve_thread_buffers(1);

    auto& uniform_grid{collision.get_uniform_grid()};
    uniform_grid.set_grid_dims(grid_dimensions);
    uniform_grid.set_cell_dims(cell_size);

    collision.initialise(entity_bounds);
}

/* **************************************** */
// Batched line queries
/* **************************************** */
void FSpatialQueryManager::trace_line_of_sight(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::trace_line_of_sight);

    trace_impl<EQueryMode::HitEntity>(*this,
                                      {.start_locations = start_locations,
                                       .end_locations = end_locations,
                                       .out_entity_handles = out_entity_handles});
}

void FSpatialQueryManager::has_line_of_sight_to_targets(
    FVector3f const& start_location,
    FVectors3f::ConstView const end_locations,
    TConstArrayView<FRegistryEntityHandle> const targets,
    TArrayView<uint8> const has_los) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::has_line_of_sight_to_targets);

    trace_impl<EQueryMode::TargetLineOfSight>(*this,
                                              {.end_locations = end_locations,
                                               .scalar_start = start_location,
                                               .targets = targets,
                                               .out_flags = has_los});
}

void FSpatialQueryManager::have_clear_lines(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    TArrayView<uint8> const clear_lines,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities) const {
    trace_impl<EQueryMode::ClearLine>(*this,
                                      {.start_locations = start_locations,
                                       .end_locations = end_locations,
                                       .ignored_entities = ignored_entities,
                                       .out_flags = clear_lines});
}

void FSpatialQueryManager::trace_closest_lines(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    FTraceHitsView const out_hits,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::trace_closest_lines);

    auto const count{start_locations.num()};
    check(end_locations.num() == count);
    check(out_hits.num() == count);
    check(ignored_entities.IsEmpty() || ignored_entities.Num() == count);

    auto const traces{make_line_traces_const_view(start_locations, end_locations)};
    if (ignored_entities.IsEmpty()) {
        collision.get_uniform_grid().trace_aabbs(traces, out_hits);
    } else {
        collision.get_uniform_grid().trace_aabbs(traces, out_hits, ignored_entities);
    }
}

void FSpatialQueryManager::sweep_closest_aabbs(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    FVector3f const moving_half_extent,
    FTraceHitsView const out_hits,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities,
    ioj::ETraceEntityFilter const entity_filter) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::sweep_closest_aabbs);

    auto const count{start_locations.num()};
    check(end_locations.num() == count);
    check(out_hits.num() == count);
    check(ignored_entities.IsEmpty() || ignored_entities.Num() == count);

    collision.get_uniform_grid().sweep_aabbs(
        make_line_traces_const_view(start_locations, end_locations),
        moving_half_extent,
        out_hits,
        ignored_entities,
        entity_filter);
}

/* **************************************** */
// Scalar and entity queries
/* **************************************** */
auto FSpatialQueryManager::has_clear_line(FVector3f const start_location,
                                          FVector3f const end_location,
                                          FRegistryEntityHandle const ignored_entity) const
    -> bool {
    return !trace_closest(start_location, end_location, ignored_entity).hit;
}

auto FSpatialQueryManager::trace_closest(FVector3f const start_location,
                                         FVector3f const end_location,
                                         FRegistryEntityHandle const ignored_entity) const
    -> FLineTraceResult {
    TStaticArray<FRegistryEntityHandle, 1> ignored_entities{ignored_entity};
    return trace_impl<EQueryMode::ClosestHit>(*this,
                                              {.scalar_start = start_location,
                                               .scalar_end = end_location,
                                               .ignored_entities = ignored_entities});
}

auto FSpatialQueryManager::collect_non_team_entities_in_range(
    FVector3f const& origin,
    ETestTeam const team,
    float const radius,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::FSpatialQueryManager::collect_non_team_entities_in_range);

    telemetry_.record_range_query();

    if (out_entities.IsEmpty()) {
        return 0;
    }

    auto const& grid{collision.get_uniform_grid()};
    validate_grid_for_range_query(grid, origin, radius);
    query_manager::FThreadBufferLease const buffer_lease{*this};
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::FSpatialQueryManager::collect_non_team_entities_in_range::loop);
    return simulation::collect_non_team_entities_in_range(
        grid.get_native_geometry(),
        grid.get_native_entity_storage(),
        make_native_query_view(entity_registry),
        buffer_lease.get(),
        to_native(origin),
        radius,
        to_native(team),
        {out_entities.GetData(), static_cast<std::size_t>(out_entities.Num())});
}

auto FSpatialQueryManager::collect_entities_of_type_in_range(
    FVector3f const& origin,
    ETestEntityType const entity_type,
    float const radius,
    FRegistryEntityHandle const ignored_entity,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::collect_entities_of_type_in_range);

    telemetry_.record_range_query();

    if (out_entities.IsEmpty()) {
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
        to_native(origin),
        radius,
        to_native(entity_type),
        ignored_entity,
        {out_entities.GetData(), static_cast<std::size_t>(out_entities.Num())});
}

auto FSpatialQueryManager::get_any_non_team_entity(ETestTeam const team) const
    -> FRegistryEntityHandle {
    return simulation::find_any_non_team_entity(make_native_query_view(entity_registry),
                                                to_native(team));
}

auto FSpatialQueryManager::get_any_non_team_entity(ETestTeam const team,
                                                   ETestEntityType const entity_type) const
    -> FRegistryEntityHandle {
    return simulation::find_any_non_team_entity(
        make_native_query_view(entity_registry), to_native(team), to_native(entity_type));
}

void FSpatialQueryManager::are_spheres_in_bounds(FVectors3f::ConstView const centres,
                                                 float const radius,
                                                 TArrayView<uint8> const out_results) const {
    collision.get_uniform_grid().are_spheres_in_bounds(centres, radius, out_results);
}

/* **************************************** */
// Collision state and telemetry
/* **************************************** */
auto FSpatialQueryManager::update(uint64 const tick) -> ioj::FDetectedOverlapsView {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::update);

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
