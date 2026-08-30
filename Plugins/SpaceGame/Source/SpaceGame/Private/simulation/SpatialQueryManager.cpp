#include "SpaceGame/simulation/SpatialQueryManager.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/support/mesh.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/World.h>
#include <HAL/PlatformMisc.h>

#include <functional>
#include <mutex>
#include <utility>

namespace ml {
namespace {
// Spatial query hits are ordered by component pointer ascending according to std::less.
auto spatial_query_component_less(UPrimitiveComponent const* const lhs,
                                  UPrimitiveComponent const* const rhs) -> bool {
    return std::less<UPrimitiveComponent const*>{}(lhs, rhs);
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
FSpatialQueryManager::FSpatialQueryManager(FTestEntityRegistry const& in_entity_registry) noexcept
    : entity_registry{&in_entity_registry}
    , collision{in_entity_registry} {}

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

void FSpatialQueryManager::initialise(UWorld& in_world,
                                      ATestSpaceShip const* const player_ship,
                                      ATestCapitalShips const& capital_ships,
                                      ATestCapitalShipFighters const& capital_ship_fighters,
                                      ATestStaticTurrets const& static_turrets,
                                      ATestTubeSpinners const& tube_spinners) {
    reserve_thread_buffers(1);

    world = &in_world;
    player_ship_access = FTestSpaceShipSpatialQueryAccess{player_ship};
    capital_ships_access = FTestCapitalShipsSpatialQueryAccess{&capital_ships};
    capital_ship_fighters_access =
        FTestCapitalShipFightersSpatialQueryAccess{&capital_ship_fighters};
    static_turrets_access = FTestStaticTurretsSpatialQueryAccess{&static_turrets};
    tube_spinners_access = FTestTubeSpinnersSpatialQueryAccess{&tube_spinners};

    auto const* const capital_ship_instances{capital_ships_access.get_spatial_query_component()};
    auto const* const capital_ship_fighter_instances{
        capital_ship_fighters_access.get_spatial_query_component()};
    auto const* const static_turret_instances{static_turrets_access.get_spatial_query_component()};
    auto const* const tube_spinner_instances{tube_spinners_access.get_spatial_query_component()};
    auto const* const player_ship_mesh{
        player_ship ? player_ship_access.get_spatial_query_component() : nullptr};

    fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_instances),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighter_instances),
        SANDBOX_NAMED_UOBJECT_PTR(static_turret_instances),
        SANDBOX_NAMED_UOBJECT_PTR(tube_spinner_instances),
    });

    component_resolvers.Reset();
    component_resolvers.Reserve(std::to_underlying(EHitResolverKind::Count));

    if (player_ship) {
        fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(player_ship_mesh)});
        component_resolvers.Add({player_ship_mesh, EHitResolverKind::PlayerShipMesh});
    }

    component_resolvers.Add({capital_ship_instances, EHitResolverKind::CapitalShipInstances});
    component_resolvers.Add(
        {capital_ship_fighter_instances, EHitResolverKind::CapitalShipFighterInstances});
    component_resolvers.Add({static_turret_instances, EHitResolverKind::StaticTurretInstances});
    component_resolvers.Add({tube_spinner_instances, EHitResolverKind::TubeSpinnerInstances});

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

auto FSpatialQueryManager::classify_component(UPrimitiveComponent const* const component) const
    -> EHitResolverKind {
    for (auto const& resolver : component_resolvers) {
        if (resolver.component == component) {
            return resolver.kind;
        }
    }

    return EHitResolverKind::Unknown;
}

