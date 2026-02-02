#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/ammo/AmmoReloadResult.h"
#include "Sandbox/combat/pawn_weapon_component/PawnWeaponComponentDelegates.h"
#include "Sandbox/combat/weapons/WeaponPickupPayload.h"
#include "Sandbox/health/HealthChange.h"
#include "Sandbox/inventory/InventoryItem.h"
#include "Sandbox/inventory/ItemType.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "WeaponBase.generated.h"

class UTexture2D;
class UBoxComponent;

UCLASS(Abstract)
class SANDBOX_API AWeaponBase
    : public AActor
    , public IInventoryItem
    , public ml::LogMsgMixin<"AWeaponBase", LogSandboxWeapon> {
    GENERATED_BODY()
  public:
    AWeaponBase();

    void set_pickup_collision(bool enabled);

    UFUNCTION()
    virtual bool can_fire() const { return false; };
    UFUNCTION()
    virtual void start_firing() {};
    UFUNCTION()
    virtual void sustain_firing(float delta_time) {};
    UFUNCTION()
    virtual void stop_firing() {};

    UFUNCTION()
    virtual FAmmoReloadResult reload(FAmmoData const& ammo_offered);
    UFUNCTION()
    virtual bool can_reload() const { return false; };
    UFUNCTION()
    FAmmoData get_ammo_needed_for_reload() const { return FAmmoData{ammo_type, max_ammo - ammo}; };

    UFUNCTION()
    EAmmoType get_ammo_type() const { return ammo_type; };
    UFUNCTION()
    FAmmoData get_current_ammo() const { return FAmmoData{ammo_type, ammo}; };
    UFUNCTION()
    FAmmoData get_max_ammo() const { return FAmmoData{ammo_type, max_ammo}; };

    UFUNCTION()
    virtual UStaticMesh* get_display_mesh() const { return nullptr; };

    // IInventoryItem
    UFUNCTION()
    virtual FDimensions get_size() const override { return size; };
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    EAmmoType ammo_type{EAmmoType::Bullets};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    int32 max_ammo{10};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    int32 ammo{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    FDimensions size{1000000, 1000000};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    FHealthChange projectile_damage{10.0f, EHealthChangeType::Damage};
};
