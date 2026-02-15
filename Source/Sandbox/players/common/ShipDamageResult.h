#pragma once

#include "CoreMinimal.h"

#include "ShipDamageResult.generated.h"

USTRUCT()
struct FShipDamageResult {
    GENERATED_BODY()

    FShipDamageResult() = default;

    auto was_killed() const { return false; }
};