void FSpatialQueryManager::resolve_hits(
    TConstArrayView<FSpatialQueryHit> const hits,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    check(hits.Num() == out_entity_handles.Num());
    check(component_resolvers.Num() >= (std::to_underlying(EHitResolverKind::Count) - 1));

    for (auto& handle : out_entity_handles) {
        handle = FRegistryEntityHandle{};
    }

    auto const n{hits.Num()};
    if (n == 0) {
        return;
    }

    struct FResolverSpan {
        int32 offset{};
        int32 count{};
    };
    TStaticArray<FResolverSpan, std::to_underlying(EHitResolverKind::Count)> resolver_spans{};

    auto previous_kind{EHitResolverKind::Unknown};
    auto const* previous_component{static_cast<UPrimitiveComponent const*>(nullptr)};
    for (int32 i{}; i < n; ++i) {
        auto const& hit{hits[i]};
        if (hit.component != previous_component) {
            if (i > 0) {
                check(spatial_query_component_less(previous_component, hit.component));
            }
            previous_component = hit.component;
            previous_kind = classify_component(hit.component);
        }

        if (previous_kind == EHitResolverKind::Unknown) {
            continue;
        }

        auto& span{resolver_spans[std::to_underlying(previous_kind)]};
        if (span.count == 0) {
            span.offset = i;
        } else {
            check(span.offset + span.count == i);
        }
        ++span.count;
    }

    for (int32 i{}; i < std::to_underlying(EHitResolverKind::Count); ++i) {
        auto const& span{resolver_spans[i]};
        if (span.count == 0) {
            continue;
        }

        auto const resolver_kind{static_cast<EHitResolverKind>(i)};
        auto const hit_slice{hits.Slice(span.offset, span.count)};
        auto const handle_slice{out_entity_handles.Slice(span.offset, span.count)};

        switch (resolver_kind) {
            case EHitResolverKind::PlayerShipMesh:
                player_ship_access.resolve_hits(hit_slice, handle_slice);
                break;
            case EHitResolverKind::CapitalShipInstances:
                capital_ships_access.resolve_hits(hit_slice, handle_slice);
                break;
            case EHitResolverKind::CapitalShipFighterInstances:
                capital_ship_fighters_access.resolve_hits(hit_slice, handle_slice);
                break;
            case EHitResolverKind::StaticTurretInstances:
                static_turrets_access.resolve_hits(hit_slice, handle_slice);
                break;
            case EHitResolverKind::TubeSpinnerInstances:
                tube_spinners_access.resolve_hits(hit_slice, handle_slice);
                break;
            case EHitResolverKind::Unknown:
            case EHitResolverKind::Count:
                break;
        }
    }
}

void FSpatialQueryManager::trace_line_of_sight(
    FVectors3f::ConstView const start_locations,
    FVectors3f::ConstView const end_locations,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::trace_line_of_sight);

    auto const n{start_locations.num()};
    check(n == end_locations.num());
    check(n == out_entity_handles.Num());
    check(IsValid(world));

    ml::fill(out_entity_handles, FRegistryEntityHandle{});

    if (n == 0) {
        return;
    }

    query_manager::FThreadBufferLease const buffer_lease{*this};
    auto& buffers{buffer_lease.get()};
    auto& line_of_sight_hits{buffers.line_of_sight_hits};
    auto& sorted_line_of_sight_hits{buffers.sorted_line_of_sight_hits};
    auto& sorted_line_of_sight_entity_handles{buffers.sorted_line_of_sight_entity_handles};
    auto& line_of_sight_sort_indices{buffers.line_of_sight_sort_indices};

    line_of_sight_hits.SetNumUninitialized(n, EAllowShrinking::No);

    FHitResult hit_result{};
    for (int32 i{}; i < n; ++i) {
        hit_result.Reset();

        auto const did_hit{world->LineTraceSingleByChannel(hit_result,
                                                           ml::get_vector3d(start_locations, i),
                                                           ml::get_vector3d(end_locations, i),
                                                           ECC_Visibility)};
        line_of_sight_hits[i] = did_hit
                                  ? FSpatialQueryHit{hit_result.GetComponent(), hit_result.Item}
                                  : FSpatialQueryHit{};
    }

    resolve_line_of_sight_hits(buffers, n);

    for (int32 i{}; i < n; ++i) {
        out_entity_handles[line_of_sight_sort_indices[i]] = sorted_line_of_sight_entity_handles[i];
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
    check(IsValid(world));

    ml::fill(has_los, uint8{0});
    if (n == 0) {
        return;
    }

    query_manager::FThreadBufferLease const buffer_lease{*this};
    auto& buffers{buffer_lease.get()};
    auto& line_of_sight_hits{buffers.line_of_sight_hits};
    auto const trace_start{FVector{start_location}};

    line_of_sight_hits.SetNumUninitialized(n, EAllowShrinking::No);

    FHitResult hit_result{};
    for (int32 i{}; i < n; ++i) {
        hit_result.Reset();

        auto const did_hit{world->LineTraceSingleByChannel(
            hit_result, trace_start, ml::get_vector3d(end_locations, i), ECC_Visibility)};
        line_of_sight_hits[i] = did_hit
                                  ? FSpatialQueryHit{hit_result.GetComponent(), hit_result.Item}
                                  : FSpatialQueryHit{};
    }

    resolve_line_of_sight_hits(buffers, n);

    auto const& sorted_entity_handles{buffers.sorted_line_of_sight_entity_handles};
    auto const& sort_indices{buffers.line_of_sight_sort_indices};
    for (int32 sorted_i{}; sorted_i < n; ++sorted_i) {
        auto const input_i{sort_indices[sorted_i]};
        auto const did_hit{line_of_sight_hits[input_i].component != nullptr};
        has_los[input_i] =
            static_cast<uint8>(!did_hit || (sorted_entity_handles[sorted_i] == targets[input_i]));
    }
}

