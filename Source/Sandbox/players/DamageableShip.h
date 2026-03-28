#pragma once

#include "Sandbox/players/ShipDamageResult.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "DamageableShip.generated.h"

class AActor;
class UPrimitiveComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKilled, AActor*);

struct ShipDamageContext {
    ShipDamageContext() = delete;
    ShipDamageContext(int32 damage,
                      AActor const& instigator,
                      UPrimitiveComponent const& component_hit)
        : damage(damage)
        , instigator(instigator)
        , component_hit(component_hit) {}

    int32 damage;
    AActor const& instigator;
    UPrimitiveComponent const& component_hit;
};

UINTERFACE(MinimalAPI)
class UDamageableShip : public UInterface {
    GENERATED_BODY()
};

class IDamageableShip {
    GENERATED_BODY()
  public:
    virtual auto apply_damage(ShipDamageContext context) -> FShipDamageResult = 0;
    virtual auto get_on_killed_delegate() -> FOnKilled& = 0;
};
