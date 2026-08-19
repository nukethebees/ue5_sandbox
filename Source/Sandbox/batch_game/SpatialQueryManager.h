#pragma once

#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include <SandboxCore/soa_vectors.h>

#include <utility>

class UPrimitiveComponent;
class UWorld;
struct FTestEntityRegistry;

namespace ml {
struct SANDBOX_API FSpatialQueryManager {
  private:
    enum class EHitResolverKind : uint8 {
        PlayerShipMesh,
        CapitalShipInstances,
        CapitalShipFighterInstances,
        StaticTurretInstances,
        TubeSpinnerInstances,
        Count,
        Unknown,
    };
  public:
    // Required only for Unreal's default-construction path.
    // Pointer members are otherwise required to be non-null.
    // Explicitly default-constructing this type violates its contract.
    FSpatialQueryManager() = default;
    explicit FSpatialQueryManager(FTestEntityRegistry const& entity_registry) noexcept;

    void initialise(UWorld& world,
                    ATestSpaceShip const* player_ship,
                    ATestCapitalShips const& capital_ships,
                    ATestCapitalShipFighters const& capital_ship_fighters,
                    ATestStaticTurrets const& static_turrets,
                    ATestTubeSpinners const& tube_spinners);

    void resolve_hits(TConstArrayView<FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;
    void trace_line_of_sight(FVectors3f::ConstView start_locations,
                             FVectors3f::ConstView end_locations,
                             TArrayView<FRegistryEntityHandle> out_entity_handles) const;

    auto collect_non_team_entities_in_range(
        FVector3f const& origin,
        ETestTeam const team,
        float const radius,
        TArrayView<FRegistryEntityHandle> const out_entities) const -> int32;
    auto get_any_non_team_entity(ETestTeam const team) const -> FRegistryEntityHandle;
    auto get_any_non_team_entity(ETestTeam const team, ETestEntityType const entity_type) const
        -> FRegistryEntityHandle;
    void are_entities_within_dist_sq(float const dist_sq,
                                     FVectors3f const& locations,
                                     TArrayView<bool> const results) const;
  private:
    struct FComponentResolver {
        UPrimitiveComponent const* component{nullptr};
        EHitResolverKind kind{EHitResolverKind::Unknown};
    };

    auto classify_component(UPrimitiveComponent const* component) const -> EHitResolverKind;

    using ComponentResolvers =
        TArray<FComponentResolver, TInlineAllocator<std::to_underlying(EHitResolverKind::Count)>>;
    FTestEntityRegistry const* const entity_registry{nullptr};
    UWorld* world{nullptr};
    ComponentResolvers component_resolvers;
    mutable TArray<FSpatialQueryHit> line_of_sight_hits;
    mutable TArray<FSpatialQueryHit> sorted_line_of_sight_hits;
    mutable TArray<FRegistryEntityHandle> sorted_line_of_sight_entity_handles;
    mutable TArray<int32> line_of_sight_sort_indices;
    FTestSpaceShipSpatialQueryAccess player_ship_access;
    FTestCapitalShipsSpatialQueryAccess capital_ships_access;
    FTestCapitalShipFightersSpatialQueryAccess capital_ship_fighters_access;
    FTestStaticTurretsSpatialQueryAccess static_turrets_access;
    FTestTubeSpinnersSpatialQueryAccess tube_spinners_access;
};
}
