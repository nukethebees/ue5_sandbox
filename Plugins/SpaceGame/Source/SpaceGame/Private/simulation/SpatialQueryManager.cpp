#include "SpaceGame/simulation/SpatialQueryManager.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <HAL/PlatformMisc.h>
#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <mutex>
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
    traces.set_num(count, EAllowShrinking::No);
    hits.set_num(count, EAllowShrinking::No);

    for (int32 i{}; i < count; ++i) {
        if constexpr (Mode == EQueryMode::TargetLineOfSight) {
            traces.starts.set(i, request.scalar_start);
        } else if constexpr (Mode == EQueryMode::ClosestHit) {
            traces.starts.set(i, request.scalar_start);
        } else {
            traces.starts.set(i, ml::get_vector3f(request.start_locations, i));
        }

        if constexpr (Mode == EQueryMode::ClosestHit) {
            traces.ends.set(i, request.scalar_end);
        } else {
            traces.ends.set(i, ml::get_vector3f(request.end_locations, i));
        }
    }

    manager.get_collision_system().get_uniform_grid().trace_aabbs(
        traces.get_const_view(), hits.get_view(), request.ignored_entities);

    if constexpr (Mode == EQueryMode::ClosestHit) {
        return {
            .location = ml::get_vector3f(hits.locations, 0),
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
}

namespace ml::query_manager {
FThreadBufferLease::FThreadBufferLease(FSpatialQueryManager const& in_manager)
    : manager{&in_manager}
    , index{manager->acquire_thread_buffer()} {}

FThreadBufferLease::~FThreadBufferLease() {
    manager->release_thread_buffer(index);
}

auto FThreadBufferLease::get() const -> FThreadBuffers& {
    return manager->thread_buffers[index];
}
}

namespace ml {
void FSpatialQueryManager::reserve_thread_buffers(int32 const count) {
    auto const hardware_thread_count{
        FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads())};
    auto const maximum_thread_buffer_count{hardware_thread_count * 2};
    checkf(count > 0, TEXT("Thread buffer count must be positive"));
    checkf(count <= maximum_thread_buffer_count,
           TEXT("Thread buffer count %d exceeds the maximum of %d"),
           count,
           maximum_thread_buffer_count);

    std::lock_guard const lock{thread_buffers_mutex};
    if (count <= thread_buffers.Num()) {
        return;
    }

    checkf(active_thread_buffer_count == 0,
           TEXT("Thread buffers cannot be grown while queries are active"));

    auto const previous_count{thread_buffers.Num()};
    thread_buffers.AddDefaulted(count - previous_count);
    free_thread_buffer_indices.Reserve(count);
    for (int32 i{previous_count}; i < count; ++i) {
        free_thread_buffer_indices.Add(i);
    }
}

auto FSpatialQueryManager::acquire_thread_buffer() const -> int32 {
    std::lock_guard const lock{thread_buffers_mutex};
    if (free_thread_buffer_indices.IsEmpty()) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("FSpatialQueryManager thread buffer pool exhausted. Reserve enough buffers "
                    "before starting concurrent queries."));
    }

    ++active_thread_buffer_count;
    return free_thread_buffer_indices.Pop(EAllowShrinking::No);
}

void FSpatialQueryManager::release_thread_buffer(int32 const index) const {
    std::lock_guard const lock{thread_buffers_mutex};
    check(thread_buffers.IsValidIndex(index));
    check(!free_thread_buffer_indices.Contains(index));
    check(active_thread_buffer_count > 0);
    free_thread_buffer_indices.Add(index);
    --active_thread_buffer_count;
}

