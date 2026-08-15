#include "SpatialQueryManager.h"

#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/HitResult.h>

#include <utility>

namespace ml {
void FSpatialQueryManager::initialise(ATestSpaceShip const* const player_ship,
                                      ATestCapitalShips const& capital_ships,
                                      ATestCapitalShipFighters const& capital_ship_fighters,
                                      ATestStaticTurrets const& static_turrets,
                                      ATestTubeSpinners const& tube_spinners) {
    initialised = false;
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

    fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_instances),
        SANDBOX_NAMED_UOBJECT_PTR(capital_ship_fighter_instances),
        SANDBOX_NAMED_UOBJECT_PTR(static_turret_instances),
        SANDBOX_NAMED_UOBJECT_PTR(tube_spinner_instances),
    });

    component_resolvers.Reset();
    component_resolvers.Reserve(std::to_underlying(EHitResolverKind::Count));

    if (player_ship) {
        auto const* const player_ship_mesh{player_ship_access.get_spatial_query_component()};
        fatal_if_uobject_ptrs_invalid({SANDBOX_NAMED_UOBJECT_PTR(player_ship_mesh)});
        component_resolvers.Add({player_ship_mesh, EHitResolverKind::PlayerShipMesh});
    }

    component_resolvers.Add({capital_ship_instances, EHitResolverKind::CapitalShipInstances});
    component_resolvers.Add(
        {capital_ship_fighter_instances, EHitResolverKind::CapitalShipFighterInstances});
    component_resolvers.Add({static_turret_instances, EHitResolverKind::StaticTurretInstances});
    component_resolvers.Add({tube_spinner_instances, EHitResolverKind::TubeSpinnerInstances});
    initialised = true;
}

auto FSpatialQueryManager::classify_hit(FHitResult const& hit) const -> EHitResolverKind {
    auto const* const component{hit.GetComponent()};
    for (auto const& resolver : component_resolvers) {
        if (resolver.component == component) {
            return resolver.kind;
        }
    }

    return EHitResolverKind::Unknown;
}

void FSpatialQueryManager::resolve_hits(
    TConstArrayView<FHitResult> const hits,
    TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
    check(hits.Num() == out_entity_handles.Num());
    check(initialised);

    for (auto& handle : out_entity_handles) {
        handle = FRegistryEntityHandle{};
    }

    auto const n{hits.Num()};
    if (n == 0) {
        return;
    }

    TArray<EHitResolverKind> resolver_kinds;
    TArray<int32> sorted_indices;
    resolver_kinds.Reserve(n);
    sorted_indices.Reserve(n);

    for (int32 i{}; i < n; ++i) {
        resolver_kinds.Add(classify_hit(hits[i]));
        sorted_indices.Add(i);
    }

    sorted_indices.Sort([&resolver_kinds](int32 const lhs_index, int32 const rhs_index) {
        auto const lhs_kind{resolver_kinds[lhs_index]};
        auto const rhs_kind{resolver_kinds[rhs_index]};
        return std::to_underlying(lhs_kind) < std::to_underlying(rhs_kind);
    });

    TArray<FHitResult> sorted_hits;
    TArray<EHitResolverKind> sorted_resolver_kinds;
    TArray<FRegistryEntityHandle> sorted_entity_handles;
    sorted_hits.Reserve(n);
    sorted_resolver_kinds.Reserve(n);
    sorted_entity_handles.AddDefaulted(n);

    for (int32 const sorted_index : sorted_indices) {
        sorted_hits.Add(hits[sorted_index]);
        sorted_resolver_kinds.Add(resolver_kinds[sorted_index]);
    }

    int32 index{};
    while (index < n) {
        auto const run_start{index};
        auto const resolver_kind{sorted_resolver_kinds[index]};

        do {
            ++index;
        } while ((index < n) && (sorted_resolver_kinds[index] == resolver_kind));

        auto const run_count{index - run_start};
        auto const hit_slice{TConstArrayView<FHitResult>{sorted_hits}.Slice(run_start, run_count)};
        auto const handle_slice{
            TArrayView<FRegistryEntityHandle>{sorted_entity_handles}.Slice(run_start, run_count)};

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
                break;
        }
    }

    for (int32 i{}; i < n; ++i) {
        out_entity_handles[sorted_indices[i]] = sorted_entity_handles[i];
    }
}
}