void FSpatialQueryManager::resolve_line_of_sight_hits(query_manager::FThreadBuffers& buffers,
                                                      int32 const count) const {
    auto& line_of_sight_hits{buffers.line_of_sight_hits};
    auto& sorted_line_of_sight_hits{buffers.sorted_line_of_sight_hits};
    auto& sorted_line_of_sight_entity_handles{buffers.sorted_line_of_sight_entity_handles};
    auto& line_of_sight_sort_indices{buffers.line_of_sight_sort_indices};

    check(line_of_sight_hits.Num() == count);

    sorted_line_of_sight_hits.SetNumUninitialized(count, EAllowShrinking::No);
    sorted_line_of_sight_entity_handles.SetNumUninitialized(count, EAllowShrinking::No);
    line_of_sight_sort_indices.SetNumUninitialized(count, EAllowShrinking::No);

    ml::fill_indices(line_of_sight_sort_indices);
    line_of_sight_sort_indices.Sort([&line_of_sight_hits](int32 const lhs, int32 const rhs) {
        return spatial_query_component_less(line_of_sight_hits[lhs].component,
                                            line_of_sight_hits[rhs].component);
    });

    for (int32 i{}; i < count; ++i) {
        sorted_line_of_sight_hits[i] = line_of_sight_hits[line_of_sight_sort_indices[i]];
    }

    resolve_hits(sorted_line_of_sight_hits, sorted_line_of_sight_entity_handles);
}

auto FSpatialQueryManager::collect_non_team_entities_in_range(
    FVector3f const& origin,
    ETestTeam const team,
    float const radius,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::FSpatialQueryManager::collect_non_team_entities_in_range);

    auto const& entity_data{entity_registry->get_entity_data()};
    auto const generations{entity_registry->get_generations()};
    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{entity_registry->get_num_elements()};
    auto const n_out_limit{out_entities.Num()};
    if (n_out_limit == 0) {
        return 0;
    }

    auto const ox{origin.X};
    auto const oy{origin.Y};
    auto const oz{origin.Z};

    for (int32 i{0}; i < n; ++i) {
        if (!entity_data.alive[i]) {
            continue;
        }
        if (entity_data.teams[i] == team) {
            continue;
        }

        auto const dist_sq{ml::dist_sq(entity_data.locations, i, ox, oy, oz)};
        if (dist_sq > radius_squared) {
            continue;
        }
        out_entities[count++] = FRegistryEntityHandle{i, generations[i]};

        if (count >= n_out_limit) {
            break;
        }
    }

    return count;
}

void FSpatialQueryManager::are_entities_within_dist_sq(float const dist_sq_threshold,
                                                       FVectors3f const& locations,
                                                       TArrayView<bool> const results) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FSpatialQueryManager::are_entities_within_dist_sq);

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(results),
    });
    ml::fill(results, false);

    auto const& entity_data{entity_registry->get_entity_data()};
    auto const n_inputs{ml::num(locations)};
    auto const n_entities{ml::num(entity_data.locations)};

    for (int32 input_i{0}; input_i < n_inputs; ++input_i) {
        float const x{locations.xs[input_i]};
        float const y{locations.ys[input_i]};
        float const z{locations.zs[input_i]};

        for (int32 entity_i{0}; entity_i < n_entities; ++entity_i) {
            if (!entity_data.alive[entity_i]) {
                continue;
            }

            float const dist_sq{ml::dist_sq(entity_data.locations, entity_i, x, y, z)};
            if (dist_sq <= dist_sq_threshold) {
                results[input_i] = true;
                break;
            }
        }
    }
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
