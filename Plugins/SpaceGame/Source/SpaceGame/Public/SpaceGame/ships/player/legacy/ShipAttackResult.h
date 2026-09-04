#pragma once

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
                      TArray<TWeakObjectPtr<AActor>> killed_actors)
        : instigator(instigator)
        , killed_actors(killed_actors) {}

    TWeakObjectPtr<AActor> instigator{nullptr};
    TArray<TWeakObjectPtr<AActor>> killed_actors{};
};
