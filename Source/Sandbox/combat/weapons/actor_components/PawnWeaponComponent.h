#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

#include "Sandbox/combat/weapons/delegates/OnAmmoChanged.h"
#include "Sandbox/combat/weapons/enums/WeaponPickupAction.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "PawnWeaponComponent.generated.h"

class USceneComponent;
class UWeaponComponent;
class AWeaponBase;
class UInventoryComponent;

UCLASS()
class SANDBOX_API UPawnWeaponComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UPawnWeaponComponent", LogSandboxWeapon> {
    GENERATED_BODY()
  public:
    UPawnWeaponComponent();

    virtual void BeginPlay() override;

    bool can_fire() const;
    void start_firing();
    void sustain_firing(float delta_time);
    void stop_firing();

    void reload();
    bool can_reload() const;

    void unequip_weapon();
    void cycle_next_weapon();
    void cycle_prev_weapon();

    void set_attach_location(USceneComponent& new_value) { attach_location = &new_value; }
    auto get_attach_location() const { return attach_location; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    EWeaponPickupAction weapon_pickup_action{EWeaponPickupAction::EquipIfNothingEquipped};

    FOnAmmoChanged on_weapon_ammo_changed;
  private:
    void equip_weapon(AWeaponBase& weapon);
    void attach_weapon(AWeaponBase& weapon, USceneComponent& location);
    void on_weapon_added(AWeaponBase& weapon);

    UFUNCTION()
    void on_active_weapon_ammo_changed(FAmmoData current_ammo);
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    AWeaponBase* active_weapon{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    USceneComponent* attach_location{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    UInventoryComponent* inventory;
};
