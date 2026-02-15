#pragma once

#include "CoreMinimal.h"

#include "ShipDamageResult.generated.h"

USTRUCT()
struct FShipDamageResult {
    GENERATED_BODY()

    FShipDamageResult() = default;

    auto was_killed() const { return was_killed_; }
  protected:
    bool was_killed_{false};
};
