#include "SpatialQueryManager.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <functional>
#include <utility>

namespace ml {
FSpatialQueryManager::FSpatialQueryManager(FTestEntityRegistry const& in_entity_registry) noexcept
    : entity_registry{&in_entity_registry} {}

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
}
