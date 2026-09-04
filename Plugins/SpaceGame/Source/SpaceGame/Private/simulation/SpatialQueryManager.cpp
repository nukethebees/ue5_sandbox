#include "SpaceGame/simulation/SpatialQueryManager.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/World.h>
#include <HAL/PlatformMisc.h>

#include <mutex>
#include <utility>

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

void FSpatialQueryManager::initialise_static_geometry(
    FCollisionGridConfig const& collision_grid_config) {
    check(IsValid(world));
    collision.initialise_static_geometry(*world, collision_grid_config);
}
auto FSpatialQueryManager::add_static_geometry(UPrimitiveComponent& component) -> bool {
    return collision.add_static_geometry(component);
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
                                      FCollisionGridConfig const& collision_grid_config,
                                      UWorld& in_world,
                                      ATestSpaceShip const* const player_ship,
                                      ATestCapitalShips const& capital_ships,
                                      ATestCapitalShipFighters const& capital_ship_fighters,
                                      ATestStaticTurrets const& static_turrets,
                                      ATestTubeSpinners const& tube_spinners) {
    reserve_thread_buffers(1);

    entity_registry = &in_entity_registry;
    world = &in_world;
    collision.set_entity_registry(in_entity_registry);
    auto& uniform_grid{collision.get_uniform_grid()};
    uniform_grid.set_grid_dims(collision_grid_config.calculate_grid_dimensions());
    uniform_grid.set_cell_dims(collision_grid_config.cell_size);

    auto const* const capital_ship_instances{
        FTestCapitalShipsSpatialQueryAccess{&capital_ships}.get_spatial_query_component()};
    auto const* const capital_ship_fighter_instances{
        FTestCapitalShipFightersSpatialQueryAccess{&capital_ship_fighters}
            .get_spatial_query_component()};
    auto const* const static_turret_instances{
        FTestStaticTurretsSpatialQueryAccess{&static_turrets}.get_spatial_query_component()};
    auto const* const tube_spinner_instances{
        FTestTubeSpinnersSpatialQueryAccess{&tube_spinners}.get_spatial_query_component()};
    auto const* const player_ship_mesh{
        player_ship ? FTestSpaceShipSpatialQueryAccess{player_ship}.get_spatial_query_component()
                    : nullptr};

    fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_instances),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighter_instances),
        SANDBOX_NAMED_UOBJECT_PTR(static_turret_instances),
        SANDBOX_NAMED_UOBJECT_PTR(tube_spinner_instances),
    });

    if (player_ship) {
        fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(player_ship_mesh)});
    }

    ioj::FCollisionSystem::EntityMeshes meshes{};
    meshes[std::to_underlying(ETestEntityType::PlayerShip)] = ml::get_static_mesh(player_ship_mesh);
    meshes[std::to_underlying(ETestEntityType::Turret)] =
        ml::get_static_mesh(static_turret_instances);
    meshes[std::to_underlying(ETestEntityType::CapitalShip)] =
        ml::get_static_mesh(capital_ship_instances);
    meshes[std::to_underlying(ETestEntityType::CapitalShipFighter)] =
        ml::get_static_mesh(capital_ship_fighter_instances);
    meshes[std::to_underlying(ETestEntityType::TubeSpinner)] =
        ml::get_static_mesh(tube_spinner_instances);
    collision.initialise(meshes);
}

void FSpatialQueryManager::trace_line_of_sight(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::trace_line_of_sight);

    auto const n{start_locations.num()};
    check(n == end_locations.num());
    check(n == out_entity_handles.Num());
    ml::fill(out_entity_handles, FRegistryEntityHandle{});

    if (n == 0) {
        return;
    }

    query_manager::FThreadBufferLease const buffer_lease{*this};
    auto& buffers{buffer_lease.get()};
    auto& traces{buffers.line_traces};
    auto& hits{buffers.trace_hits};
    traces.set_num(n, EAllowShrinking::No);
    hits.set_num(n, EAllowShrinking::No);
    for (int32 i{}; i < n; ++i) {
        traces.starts.set(i, ml::get_vector3f(start_locations, i));
        traces.ends.set(i, ml::get_vector3f(end_locations, i));
    }

    collision.get_uniform_grid().trace_aabbs(traces.get_const_view(), hits.get_view());
    for (int32 i{}; i < n; ++i) {
        out_entity_handles[i] = hits.entities[i];
    }
}

void FSpatialQueryManager::has_line_of_sight_to_targets(
    FVector3f const& start_location,
    FVectors3f::ConstView const end_locations,
    TConstArrayView<FRegistryEntityHandle> const targets,
    TArrayView<uint8> const has_los) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::has_line_of_sight_to_targets);

    auto const n{end_locations.num()};
    check(n == targets.Num());
    check(n == has_los.Num());
    ml::fill(has_los, uint8{0});
    if (n == 0) {
        return;
    }

    query_manager::FThreadBufferLease const buffer_lease{*this};
    auto& buffers{buffer_lease.get()};
    auto& traces{buffers.line_traces};
    auto& hits{buffers.trace_hits};
    traces.set_num(n, EAllowShrinking::No);
    hits.set_num(n, EAllowShrinking::No);
    for (int32 i{}; i < n; ++i) {
        traces.starts.set(i, start_location);
        traces.ends.set(i, ml::get_vector3f(end_locations, i));
    }

    collision.get_uniform_grid().trace_aabbs(traces.get_const_view(), hits.get_view());
    for (int32 i{}; i < n; ++i) {
        has_los[i] = static_cast<uint8>(hits.hits[i] == 0 || hits.entities[i] == targets[i]);
    }
}

void FSpatialQueryManager::have_clear_lines(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    TArrayView<uint8> const clear_lines,
    TConstArrayView<FRegistryEntityHandle> const ignored_entities) const {
    auto const n{start_locations.num()};
    check(n == end_locations.num());
    check(n == clear_lines.Num());
    check(ignored_entities.IsEmpty() || ignored_entities.Num() == n);

    if (n == 0) {
        return;
    }

    query_manager::FThreadBufferLease const buffer_lease{*this};
    auto& buffers{buffer_lease.get()};
    auto& traces{buffers.line_traces};
    auto& hits{buffers.trace_hits};
    traces.set_num(n, EAllowShrinking::No);
    hits.set_num(n, EAllowShrinking::No);
    for (int32 i{}; i < n; ++i) {
        traces.starts.set(i, ml::get_vector3f(start_locations, i));
        traces.ends.set(i, ml::get_vector3f(end_locations, i));
    }

    collision.get_uniform_grid().trace_aabbs(
        traces.get_const_view(), hits.get_view(), ignored_entities);
    for (int32 i{}; i < n; ++i) {
        clear_lines[i] = static_cast<uint8>(hits.hits[i] == 0);
    }
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
    FVectors3f starts;
    FVectors3f ends;
    starts.add(start_location);
    ends.add(end_location);
    FTraceHits hits;
    hits.add_uninitialised(1);
    TStaticArray<FRegistryEntityHandle, 1> ignored_entities{ignored_entity};
    collision.get_uniform_grid().trace_aabbs(
        FLineTracesConstView{starts.get_const_view(), ends.get_const_view()},
        hits.get_view(),
        ignored_entities);
    return {
        .location = ml::get_vector3f(hits.locations, 0),
        .entity = hits.entities[0],
        .static_geometry_index = hits.static_geometry_indices[0],
        .hit = hits.hits[0] != 0,
    };
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
