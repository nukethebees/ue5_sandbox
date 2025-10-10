#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "PawnWeaponComponent.generated.h"

class UWeaponComponent;
class AWeaponBase;

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

    void set_spawn_parameters(FActorSpawnParameters new_value) { spawn_parameters = new_value; }
    auto get_spawn_parameters() const { return spawn_parameters; }

    void set_spawn_transform(FTransform new_value) { spawn_transform = new_value; }
    auto get_spawn_transform() const { return spawn_transform; }
  protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapons")
    AWeaponBase* active_weapon{nullptr};

    FActorSpawnParameters spawn_parameters;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapons")
    FTransform spawn_transform{};
};
