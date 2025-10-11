#pragma once

#include "CoreMinimal.h"

#include "Sandbox/interfaces/inventory/InventoryItem.h"
#include "Sandbox/interfaces/weapons/WeaponInterface.h"

#include "WeaponBase.generated.h"

UCLASS(Abstract)
class SANDBOX_API AWeaponBase
    : public AActor
    , public IWeaponInterface
    , public IInventoryItem {
    GENERATED_BODY()
  public:
    // IWeaponInterface
    UFUNCTION()
    virtual bool can_fire() const override { return false; };
    UFUNCTION()
    virtual void start_firing() override {};
    UFUNCTION()
    virtual void sustain_firing(float delta_time) override {};
    UFUNCTION()
    virtual void stop_firing() override {};

    UFUNCTION()
    virtual FAmmoReloadResult reload(FAmmo ammo_offered) override { return {}; };
    UFUNCTION()
    virtual bool can_reload() const override { return false; };

    UFUNCTION()
    virtual EAmmoType get_ammo_type() const override { return EAmmoType::Bullets; };
    UFUNCTION()
    virtual FAmmo get_current_ammo() const override { return FAmmo{}; };
    UFUNCTION()
    virtual FAmmo get_max_ammo() const override { return FAmmo{}; };

    UFUNCTION()
    virtual UStaticMesh* get_display_mesh() const { return nullptr; };

    // IInventoryItem
    UFUNCTION()
    virtual FDimensions get_size() const override { return FDimensions{1000000, 1000000}; };
    virtual bool is_weapon() const override final { return true; };
    UFUNCTION()
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("WeaponBaseDefaultValue")};
        return default_name;
    };

    // AWeaponBase
    UFUNCTION()
    void hide_weapon();
    UFUNCTION()
    void show_weapon();
};
