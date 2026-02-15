#pragma once

#include "Sandbox/players/common/ShipDamageResult.h"

#include "CoreMinimal.h"

#include "DamageableShip.generated.h"

class AActor;

UINTERFACE(MinimalAPI)
class UDamageableShip : public UInterface {
    GENERATED_BODY()
};

class IDamageableShip {
    GENERATED_BODY()
  public:
    virtual auto apply_damage(int32 damage, AActor const& instigator) -> FShipDamageResult = 0;
};
