#include "SpatialQueryManager.h"

#include <SandboxCoreEngine/uobject_utils.h>

#include <functional>
#include <utility>

namespace ml {
void FSpatialQueryManager::initialise(ATestSpaceShip const* const player_ship,
                                      ATestCapitalShips const& capital_ships,
                                      ATestCapitalShipFighters const& capital_ship_fighters,
                                      ATestStaticTurrets const& static_turrets,
                                      ATestTubeSpinners const& tube_spinners) {
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
    std::less<void const*> const pointer_less{};

    for (int32 i{}; i < n; ++i) {
        auto const& hit{hits[i]};
        if (hit.component != previous_component) {
            if (i > 0) {
                check(pointer_less(previous_component, hit.component));
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
}
