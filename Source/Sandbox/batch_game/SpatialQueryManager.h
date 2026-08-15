#pragma once

#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>

#include <Containers/ArrayView.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

#include <utility>

class UPrimitiveComponent;

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
    void initialise(ATestSpaceShip const* player_ship,
                    ATestCapitalShips const& capital_ships,
                    ATestCapitalShipFighters const& capital_ship_fighters,
                    ATestStaticTurrets const& static_turrets,
                    ATestTubeSpinners const& tube_spinners);

    void resolve_hits(TConstArrayView<FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;
  private:
    struct FComponentResolver {
        UPrimitiveComponent const* component{nullptr};
        EHitResolverKind kind{EHitResolverKind::Unknown};
    };

    auto classify_component(UPrimitiveComponent const* component) const -> EHitResolverKind;

    using ComponentResolvers =
        TArray<FComponentResolver, TInlineAllocator<std::to_underlying(EHitResolverKind::Count)>>;
    ComponentResolvers component_resolvers;
    FTestSpaceShipSpatialQueryAccess player_ship_access;
    FTestCapitalShipsSpatialQueryAccess capital_ships_access;
    FTestCapitalShipFightersSpatialQueryAccess capital_ship_fighters_access;
    FTestStaticTurretsSpatialQueryAccess static_turrets_access;
    FTestTubeSpinnersSpatialQueryAccess tube_spinners_access;
};
}
