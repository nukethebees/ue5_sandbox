#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Sandbox/combat/ammo/AmmoData.h"
#include "Sandbox/combat/ammo/AmmoType.h"

#include "AmmoSource.generated.h"

UINTERFACE(BlueprintType)
class UAmmoSource : public UInterface {
    GENERATED_BODY()
};

class IAmmoSource {
    GENERATED_BODY()
  public:
    virtual FAmmoData get_current_ammo() const = 0;
    virtual FAmmoData get_max_ammo() const = 0;

    virtual bool can_consume(FAmmoData amount) const = 0;
    virtual void consume(FAmmoData amount) = 0;

    virtual EAmmoType get_ammo_type() const = 0;
};
