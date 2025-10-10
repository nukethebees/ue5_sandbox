#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

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
    UPawnWeaponComponent() = default;

    bool can_fire() const;
    void start_firing();
    void sustain_firing(float delta_time);
    void stop_firing();

    void reload();
    bool can_reload() const;

    void equip_weapon(AWeaponBase* weapon);
  protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapons")
    AWeaponBase* active_weapon{nullptr};
};
