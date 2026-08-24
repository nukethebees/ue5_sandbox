#pragma once

#include "SpaceGame/ships/player/legacy/ShipAttackResult.h"

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ShipScoringSubsystem.generated.h"

class ASpaceShip;

UCLASS()
class SPACEGAME_API UShipScoringSubsystem : public UWorldSubsystem {
    GENERATED_BODY()
  public:
    void register_kills(FShipAttackResult attack);
    void register_points(ASpaceShip& ship, int32 pts);
};