void FSpatialQueryManager::initialise(FTestEntityRegistry const& in_entity_registry,
                                      FIntVector3 const grid_dimensions,
                                      FVector3f const cell_size,
                                      ioj::FEntityAABBs const& entity_bounds) {
    reserve_thread_buffers(1);

    entity_registry = &in_entity_registry;
    collision.set_entity_registry(in_entity_registry);
    auto& uniform_grid{collision.get_uniform_grid()};
    uniform_grid.set_grid_dims(grid_dimensions);
    uniform_grid.set_cell_dims(cell_size);

    collision.initialise(entity_bounds);
}

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

    auto const n_out_limit{out_entities.Num()};
    if (n_out_limit == 0) {
        return 0;
    }

    auto const& grid{collision.get_uniform_grid()};
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
               *ioj::CollisionUniformGrid::to_string(grid_dims));
    }

    auto const abs_radius{FMath::Abs(radius)};
    FVector3f const radius_extent{abs_radius, abs_radius, abs_radius};
    auto const min_point{origin - radius_extent};
    auto const max_point{origin + radius_extent};
    auto [min_coord, max_coord]{grid.to_cell_coord_bounds(min_point, max_point)};
    auto const grid_dims{grid.get_grid_dims()};
    auto const max_grid_coord{grid_dims - FIntVector3{1, 1, 1}};
    if (max_coord.X < 0 || max_coord.Y < 0 || max_coord.Z < 0 || min_coord.X > max_grid_coord.X ||
        min_coord.Y > max_grid_coord.Y || min_coord.Z > max_grid_coord.Z) {
        return 0;
    }

    min_coord.X = FMath::Max(min_coord.X, 0);
    min_coord.Y = FMath::Max(min_coord.Y, 0);
    min_coord.Z = FMath::Max(min_coord.Z, 0);
    max_coord.X = FMath::Min(max_coord.X, max_grid_coord.X);
    max_coord.Y = FMath::Min(max_coord.Y, max_grid_coord.Y);
    max_coord.Z = FMath::Min(max_coord.Z, max_grid_coord.Z);

    query_manager::FThreadBufferLease const buffer_lease{*this};
    auto& seen_entities{buffer_lease.get().range_query_seen_entities};
    auto const entity_count{entity_registry->get_num_elements()};
    seen_entities.Init(false, entity_count);

    auto const& entity_data{entity_registry->get_entity_data()};
    auto const radius_squared{radius * radius};
    int32 count{};

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            Sandbox::FSpatialQueryManager::collect_non_team_entities_in_range::loop);
        for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
            for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                    for (auto const handle : grid.get_cell_entities({x, y, z})) {
                        if (!entity_registry->is_valid_alive(handle) ||
                            seen_entities[handle.index]) {
                            continue;
                        }
                        seen_entities[handle.index] = true;

                        if (entity_data.teams[handle.index] == team) {
                            continue;
                        }

                        auto const dist_sq{ml::dist_sq(
                            entity_data.locations, handle.index, origin.X, origin.Y, origin.Z)};
                        if (dist_sq > radius_squared) {
                            continue;
                        }

                        out_entities[count++] = handle;
                        if (count >= n_out_limit) {
                            return count;
                        }
                    }
                }
            }
        }
    }

    return count;
}

auto FSpatialQueryManager::get_any_non_team_entity(ETestTeam const team) const
    -> FRegistryEntityHandle {
    auto const& entity_data{entity_registry->get_entity_data()};
    auto const generations{entity_registry->get_generations()};
    auto const n{entity_registry->get_num_elements()};

    for (int32 i{0}; i < n; ++i) {
        if (!entity_data.alive[i]) {
            continue;
        }
        if (entity_data.teams[i] != team) {
            return {i, generations[i]};
        }
    }

    return {};
}

auto FSpatialQueryManager::get_any_non_team_entity(ETestTeam const team,
                                                   ETestEntityType const entity_type) const
    -> FRegistryEntityHandle {
    auto const& entity_data{entity_registry->get_entity_data()};
    auto const generations{entity_registry->get_generations()};
    auto const n{entity_registry->get_num_elements()};

    for (int32 i{0}; i < n; ++i) {
        if (!entity_data.alive[i]) {
            continue;
        }
        if (entity_data.teams[i] == team) {
            continue;
        }
        if (entity_data.entity_types[i] != entity_type) {
            continue;
        }

        return {i, generations[i]};
    }

    return {};
}

void FSpatialQueryManager::update() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::update);

    collision.update();
}
}
