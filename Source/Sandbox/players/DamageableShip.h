#pragma once

#include "Sandbox/players/ShipDamageResult.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "DamageableShip.generated.h"

class AActor;

struct ShipDamageContext {
    ShipDamageContext() = delete;
    ShipDamageContext(int32 damage, AActor const& instigator)
        : damage(damage)
        , instigator(instigator) {}

    int32 damage;
    AActor const& instigator;
};

UINTERFACE(MinimalAPI)
class UDamageableShip : public UInterface {
    GENERATED_BODY()
};

class IDamageableShip {
    GENERATED_BODY()
  public:
    virtual auto apply_damage(ShipDamageContext context) -> FShipDamageResult = 0;
};
