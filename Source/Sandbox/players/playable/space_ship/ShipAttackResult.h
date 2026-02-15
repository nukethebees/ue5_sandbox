#pragma once

#include "Sandbox/combat/weapons/ShipProjectileType.h"

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "ShipAttackResult.generated.h"

class AActor;

USTRUCT()
struct FShipAttackResult {
    GENERATED_BODY()

    using Actors = TArray<TWeakObjectPtr<AActor>>;

    FShipAttackResult() = default;
    FShipAttackResult(TWeakObjectPtr<AActor> instigator,
                      EShipProjectileType projectile_type,
                      TArray<TWeakObjectPtr<AActor>> killed_actors)
        : instigator(instigator)
        , projectile_type(projectile_type)
        , killed_actors(killed_actors) {}

    TWeakObjectPtr<AActor> instigator{nullptr};
    EShipProjectileType projectile_type{EShipProjectileType::laser};
    TArray<TWeakObjectPtr<AActor>> killed_actors{};
};
