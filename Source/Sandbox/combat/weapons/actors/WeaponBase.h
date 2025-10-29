#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/weapons/data/WeaponPickupPayload.h"
#include "Sandbox/combat/weapons/delegates/OnAmmoChanged.h"
#include "Sandbox/combat/weapons/interfaces/WeaponInterface.h"
#include "Sandbox/inventory/enums/ItemType.h"
#include "Sandbox/inventory/interfaces/InventoryItem.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "WeaponBase.generated.h"

class UTexture2D;
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
    virtual AWeaponBase* get_weapon() override final { return this; };
    UFUNCTION()
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("WeaponBaseDefaultValue")};
        return default_name;
    };
    virtual UTexture2D* get_display_image() const override { return display_image; }
    virtual EItemType get_item_type() const override { return EItemType::Weapon; }

    // AWeaponBase
    UFUNCTION()
    void hide_weapon();
    UFUNCTION()
    void show_weapon();

    FOnAmmoChanged on_ammo_changed;
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    FWeaponPickupPayload trigger_payload;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UTexture2D* display_image{nullptr};
};
