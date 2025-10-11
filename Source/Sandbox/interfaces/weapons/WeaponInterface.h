// IWeaponInterface.h
#pragma once

#include "CoreMinimal.h"

#include "UObject/Interface.h"

#include "Sandbox/data/weapons/AmmoReloadResult.h"
#include "Sandbox/enums/AmmoType.h"
#include "Sandbox/typedefs/Ammo.h"

#include "WeaponInterface.generated.h"

class UStaticMesh;

UINTERFACE(BlueprintType)
class UWeaponInterface : public UInterface {
    GENERATED_BODY()
};

class IWeaponInterface {
    GENERATED_BODY()
  public:
    UFUNCTION()
    virtual void start_firing() = 0;
    UFUNCTION()
    virtual void sustain_firing(float DeltaTime) = 0;
    UFUNCTION()
    virtual void stop_firing() = 0;

    UFUNCTION()
    virtual FAmmoReloadResult reload(FAmmo ammo_offered) = 0;
    UFUNCTION()
    virtual bool can_reload() const = 0;

    UFUNCTION()
    virtual bool can_fire() const = 0;

    UFUNCTION()
    virtual EAmmoType get_ammo_type() const = 0;
    UFUNCTION()
    virtual FAmmo get_current_ammo() const = 0;
    UFUNCTION()
    virtual FAmmo get_max_ammo() const = 0;

    UFUNCTION()
    virtual UStaticMesh* get_display_mesh() const = 0;
};
