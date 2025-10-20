#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/data/trigger/WeaponPickupPayload.h"
#include "Sandbox/interfaces/inventory/InventoryItem.h"
#include "Sandbox/interfaces/weapons/WeaponInterface.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "WeaponBase.generated.h"

class UBoxComponent;

UCLASS(Abstract)
class SANDBOX_API AWeaponBase
    : public AActor
    , public IWeaponInterface
    , public IInventoryItem
    , public ml::LogMsgMixin<"AWeaponBase", LogSandboxWeapon> {
    GENERATED_BODY()
  public:
    AWeaponBase();

    void set_pickup_collision(bool enabled);

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
    virtual FAmmoReloadResult reload(FAmmoData const& ammo_offered) override { return {}; };
    UFUNCTION()
    virtual bool can_reload() const override { return false; };

    UFUNCTION()
    virtual EAmmoType get_ammo_type() const override { return EAmmoType::Bullets; };
    UFUNCTION()
    virtual FAmmoData get_current_ammo() const override { return FAmmoData{}; };
    UFUNCTION()
    virtual FAmmoData get_max_ammo() const override { return FAmmoData{}; };

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
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    FWeaponPickupPayload trigger_payload;
};
