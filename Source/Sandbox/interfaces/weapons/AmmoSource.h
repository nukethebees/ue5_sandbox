#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Sandbox/enums/AmmoType.h"
#include "Sandbox/typedefs/Ammo.h"

#include "AmmoSource.generated.h"

UINTERFACE(BlueprintType)
class UAmmoSource : public UInterface {
    GENERATED_BODY()
};

class IAmmoSource {
    GENERATED_BODY()
  public:
    virtual FAmmo get_current_ammo() const = 0;
    virtual FAmmo get_max_ammo() const = 0;

    virtual bool can_consume(FAmmo amount) const = 0;
    virtual void consume(FAmmo amount) = 0;

    virtual EAmmoType get_ammo_type() const = 0;
};
