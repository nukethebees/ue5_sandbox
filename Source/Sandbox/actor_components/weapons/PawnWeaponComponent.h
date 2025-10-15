#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

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

    void equip_weapon(AWeaponBase* weapon);
    void unequip_weapon();
    bool pickup_new_weapon(TSubclassOf<AWeaponBase> weapon_class);
    bool pickup_new_weapon(AWeaponBase& weapon);

    void set_spawn_parameters(FActorSpawnParameters new_value) { spawn_parameters = new_value; }
    auto get_spawn_parameters() const { return spawn_parameters; }

    void set_spawn_transform(FTransform new_value) { spawn_transform = new_value; }
    auto get_spawn_transform() const { return spawn_transform; }

    void set_attach_location(USceneComponent* new_value);
    auto get_attach_location() const { return attach_location; }
  private:
    bool pickup_new_weapon(AWeaponBase& weapon,
                           UInventoryComponent& inventory_component,
                           USceneComponent& location);
    AWeaponBase* spawn_weapon(TSubclassOf<AWeaponBase> weapon_class, AActor& owner, UWorld& world);
    void attach_weapon(AWeaponBase& weapon, USceneComponent& location);
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    AWeaponBase* active_weapon{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    USceneComponent* attach_location{nullptr};

    FActorSpawnParameters spawn_parameters;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
    FTransform spawn_transform{};
};
