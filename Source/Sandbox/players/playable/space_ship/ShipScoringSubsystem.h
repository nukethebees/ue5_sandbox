#pragma once

#include "Sandbox/players/playable/space_ship/ShipAttackResult.h"

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ShipScoringSubsystem.generated.h"

UCLASS()
class SANDBOX_API UShipScoringSubsystem : public UWorldSubsystem {
    GENERATED_BODY()
  public:
    void register_kills(FShipAttackResult attack);
};
