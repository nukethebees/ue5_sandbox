// IWeaponInterface.h
#pragma once

#include "CoreMinimal.h"

#include "UObject/Interface.h"

#include "Sandbox/enums/AmmoType.h"
#include "Sandbox/typedefs/Ammo.h"

#include "WeaponInterface.generated.h"

UINTERFACE(BlueprintType)
class UWeaponInterface : public UInterface {
    GENERATED_BODY()
};

class IWeaponInterface {
    GENERATED_BODY()
  public:
    virtual void start_firing() = 0;
    virtual void sustain_firing(float DeltaTime) = 0;
    virtual void stop_firing() = 0;

    virtual void reload() = 0;
    virtual bool can_reload() const = 0;

    virtual bool can_fire() const = 0;

    virtual EAmmoType get_ammo_type() const = 0;
    virtual Ammo get_current_ammo() const = 0;
    virtual Ammo get_max_ammo() const = 0;
};
